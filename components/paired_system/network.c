#include "pair_internal.h"
#include "cyd_services.h"
#include "cyd_peer.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#if CYD_PAIR_MANAGER
typedef struct {
    uint32_t id;
    void *io;
    cyd_endpoint_t endpoint;
    uint64_t touched, owner;
} channel_t;
static channel_t *channels;
static cyd_io_t local_io;
void cyd_pair_network_tick(void) {
    if (!channels)
        return;
    cyd_peer_info_t peer;
    cyd_peer_snapshot(&peer);
    uint64_t now = esp_timer_get_time() / 1000;
    for (unsigned i = 0; i < 3; i++)
        if (channels[i].io && (!peer.connected || peer.remote_boot != channels[i].owner ||
                               now - channels[i].touched > 120000)) {
            local_io.close(channels[i].io);
            memset(&channels[i], 0, sizeof(channels[i]));
        }
}
esp_err_t cyd_pair_network(unsigned op, const void *in, size_t n, void *out, size_t cap,
                           size_t *size) {
    if (!channels) {
        channels = heap_caps_calloc(3, sizeof(*channels), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!channels)
            return ESP_ERR_NO_MEM;
    }
    if (!local_io.open)
        local_io = cyd_services_esp_io();
    cyd_pair_network_tick();
    *size = 0;
    if (cap < 4)
        return ESP_ERR_INVALID_SIZE;
    if (op == CYD_PAIR_NET_OPEN) {
        cJSON *o = cyd_json_parse(in, n, CYD_PAIR_REQUEST_MAX);
        int64_t port;
        if (!o || !cyd_pair_number(o, "port", 1, 65535, &port)) {
            cJSON_Delete(o);
            return ESP_ERR_INVALID_ARG;
        }
        unsigned i;
        for (i = 0; i < 3; i++)
            if (!channels[i].io)
                break;
        if (i == 3) {
            cJSON_Delete(o);
            return ESP_ERR_NO_MEM;
        }
        cyd_endpoint_t *ep = &channels[i].endpoint;
        memset(ep, 0, sizeof(*ep));
        const char *host = cyd_pair_string(o, "host"), *name = cyd_pair_string(o, "name"),
                   *ca = cyd_pair_string(o, "ca");
        if (!*host || strlen(host) >= sizeof(ep->host) || strlen(name) >= sizeof(ep->verify_name) ||
            strlen(ca) >= sizeof(ep->ca)) {
            cJSON_Delete(o);
            return ESP_ERR_INVALID_ARG;
        }
        strcpy(ep->host, host);
        strcpy(ep->verify_name, name);
        strcpy(ep->ca, ca);
        ep->port = (unsigned)port;
        ep->tls = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(o, "tls"));
        cJSON_Delete(o);
        char err[96];
        channels[i].io = local_io.open(ep, err, sizeof(err));
        if (!channels[i].io)
            return ESP_FAIL;
        do {
            channels[i].id = esp_random();
        } while (!channels[i].id);
        cyd_peer_info_t peer;
        cyd_peer_snapshot(&peer);
        channels[i].owner = peer.remote_boot;
        channels[i].touched = esp_timer_get_time() / 1000;
        if (cap < 12)
            return ESP_ERR_INVALID_SIZE;
        memcpy(out, &channels[i].id, 4);
        memcpy((uint8_t *)out + 4, &peer.local_boot, 8);
        *size = 12;
        return ESP_OK;
    }
    if (n < 4)
        return ESP_ERR_INVALID_ARG;
    uint32_t id;
    memcpy(&id, in, 4);
    unsigned i;
    for (i = 0; i < 3; i++)
        if (channels[i].io && channels[i].id == id)
            break;
    if (i == 3)
        return ESP_ERR_NOT_FOUND;
    channels[i].touched = esp_timer_get_time() / 1000;
    int result = 0;
    if (op == CYD_PAIR_NET_CONNECT) {
        char error[96];
        result = local_io.connect(channels[i].io, error, sizeof(error));
    } else if (op == CYD_PAIR_NET_READ) {
        size_t want = cap - 4;
        if (want > 2048)
            want = 2048;
        result = local_io.read(channels[i].io, (uint8_t *)out + 4, want);
        if (result > 0)
            *size = (size_t)result;
    } else if (op == CYD_PAIR_NET_WRITE)
        result = local_io.write(channels[i].io, (const uint8_t *)in + 4, n - 4);
    else if (op == CYD_PAIR_NET_CLOSE) {
        local_io.close(channels[i].io);
        memset(&channels[i], 0, sizeof(channels[i]));
        result = 1;
    } else
        return ESP_ERR_INVALID_ARG;
    memcpy(out, &result, 4);
    *size += 4;
    return ESP_OK;
}
#else
void cyd_pair_network_tick(void) {}
esp_err_t cyd_pair_network(unsigned op, const void *in, size_t n, void *out, size_t cap,
                           size_t *size) {
    (void)op;
    (void)in;
    (void)n;
    (void)out;
    (void)cap;
    (void)size;
    return ESP_ERR_NOT_SUPPORTED;
}
#endif
