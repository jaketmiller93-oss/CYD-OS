#include "cyd_pair.h"
#include "cyd_peer.h"
#include "cyd_device.h"
#include "pair_ops.h"
#include "cyd_pair_io.h"
#include "cyd_pair_manager.h"
#include "cyd_ota.h"
#include "esp_heap_caps.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/task.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <stdio.h>
/* One request and one retained response per direction. Server execution is on
 * its service task, never under the UART mutex. Data lives in PSRAM. */
#define CHUNK 192u
static SemaphoreHandle_t serial;
static uint8_t *request, *response;
static uint32_t token, total, received;
static unsigned operation;
static uint64_t owner, last_at;
static size_t response_size;
static esp_err_t response_error;
static bool executed;
extern void cyd_pair_network_tick(void);
extern void cyd_pair_update_tick(void);
extern esp_err_t cyd_pair_dispatch(unsigned, const void *, size_t, void *, size_t, size_t *);
static uint32_t u32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static void w32(uint8_t *p, uint32_t n) {
    for (unsigned i = 0; i < 4; i++)
        p[i] = (uint8_t)(n >> (i * 8));
}
static void serve(void *unused) {
    (void)unused;
    for (;;) {
        cyd_peer_request_t r;
        cyd_pair_network_tick();
        cyd_pair_update_tick();
        if (token && (uint64_t)esp_timer_get_time() / 1000 - last_at >= 60000) {
            token = 0;
            memset(request, 0, CYD_PAIR_REQUEST_MAX);
            memset(response, 0, CYD_PAIR_RESPONSE_MAX);
        }
        if (!cyd_peer_receive(&r, 100))
            continue;
        const uint8_t *p = r.event.data;
        size_t n = r.event.size;
        uint8_t reply[CHUNK + 8];
        esp_err_t e = ESP_ERR_INVALID_ARG;
        size_t size = 4;
        uint64_t now = esp_timer_get_time() / 1000;
        if (n >= 5) {
            uint32_t t = u32(p + 1);
            if (p[0] == 1 && n == 11 && t && u32(p + 5) <= CYD_PAIR_REQUEST_MAX) {
                /* A client serializes complete RPCs, not just individual fragments. */
                memset(request, 0, CYD_PAIR_REQUEST_MAX);
                memset(response, 0, CYD_PAIR_RESPONSE_MAX);
                token = t;
                owner = r.boot;
                total = u32(p + 5);
                operation = p[9] | (unsigned)p[10] << 8;
                received = 0;
                response_size = 0;
                executed = false;
                last_at = now;
                e = ESP_OK;
            } else if (t == token && owner == r.boot && now - last_at < 60000) {
                if (p[0] == 2 && n >= 9 && !executed && u32(p + 5) == received &&
                    n - 9 <= total - received) {
                    memcpy(request + received, p + 9, n - 9);
                    received += (uint32_t)(n - 9);
                    e = ESP_OK;
                } else if (p[0] == 3 && n == 5 && received == total && !executed) {
                    executed = true;
                    response_size = 0;
                    response_error = cyd_ota_boot_confirmed()
                        ? cyd_pair_dispatch(operation, request, total, response,
                                            CYD_PAIR_RESPONSE_MAX, &response_size)
                        : ESP_ERR_INVALID_STATE;
                    if (response_size > CYD_PAIR_RESPONSE_MAX) {
                        response_error = ESP_ERR_INVALID_SIZE;
                        response_size = 0;
                    }
                    e = ESP_OK;
                    w32(reply + 4, (uint32_t)response_error);
                    w32(reply + 8, (uint32_t)response_size);
                    size = 12;
                } else if (p[0] == 4 && n == 9 && executed && u32(p + 5) <= response_size) {
                    size_t off = u32(p + 5), take = response_size - off;
                    if (take > CHUNK)
                        take = CHUNK;
                    memcpy(reply + 4, response + off, take);
                    size = 4 + take;
                    e = ESP_OK;
                } else if (p[0] == 5 && n == 5) {
                    token = 0;
                    memset(request, 0, CYD_PAIR_REQUEST_MAX);
                    memset(response, 0, CYD_PAIR_RESPONSE_MAX);
                    e = ESP_OK;
                }
                last_at = now;
            }
        }
        w32(reply, (uint32_t)e);
        cyd_peer_reply(&r, reply, size);
    }
}
static esp_err_t exchange(const void *p, size_t n, uint8_t *reply, size_t *len, unsigned wait) {
    *len = CHUNK + 8;
    esp_err_t e = cyd_peer_exchange(p, n, reply, *len, len, wait);
    if (!e && *len < 4)
        e = ESP_ERR_INVALID_RESPONSE;
    if (!e)
        e = (esp_err_t)u32(reply);
    return e;
}
esp_err_t cyd_pair_rpc(unsigned op, const void *in, size_t n, void *out, size_t cap, size_t *size) {
    if (!serial || n > CYD_PAIR_REQUEST_MAX || (n && !in) || op > 65535 || !size)
        return ESP_ERR_INVALID_ARG;
    *size = 0;
    if (!cyd_ota_boot_confirmed())
        return ESP_ERR_INVALID_STATE;
    if (xSemaphoreTake(serial, pdMS_TO_TICKS(5000)) != pdTRUE)
        return ESP_ERR_TIMEOUT;
    uint32_t t;
    do {
        t = esp_random();
    } while (!t);
    uint8_t p[CHUNK + 9], reply[CHUNK + 8];
    size_t len = 0;
    p[0] = 1;
    w32(p + 1, t);
    w32(p + 5, (uint32_t)n);
    p[9] = op;
    p[10] = op >> 8;
    esp_err_t e = exchange(p, 11, reply, &len, 5000);
    for (size_t off = 0; !e && off < n;) {
        size_t take = n - off;
        if (take > CHUNK)
            take = CHUNK;
        p[0] = 2;
        w32(p + 1, t);
        w32(p + 5, (uint32_t)off);
        memcpy(p + 9, (const uint8_t *)in + off, take);
        e = exchange(p, take + 9, reply, &len, 5000);
        off += take;
    }
    if (!e) {
        p[0] = 3;
        w32(p + 1, t);
        unsigned wait = (op == CYD_PAIR_APP_FINISH || op == CYD_PAIR_UPDATE_BEGIN ||
                         op == CYD_PAIR_UPDATE_FINISH || op == CYD_PAIR_FILE_COMMIT)
                            ? 30000
                            : 5000;
        e = exchange(p, 5, reply, &len, wait);
        if (!e && len != 12)
            e = ESP_ERR_INVALID_RESPONSE;
        if (!e) {
            e = (esp_err_t)u32(reply + 4);
            size_t wanted = u32(reply + 8);
            if (!e && (wanted > cap || (wanted && !out)))
                e = ESP_ERR_INVALID_SIZE;
            while (!e && *size < wanted) {
                p[0] = 4;
                w32(p + 1, t);
                w32(p + 5, (uint32_t)*size);
                e = exchange(p, 9, reply, &len, 5000);
                if (!e && (len <= 4 || len - 4 > wanted - *size))
                    e = ESP_ERR_INVALID_RESPONSE;
                if (!e) {
                    memcpy((uint8_t *)out + *size, reply + 4, len - 4);
                    *size += len - 4;
                }
            }
        }
    }
    /* Do not retry a failed EXEC: its side effects may already have happened. */
    if (!e) {
        p[0] = 5;
        w32(p + 1, t);
        exchange(p, 5, reply, &len, 5000);
    }
    xSemaphoreGive(serial);
    return e;
}
cJSON *cyd_pair_json(unsigned op, const cJSON *in) {
    char *text = in ? cJSON_PrintUnformatted(in) : NULL;
    char *out = heap_caps_malloc(CYD_PAIR_RESPONSE_MAX + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!out) {
        free(text);
        return NULL;
    }
    size_t n = 0;
    esp_err_t e = cyd_pair_rpc(op, text, text ? strlen(text) : 0, out, CYD_PAIR_RESPONSE_MAX, &n);
    free(text);
    cJSON *o = NULL;
    if (!e)
        o = cyd_json_parse(out, n, CYD_PAIR_RESPONSE_MAX);
    free(out);
    return o;
}
extern void cyd_pair_network_cache(const cJSON *);
#if CYD_PAIR_HOST
static void sync_manager(void *arg) {
    (void)arg;
    for (;;) {
        if (!cyd_ota_boot_confirmed()) {
            vTaskDelay(pdMS_TO_TICKS(250));
            continue;
        }
        cJSON *s = cyd_pair_json(CYD_PAIR_STATUS, NULL);
        if (s) {
            cyd_pair_network_cache(s);
            cJSON *epoch = cJSON_GetObjectItemCaseSensitive(s, "epoch"),
                  *offset = cJSON_GetObjectItemCaseSensitive(s, "offset");
            if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(s, "clock_valid")) &&
                cJSON_IsNumber(epoch) && cJSON_IsNumber(offset))
                cyd_device_adopt_clock(
                    (int64_t)epoch->valuedouble, offset->valueint,
                    cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(s, "clock_12h")));
        }
        if (!s)
            cyd_pair_network_cache(NULL);
        cJSON_Delete(s);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
