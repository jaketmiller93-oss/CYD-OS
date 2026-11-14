#pragma once
#include "network_policy.h"
#include "esp_err.h"
#define CYD_WIFI_SCAN_MAX 16
typedef enum {
    CYD_NET_STARTING,
    CYD_NET_OFFLINE,
    CYD_NET_CONNECTING,
    CYD_NET_CONNECTED,
    CYD_NET_FAILED
} cyd_net_state_t;
typedef struct {
    char ssid[33];
    int8_t rssi;
    bool secured, supported;
} cyd_wifi_scan_t;
typedef struct {
    bool ready, busy, scanning, station_enabled, ap_enabled;
    cyd_net_state_t state;
    uint32_t revision, scan_revision;
    char ssid[33], station_ip[16], ap_ssid[33], ap_ip[16], message[128];
    unsigned saved_count, scan_count;
    char saved[CYD_WIFI_PROFILES][33];
    cyd_wifi_scan_t scan[CYD_WIFI_SCAN_MAX];
} cyd_net_status_t;
void cyd_network_snapshot(cyd_net_status_t *);
/* All mutations enqueue work; no radio/flash operation blocks the UI task. */
esp_err_t cyd_network_scan(void);
esp_err_t cyd_network_join(const char *ssid, const char *password);
esp_err_t cyd_network_join_saved(const char *ssid);
esp_err_t cyd_network_forget(const char *ssid);
esp_err_t cyd_network_disconnect(void);
esp_err_t cyd_network_reconnect(void);
esp_err_t cyd_network_set_ap(const char *ssid, const char *password, bool enabled, bool ap_only);
/* Physical settings UI only; never return the AP password through HTTP or Lua. */
void cyd_network_ap_credentials(char *ssid, size_t ssid_size, char *password, size_t password_size);
