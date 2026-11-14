#include "cyd_link.h"
#include "cyd_peer.h"
#include "sdkconfig.h"
#ifdef CONFIG_CYD_LINK_ENABLE
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/task.h"
static cyd_link_t engine;
static SemaphoreHandle_t guard;
static bool running;
static uint32_t latency_ms;
static int last_result = -1;
static const char *startup = "Not started";
static cyd_link_role_t role(void) {
#ifdef CONFIG_IDF_TARGET_ESP32S3
    return CYD_LINK_MAIN;
#else
    return CYD_LINK_COMPANION;
#endif
}
/* This is a known-conflict guard, not a replacement for connector/module pinouts. */
static bool available(int pin, bool tx) {
    if (pin < 0 || !GPIO_IS_VALID_GPIO(pin) || (tx && !GPIO_IS_VALID_OUTPUT_GPIO(pin)))
        return false;
#ifdef CONFIG_IDF_TARGET_ESP32S3
    const int used[] = {0,  3,  4,  5,  6,  7,  9,  10, 11, 12, 13, 14, 15, 16,
                        17, 18, 19, 20, 21, 26, 27, 28, 29, 30, 31, 32, 33, 34,
                        35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48};
#else
    const int used[] = {1, 2, 6, 7, 10, 11, 12, 13, 14, 23, 24, 25, 27, 28};
#endif
    for (unsigned i = 0; i < sizeof(used) / sizeof(used[0]); i++)
        if (pin == used[i])
            return false;
    return true;
}
static void worker(void *unused) {
    (void)unused;
    uint8_t input[256], output[CYD_LINK_FRAME];
    size_t size = 0, offset = 0;
    uint64_t next_probe = 0, sent_at = 0;
    uint32_t probe = 0;
    bool was_ready = false;
    for (;;) {
        int got = uart_read_bytes(UART_NUM_1, input, sizeof(input), 0);
        uint64_t now = (uint64_t)esp_timer_get_time() / 1000;
        xSemaphoreTake(guard, portMAX_DELAY);
        cyd_link_tick(&engine, now);
        if (got > 0)
            cyd_link_feed(&engine, input, got);
        if (engine.ready && now >= next_probe && !probe) {
            probe = cyd_link_ping(&engine, "CYDOS", 5, 1000);
            if (probe) {
                sent_at = now;
                next_probe = now + 2000;
            }
        }
        cyd_link_event_t event;
        while (cyd_link_pop(&engine, &event))
            if (event.id == probe) {
                last_result = event.result;
                latency_ms = event.result == CYD_LINK_OK ? (uint32_t)(now - sent_at) : 0;
                probe = 0;
            }
        bool ready = engine.ready;
        if (offset == size) {
            offset = size = 0;
            cyd_link_take_tx(&engine, output, sizeof(output), &size);
        }
        xSemaphoreGive(guard);
        if (ready != was_ready) {
            cyd_log("peer", ready ? "Diagnostic peer connected (no service offload)"
                                  : "Diagnostic peer disconnected");
            was_ready = ready;
        }
        if (offset < size) {
            int written = uart_tx_chars(UART_NUM_1, (char *)output + offset, size - offset);
            if (written > 0)
                offset += written;
        }
        vTaskDelay(pdMS_TO_TICKS(1) ? pdMS_TO_TICKS(1) : 1);
    }
}
esp_err_t cyd_peer_start(void) {
    if (running)
        return ESP_ERR_INVALID_STATE;
#ifndef CONFIG_CYD_LINK_WIRING_CONFIRMED
    startup = "UART disabled: actual wiring is not confirmed";
    return ESP_ERR_INVALID_STATE;
#endif
    int tx = CONFIG_CYD_LINK_TX_GPIO, rx = CONFIG_CYD_LINK_RX_GPIO;
    if (tx == rx || !available(tx, true) || !available(rx, false)) {
        startup = "UART disabled: unset or conflicting GPIO";
        return ESP_ERR_INVALID_ARG;
    }
    uart_config_t config = {.baud_rate = CONFIG_CYD_LINK_BAUD,
                            .data_bits = UART_DATA_8_BITS,
                            .parity = UART_PARITY_DISABLE,
                            .stop_bits = UART_STOP_BITS_1,
                            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
                            .source_clk = UART_SCLK_DEFAULT};
    esp_err_t e = uart_param_config(UART_NUM_1, &config);
    if (!e)
        e = uart_set_pin(UART_NUM_1, tx, rx, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (e) {
        startup = "UART configuration failed";
        return e;
    }
    e = uart_driver_install(UART_NUM_1, 2048, 0, 0, NULL, 0);
    if (e) {
        startup = "UART driver allocation failed";
        return e;
    }
    guard = xSemaphoreCreateMutex();
    if (!guard) {
        uart_driver_delete(UART_NUM_1);
        startup = "Link mutex allocation failed";
        return ESP_ERR_NO_MEM;
    }
    uint64_t nonce;
    do {
        nonce = (uint64_t)esp_random() << 32 | esp_random();
    } while (!nonce);
    cyd_link_init(&engine, role(), nonce);
    if (xTaskCreate(worker, "peer_uart", 4096, NULL, 3, NULL) != pdPASS) {
        vSemaphoreDelete(guard);
        guard = NULL;
        uart_driver_delete(UART_NUM_1);
        startup = "Link task allocation failed";
        return ESP_ERR_NO_MEM;
    }
    running = true;
    startup = "Diagnostic link active; app/network routing not implemented";
    return ESP_OK;
}
cJSON *cyd_peer_status(void) {
    bool ready = false;
    unsigned errors = 0, drops = 0, resets = 0;
    uint32_t rtt = 0;
    int result = 0;
    if (running) {
        xSemaphoreTake(guard, portMAX_DELAY);
        ready = engine.ready;
        errors = engine.bad_frames;
        drops = engine.dropped_frames;
        resets = engine.peer_resets;
        rtt = latency_ms;
        result = last_result;
        xSemaphoreGive(guard);
    }
    cJSON *o = cJSON_CreateObject();
    if (!o)
        return NULL;
    cJSON_AddBoolToObject(o, "configured", true);
    cJSON_AddBoolToObject(o, "running", running);
    cJSON_AddStringToObject(
        o, "role", role() == CYD_LINK_MAIN ? "s3_application_host" : "c5_connectivity_companion");
    cJSON_AddBoolToObject(o, "connected", ready);
    cJSON_AddNumberToObject(o, "protocol", CYD_LINK_PROTOCOL);
    cJSON_AddStringToObject(o, "message", startup);
    cJSON_AddBoolToObject(o, "service_offload", false);
    cJSON_AddBoolToObject(o, "authenticated", false);
    cJSON_AddNumberToObject(o, "baud", CONFIG_CYD_LINK_BAUD);
    cJSON_AddNumberToObject(o, "bad_frames", errors);
    cJSON_AddNumberToObject(o, "dropped_frames", drops);
    cJSON_AddNumberToObject(o, "peer_resets", resets);
    cJSON_AddNumberToObject(o, "last_echo_result", result);
    cJSON_AddNumberToObject(o, "echo_latency_ms", rtt);
    return o;
}
#else
esp_err_t cyd_peer_start(void) { return ESP_OK; }
cJSON *cyd_peer_status(void) {
    cJSON *o = cJSON_CreateObject();
    if (o) {
        cJSON_AddBoolToObject(o, "configured", false);
        cJSON_AddBoolToObject(o, "connected", false);
        cJSON_AddBoolToObject(o, "service_offload", false);
        cJSON_AddStringToObject(o, "message", "Standalone; diagnostic UART link disabled");
    }
    return o;
}
#endif