#endif
esp_err_t cyd_pair_start(void) {
#if CYD_PAIR_ENABLED
    serial = xSemaphoreCreateMutex();
    request = heap_caps_malloc(CYD_PAIR_REQUEST_MAX, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    response = heap_caps_malloc(CYD_PAIR_RESPONSE_MAX, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    const char *failure = !serial ? "RPC mutex" : !request ? "request PSRAM" :
                          !response ? "response PSRAM" : NULL;
    if (!failure && xTaskCreate(serve, "pair_services", 8192, NULL, 2, NULL) != pdPASS)
        failure = "RPC service task (8192-byte internal stack)";
    if (failure) {
        char detail[160];
        snprintf(detail, sizeof(detail), "Startup failed at %s", failure);
        cyd_log("pair", detail);
        free(request);
        free(response);
        if (serial)
            vSemaphoreDelete(serial);
        serial = NULL;
        request = response = NULL;
        return ESP_ERR_NO_MEM;
    }
#if CYD_PAIR_HOST
    esp_err_t e = cyd_pair_io_start();
    if (e) {
        cyd_log("pair", "Startup failed at network proxy");
        return e;
    }
    if (xTaskCreate(sync_manager, "manager_sync", 4096, NULL, 2, NULL) != pdPASS) {
        cyd_log("pair", "Startup failed at manager sync task");
        return ESP_ERR_NO_MEM;
    }
#endif
#if CYD_PAIR_MANAGER
    esp_err_t e = cyd_pair_manager_start();
    if (e)
        return e; /* Preserve NVS/settings failures; they are not heap errors. */
#endif
#endif
    return ESP_OK;
}
