#include "cyd_pair.h"
#include "cyd_network.h"
#include "cyd_ota.h"
#include "esp_timer.h"
#include <string.h>
#include <stdio.h>
#if CYD_PAIR_HOST
static cyd_net_status_t cached;
static SemaphoreHandle_t cache_guard;
static esp_timer_handle_t heartbeat_timer;
static void heartbeat(void *arg) {
    (void)arg;
    cyd_ota_health_heartbeat(CYD_HEALTH_NETWORK);
}
void cyd_pair_network_cache(const cJSON *o) {
    const cJSON *net = cJSON_GetObjectItemCaseSensitive(o, "network");
    if (!cJSON_IsObject(net)) {
        xSemaphoreTake(cache_guard, portMAX_DELAY);
        memset(&cached, 0, sizeof(cached));
        snprintf(cached.message, sizeof(cached.message), "C5 manager unavailable");
        xSemaphoreGive(cache_guard);
        return;
    }
    xSemaphoreTake(cache_guard, portMAX_DELAY);
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(net, "station_ip");
    if (cJSON_IsString(v))
        snprintf(cached.station_ip, sizeof(cached.station_ip), "%s", v->valuestring);
    v = cJSON_GetObjectItemCaseSensitive(net, "ap_ip");
    if (cJSON_IsString(v))
        snprintf(cached.ap_ip, sizeof(cached.ap_ip), "%s", v->valuestring);
    v = cJSON_GetObjectItemCaseSensitive(net, "station_ssid");
    if (cJSON_IsString(v))
        snprintf(cached.ssid, sizeof(cached.ssid), "%s", v->valuestring);
    v = cJSON_GetObjectItemCaseSensitive(net, "ap_ssid");
    if (cJSON_IsString(v))
        snprintf(cached.ap_ssid, sizeof(cached.ap_ssid), "%s", v->valuestring);
    cached.ready = true;
    cached.ap_enabled = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(net, "ap_enabled"));
    cached.state = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(net, "connected"))
                       ? CYD_NET_CONNECTED
                       : CYD_NET_OFFLINE;
    snprintf(cached.message, sizeof(cached.message), "Managed by C5");
    cached.revision++;
    xSemaphoreGive(cache_guard);
}
esp_err_t cyd_network_init(void) {
    cache_guard = xSemaphoreCreateMutex();
    if (!cache_guard)
        return ESP_ERR_NO_MEM;
    snprintf(cached.message, sizeof(cached.message), "Connections are managed on C5");
    esp_timer_create_args_t args = {.callback = heartbeat, .name = "host_broker_health"};
    esp_err_t e = esp_timer_create(&args, &heartbeat_timer);
    if (!e)
        e = esp_timer_start_periodic(heartbeat_timer, 250000);
    return e;
}
void cyd_network_snapshot(cyd_net_status_t *out) {
    if (cache_guard)
        xSemaphoreTake(cache_guard, portMAX_DELAY);
    *out = cached;
    if (cache_guard)
        xSemaphoreGive(cache_guard);
}
void cyd_network_address(char *out, size_t cap) {
    cyd_net_status_t s;
    cyd_network_snapshot(&s);
    snprintf(out, cap, "%s", s.station_ip[0] ? s.station_ip : s.ap_ip[0] ? s.ap_ip : "offline");
}
bool cyd_network_connected(void) {
    cyd_net_status_t s;
    cyd_network_snapshot(&s);
    return s.state == CYD_NET_CONNECTED;
}
void cyd_network_ap_credentials(char *a, size_t an, char *b, size_t bn) {
    if (an)
        *a = 0;
    if (bn)
        *b = 0;
}
esp_err_t cyd_network_scan(void) {
    return ESP_ERR_NOT_SUPPORTED;
}
esp_err_t cyd_network_join(const char *a, const char *b) {
    (void)a;
    (void)b;
    return ESP_ERR_NOT_SUPPORTED;
}
esp_err_t cyd_network_join_saved(const char *a) {
    (void)a;
    return ESP_ERR_NOT_SUPPORTED;
}
esp_err_t cyd_network_forget(const char *a) {
    (void)a;
    return ESP_ERR_NOT_SUPPORTED;
}
esp_err_t cyd_network_disconnect(void) {
    return ESP_ERR_NOT_SUPPORTED;
}
esp_err_t cyd_network_reconnect(void) {
    return ESP_ERR_NOT_SUPPORTED;
}
esp_err_t cyd_network_set_ap(const char *a, const char *b, bool c, bool d) {
    (void)a;
    (void)b;
    (void)c;
    (void)d;
    return ESP_ERR_NOT_SUPPORTED;
}
#else
void cyd_pair_network_cache(const cJSON *o) {
    (void)o;
}
#endif
