#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define CYD_WIFI_PROFILES 5
#define CYD_WIFI_SSID_BYTES 32
#define CYD_WIFI_CONFIG_VERSION 1
/* Persisted as one NVS blob. Credentials never appear in the public status. */
typedef struct {
    char ssid[33];
    char password[65];
} cyd_wifi_profile_t;
typedef struct {
    uint32_t version;
    uint8_t count;
    int8_t preferred;
    uint8_t station_enabled, ap_enabled;
    char ap_ssid[33], ap_password[64];
    cyd_wifi_profile_t profiles[CYD_WIFI_PROFILES];
} cyd_wifi_config_t;
bool cyd_wifi_valid_ssid(const char *);
bool cyd_wifi_valid_password(const char *, bool access_point);
bool cyd_wifi_config_valid(const cyd_wifi_config_t *);
int cyd_wifi_find(const cyd_wifi_config_t *, const char *ssid);
/* A successful connection updates or appends a profile, making it preferred. */
bool cyd_wifi_remember(cyd_wifi_config_t *, const cyd_wifi_profile_t *);
bool cyd_wifi_forget(cyd_wifi_config_t *, unsigned index);
