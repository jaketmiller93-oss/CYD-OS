#include "cyd_link.h"
#include "cyd_peer.h"
#include "sdkconfig.h"
#include <stdio.h>
#include <string.h>
static cyd_link_role_t role(void) {
#ifdef CONFIG_IDF_TARGET_ESP32S3
    return CYD_LINK_MAIN;
#else
    return CYD_LINK_COMPANION;
#endif
}
#ifdef CONFIG_CYD_LINK_ENABLE
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "freertos/task.h"
#include "freertos/queue.h"
/* The bounded protocol state is not DMA or an ISR buffer. Keep internal RAM
 * available for the web server and task stacks on the single-core C5. */
static cyd_link_t *engine;
static void release_engine(void) {
    heap_caps_free(engine);
    engine = NULL;
}
static SemaphoreHandle_t guard;
#ifdef CONFIG_CYD_PAIRED_SYSTEM
static SemaphoreHandle_t call_guard,call_done;
static QueueHandle_t requests;
static uint32_t call_id;
static cyd_link_event_t call_result;
#endif
static bool running;
static uint32_t latency_ms;
static int last_result = -1;
static const char *startup = "Not started";
/* This is a known-conflict guard, not a replacement for connector/module pinouts. */
static bool available(int pin, bool tx) {
    if (pin < 0 || !GPIO_IS_VALID_GPIO(pin) || (tx && !GPIO_IS_VALID_OUTPUT_GPIO(pin)))
        return false;
#ifdef CONFIG_IDF_TARGET_ESP32S3
    const int used[] = {0,  1,  3,  4,  5,  6,  7,  9,  10, 11, 12, 13, 14, 15, 16,
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
    uint64_t next_probe = 0, sent_at = 0, next_status = 0;
    uint32_t probe = 0;
    bool was_ready = false;
    for (;;) {
        int got = uart_read_bytes(UART_NUM_1, input, sizeof(input), 0);
        uint64_t now = (uint64_t)esp_timer_get_time() / 1000;
        cyd_link_status_t health = {0};
        if (now >= next_status) {
            char ip[32] = {0};
            cyd_network_address(ip, sizeof(ip));
            health = (cyd_link_status_t){
                .uptime_ms = now, .free_heap = esp_get_free_heap_size(),
                .sd_ready = cyd_storage_ready(),
                .network_address = ip[0] && strcmp(ip, "offline") != 0 && strcmp(ip, "0.0.0.0") != 0};
            snprintf(health.version, sizeof(health.version), "%s", cyd_system_version());
        }
        xSemaphoreTake(guard, portMAX_DELAY);
        cyd_link_tick(engine, now);
        if (got > 0)
            cyd_link_feed(engine, input, got);
        if (now >= next_status) {
            cyd_link_send_status(engine, &health);
            next_status = now + 1000;
        }
        if (engine->ready && now >= next_probe && !probe) {
            probe = cyd_link_ping(engine, "CYDOS", 5, 1000);
            if (probe) {
                sent_at = now;
                next_probe = now + 2000;
            }
        }
        cyd_link_event_t event;
#ifdef CONFIG_CYD_PAIRED_SYSTEM
        cyd_peer_request_t incoming;
        while(cyd_link_receive(engine,&incoming.event)) {
            incoming.boot=engine->peer_boot;
            xQueueSend(requests,&incoming,0);
        }
#endif
        while (cyd_link_pop(engine, &event))
#ifdef CONFIG_CYD_PAIRED_SYSTEM
            if(event.id==call_id && call_id) {
                call_result=event;call_id=0;xSemaphoreGive(call_done);
            } else
#endif
            if (event.id == probe) {
                last_result = event.result;
                latency_ms = event.result == CYD_LINK_OK ? (uint32_t)(now - sent_at) : 0;
                probe = 0;
            }
        bool ready = engine->ready;
        if (offset == size) {
            offset = size = 0;
            cyd_link_take_tx(engine, output, sizeof(output), &size);
        }
        xSemaphoreGive(guard);
        if (ready != was_ready) {
            cyd_log("peer", ready ? "Peer connected; board status available"
                                  : "Peer disconnected; local OS remains available");
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
    engine = heap_caps_calloc(1, sizeof(*engine), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!engine) {
        startup = "Link protocol memory unavailable; local OS remains available";
        return ESP_ERR_NO_MEM;
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
        release_engine();
        return e;
    }
    e = uart_driver_install(UART_NUM_1, 2048, 0, 0, NULL, 0);
    if (e) {
        startup = "UART driver allocation failed";
        release_engine();
        return e;
    }
    guard = xSemaphoreCreateMutex();
    if (!guard) {
        uart_driver_delete(UART_NUM_1);
        release_engine();
        startup = "Link mutex allocation failed";
        return ESP_ERR_NO_MEM;
    }
#ifdef CONFIG_CYD_PAIRED_SYSTEM
    call_guard=xSemaphoreCreateMutex();call_done=xSemaphoreCreateBinary();
    requests=xQueueCreate(2,sizeof(cyd_peer_request_t));
    if(!call_guard || !call_done || !requests) {
        if(call_guard)vSemaphoreDelete(call_guard);
        if(call_done)vSemaphoreDelete(call_done);
        if(requests)vQueueDelete(requests);
        vSemaphoreDelete(guard);guard=NULL;uart_driver_delete(UART_NUM_1);release_engine();
        startup="RPC queue allocation failed";return ESP_ERR_NO_MEM;
    }
#endif
    uint64_t nonce;
    do {
        nonce = (uint64_t)esp_random() << 32 | esp_random();
    } while (!nonce);
    cyd_link_init(engine, role(), nonce);
    if (xTaskCreate(worker, "peer_uart", 4096, NULL, 3, NULL) != pdPASS) {
#ifdef CONFIG_CYD_PAIRED_SYSTEM
        vSemaphoreDelete(call_guard);vSemaphoreDelete(call_done);vQueueDelete(requests);
#endif
        vSemaphoreDelete(guard);
        guard = NULL;
        uart_driver_delete(UART_NUM_1);
        release_engine();
        startup = "Link task allocation failed";
        return ESP_ERR_NO_MEM;
    }
    running = true;
#ifdef CONFIG_CYD_PAIRED_SYSTEM
    startup = "Paired tablet service link active";
#else
    startup = "Board status link active; app/network routing not implemented";
#endif
    return ESP_OK;
}
void cyd_peer_snapshot(cyd_peer_info_t *out) {
    *out = (cyd_peer_info_t){.configured = true, .role = role(),
                            .running = running, .message = startup, .echo_result = -1};
    if (!running)
        return;
    xSemaphoreTake(guard, portMAX_DELAY);
    out->connected = engine->ready;
    out->local_boot = engine->boot;
    out->remote_boot = engine->peer_boot;
    out->services = (engine->peer_capabilities & CYD_LINK_RPC) != 0;
    out->remote_valid = cyd_link_get_status(engine, &out->remote, &out->age_ms);
    out->errors = engine->bad_frames;
    out->drops = engine->dropped_frames;
    out->resets = engine->peer_resets;
    out->latency_ms = latency_ms;
    out->echo_result = last_result;
    xSemaphoreGive(guard);
}
#else
esp_err_t cyd_peer_start(void) { return ESP_OK; }
void cyd_peer_snapshot(cyd_peer_info_t *out) {
    *out = (cyd_peer_info_t){.role = role(), .echo_result = -1,
        .message = "Local operation; board link disabled pending qualified wiring"};
}
#endif
cJSON *cyd_peer_status(void) {
    cyd_peer_info_t s;
    cyd_peer_snapshot(&s);
    cJSON *o = cJSON_CreateObject();
    if (!o)
        return NULL;
    cJSON_AddBoolToObject(o, "configured", s.configured);
    cJSON_AddBoolToObject(o, "running", s.running);
    cJSON_AddStringToObject(o, "role", s.role == CYD_LINK_MAIN
        ? "s3_application_host" : "c5_connectivity_companion");
    cJSON_AddBoolToObject(o, "connected", s.connected);
    cJSON_AddNumberToObject(o, "protocol", CYD_LINK_PROTOCOL);
    cJSON_AddStringToObject(o, "message", s.message);
#ifdef CONFIG_CYD_PAIRED_SYSTEM
    cJSON_AddBoolToObject(o,"service_offload",s.connected&&s.services);
#else
    cJSON_AddBoolToObject(o, "service_offload", false);
#endif
    cJSON_AddBoolToObject(o, "authenticated", false);
#ifdef CONFIG_CYD_LINK_ENABLE
    cJSON_AddNumberToObject(o, "baud", CONFIG_CYD_LINK_BAUD);
#endif
    cJSON_AddNumberToObject(o, "bad_frames", s.errors);
    cJSON_AddNumberToObject(o, "dropped_frames", s.drops);
    cJSON_AddNumberToObject(o, "peer_resets", s.resets);
    cJSON_AddNumberToObject(o, "last_echo_result", s.echo_result);
    cJSON_AddNumberToObject(o, "echo_latency_ms", s.latency_ms);
    cJSON_AddBoolToObject(o, "remote_status_valid", s.remote_valid);
    if (s.remote_valid) {
        cJSON *remote = cJSON_AddObjectToObject(o, "remote");
        if (remote) {
            cJSON_AddStringToObject(remote, "firmware", s.remote.version);
            cJSON_AddNumberToObject(remote, "uptime_ms", (double)s.remote.uptime_ms);
            cJSON_AddNumberToObject(remote, "free_heap", s.remote.free_heap);
            cJSON_AddBoolToObject(remote, "sd_ready", s.remote.sd_ready);
            cJSON_AddBoolToObject(remote, "network_address", s.remote.network_address);
            cJSON_AddNumberToObject(remote, "age_ms", s.age_ms);
        }
    }
    return o;
}

esp_err_t cyd_peer_exchange(const void *in,size_t n,void *out,size_t cap,size_t *size,unsigned wait) {
#if defined(CONFIG_CYD_LINK_ENABLE) && defined(CONFIG_CYD_PAIRED_SYSTEM)
    if(!running || n>CYD_LINK_PAYLOAD-8 || wait<100 || wait>30000)return ESP_ERR_INVALID_STATE;
    if(xSemaphoreTake(call_guard,pdMS_TO_TICKS(wait))!=pdTRUE)return ESP_ERR_TIMEOUT;
    xSemaphoreTake(call_done,0);
    xSemaphoreTake(guard,portMAX_DELAY);
    call_id=cyd_link_request(engine,in,n,wait);
    bool sent=call_id!=0;
    xSemaphoreGive(guard);
    esp_err_t e=sent?ESP_ERR_TIMEOUT:ESP_ERR_INVALID_STATE;
    if(sent && xSemaphoreTake(call_done,pdMS_TO_TICKS(wait+100))==pdTRUE) {
        xSemaphoreTake(guard,portMAX_DELAY);
        if(call_result.result==CYD_LINK_OK && call_result.size<=cap) {
            if(call_result.size)memcpy(out,call_result.data,call_result.size);
            *size=call_result.size;e=ESP_OK;
        }
        xSemaphoreGive(guard);
    }
    xSemaphoreTake(guard,portMAX_DELAY);
    if(call_id)cyd_link_cancel(engine,call_id);
    call_id=0;xSemaphoreGive(guard);xSemaphoreGive(call_guard);return e;
#else
    (void)in;(void)n;(void)out;(void)cap;(void)size;(void)wait;return ESP_ERR_NOT_SUPPORTED;
#endif
}
bool cyd_peer_receive(cyd_peer_request_t *r,unsigned wait) {
#if defined(CONFIG_CYD_LINK_ENABLE) && defined(CONFIG_CYD_PAIRED_SYSTEM)
    if(!running || xQueueReceive(requests,r,pdMS_TO_TICKS(wait))!=pdTRUE)return false;
    xSemaphoreTake(guard,portMAX_DELAY);
    bool current=engine->ready && engine->peer_boot==r->boot;
    xSemaphoreGive(guard);
    return current;
#else
    (void)r;(void)wait;return false;
#endif
}
bool cyd_peer_reply(const cyd_peer_request_t *r,const void *data,size_t n) {
#if defined(CONFIG_CYD_LINK_ENABLE) && defined(CONFIG_CYD_PAIRED_SYSTEM)
    if(!running)return false;
    xSemaphoreTake(guard,portMAX_DELAY);
    bool ok=cyd_link_reply(engine,r->boot,r->event.id,data,n);
    xSemaphoreGive(guard);return ok;
#else
    (void)r;(void)data;(void)n;return false;
#endif
}
