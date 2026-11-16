#include "pair_internal.h"
#include "cyd_pair_io.h"
#include "cyd_peer.h"
#include "esp_heap_caps.h"
#include "freertos/task.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Stable bounded slots outlive app cancellation. All UART work is on this
 * worker; Lua/service IO callbacks only copy buffers under a short mutex. */
typedef struct {
    bool used, closing, failed, ready;
    uint32_t remote;
    uint64_t epoch;
    cyd_endpoint_t endpoint;
    uint8_t rx[2048], tx[2048];
    size_t rx_size, tx_size;
} proxy_t;
static proxy_t *slots;
static SemaphoreHandle_t guard;
static uint8_t *request_buffer, *reply_buffer;
static void *open_proxy(const cyd_endpoint_t *ep, char *error, size_t cap) {
    if (!guard)
        return NULL;
    xSemaphoreTake(guard, portMAX_DELAY);
    proxy_t *p = NULL;
    for (unsigned i = 0; i < 3; i++)
        if (!slots[i].used) {
            p = &slots[i];
            memset(p, 0, sizeof(*p));
            p->used = true;
            p->endpoint = *ep;
            break;
        }
    xSemaphoreGive(guard);
    if (!p)
        snprintf(error, cap, "Companion connection slots busy");
    return p;
}
static int connect_proxy(void *ctx, char *error, size_t cap) {
    proxy_t *p = ctx;
    xSemaphoreTake(guard, portMAX_DELAY);
    int state = p->failed ? -1 : p->ready ? 1 : 0;
    xSemaphoreGive(guard);
    if (state < 0)
        snprintf(error, cap, "C5 connection unavailable or certificate verification failed");
    return state;
}
static int read_proxy(void *ctx, void *out, size_t cap) {
    proxy_t *p = ctx;
    xSemaphoreTake(guard, portMAX_DELAY);
    size_t n = p->rx_size;
    if (n > cap)
        n = cap;
    if (n) {
        memcpy(out, p->rx, n);
        memmove(p->rx, p->rx + n, p->rx_size - n);
        p->rx_size -= n;
    }
    int result = n ? (int)n : p->failed ? -1 : 0;
    xSemaphoreGive(guard);
    return result;
}
static int write_proxy(void *ctx, const void *data, size_t n) {
    proxy_t *p = ctx;
    xSemaphoreTake(guard, portMAX_DELAY);
    int result = 0;
    if (p->failed)
        result = -1;
    else if (!p->tx_size) {
        if (n > sizeof(p->tx))
            n = sizeof(p->tx);
        memcpy(p->tx, data, n);
        p->tx_size = n;
        result = (int)n;
    }
    xSemaphoreGive(guard);
    return result;
}
static void close_proxy(void *ctx) {
    xSemaphoreTake(guard, portMAX_DELAY);
    ((proxy_t *)ctx)->closing = true;
    xSemaphoreGive(guard);
}
static void run(void *arg) {
    (void)arg;
    uint8_t *in = request_buffer, *out = reply_buffer;
    for (;;) {
        for (unsigned i = 0; i < 3; i++) {
            proxy_t *p = &slots[i];
            xSemaphoreTake(guard, portMAX_DELAY);
            if (!p->used) {
                xSemaphoreGive(guard);
                continue;
            }
            cyd_peer_info_t peer;
            cyd_peer_snapshot(&peer);
            if (p->remote && (!peer.connected || p->epoch != peer.remote_boot)) {
                p->remote = 0;
                p->failed = true;
            }
            bool closing = p->closing, ready = p->ready, failed = p->failed;
            uint32_t remote = p->remote;
            size_t tx = p->tx_size, rx = p->rx_size;
            unsigned op = CYD_PAIR_NET_OPEN;
            size_t n = 0;
            if (remote) {
                memcpy(in, &remote, 4);
                n = 4;
                if (closing || failed)
                    op = CYD_PAIR_NET_CLOSE;
                else if (!ready)
                    op = CYD_PAIR_NET_CONNECT;
                else if (tx) {
                    op = CYD_PAIR_NET_WRITE;
                    memcpy(in + 4, p->tx, tx);
                    n += tx;
                } else if (!rx)
                    op = CYD_PAIR_NET_READ;
                else {
                    xSemaphoreGive(guard);
                    continue;
                }
            } else if (closing) {
                memset(p, 0, sizeof(*p));
                xSemaphoreGive(guard);
                continue;
            } else if (failed) {
                xSemaphoreGive(guard);
                continue;
            } else {
                cJSON *o = cJSON_CreateObject();
                cJSON_AddStringToObject(o, "host", p->endpoint.host);
                cJSON_AddStringToObject(o, "name", p->endpoint.verify_name);
                cJSON_AddStringToObject(o, "ca", p->endpoint.ca);
                cJSON_AddNumberToObject(o, "port", p->endpoint.port);
                cJSON_AddBoolToObject(o, "tls", p->endpoint.tls);
                char *json = cJSON_PrintUnformatted(o);
                cJSON_Delete(o);
                if (!json || strlen(json) > CYD_PAIR_REQUEST_MAX) {
                    free(json);
                    p->failed = true;
                    xSemaphoreGive(guard);
                    continue;
                }
                n = strlen(json);
                memcpy(in, json, n);
                free(json);
            }
            xSemaphoreGive(guard);
            size_t got = 0;
            esp_err_t e = cyd_pair_rpc(op, in, n, out, 2052, &got);
            int value = 0;
            if (!e && got >= 4)
                memcpy(&value, out, 4);
            else
                e = ESP_FAIL;
            xSemaphoreTake(guard, portMAX_DELAY);
            if (op == CYD_PAIR_NET_CLOSE) {
                if (p->closing)
                    memset(p, 0, sizeof(*p));
                else {
                    p->remote = 0;
                    p->failed = true;
                }
            } else if (e)
                p->failed = true;
            else if (op == CYD_PAIR_NET_OPEN) {
                if (got != 12)
                    p->failed = true;
                else {
                    memcpy(&p->remote, out, 4);
                    memcpy(&p->epoch, out + 4, 8);
                }
            } else if (op == CYD_PAIR_NET_CONNECT) {
                p->ready = value == 1;
                p->failed = value < 0;
            } else if (op == CYD_PAIR_NET_WRITE) {
                if (value < 0 || (size_t)value > p->tx_size)
                    p->failed = true;
                else {
                    memmove(p->tx, p->tx + value, p->tx_size - (size_t)value);
                    p->tx_size -= (size_t)value;
                }
            } else if (op == CYD_PAIR_NET_READ) {
                if (value < 0)
                    p->failed = true;
                else if (value > 0 && (size_t)value == got - 4 && (size_t)value <= sizeof(p->rx)) {
                    memcpy(p->rx, out + 4, (size_t)value);
                    p->rx_size = (size_t)value;
                }
            }
            xSemaphoreGive(guard);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
esp_err_t cyd_pair_io_start(void) {
    slots = heap_caps_calloc(3, sizeof(*slots), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    guard = xSemaphoreCreateMutex();
    request_buffer = heap_caps_malloc(CYD_PAIR_REQUEST_MAX, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    reply_buffer = heap_caps_malloc(2052, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!slots || !guard || !request_buffer || !reply_buffer ||
        xTaskCreate(run, "c5_transport", 4096, NULL, 2, NULL) != pdPASS) {
        free(slots);
        free(request_buffer);
        free(reply_buffer);
        if (guard)
            vSemaphoreDelete(guard);
        guard = NULL;
        slots = NULL;
        request_buffer = reply_buffer = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
cyd_io_t cyd_pair_io(void) {
    return (cyd_io_t){open_proxy, connect_proxy, read_proxy, write_proxy, close_proxy};
}
