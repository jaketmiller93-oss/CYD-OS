#include "network_policy.h"
#include <string.h>
bool cyd_wifi_valid_ssid(const char *s) {
    if (!s)
        return false;
    size_t n = strnlen(s, 33);
    if (!n || n > 32)
        return false;
    for (size_t i = 0; i < n; i++)
        if ((unsigned char)s[i] < 32 || (unsigned char)s[i] == 127)
            return false;
    return true;
}
bool cyd_wifi_valid_password(const char *s, bool ap) {
    if (!s)
        return false;
    size_t n = strnlen(s, 65);
    if (!ap && n == 0)
        return true;
    if (ap && (n < 12 || n > 63))
        return false;
    if (!ap && (n < 8 || n > 64))
        return false;
    if (!ap && n == 64) {
        for (size_t i = 0; i < n; i++)
            if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f') ||
                  (s[i] >= 'A' && s[i] <= 'F')))
                return false;
    }
    for (size_t i = 0; i < n; i++)
        if ((unsigned char)s[i] < 32 || (unsigned char)s[i] == 127)
            return false;
    return true;
}
int cyd_wifi_find(const cyd_wifi_config_t *c, const char *ssid) {
    if (!c || !ssid || c->count > CYD_WIFI_PROFILES)
        return -1;
    for (unsigned i = 0; i < c->count; i++)
        if (!strcmp(c->profiles[i].ssid, ssid))
            return (int)i;
    return -1;
}
bool cyd_wifi_config_valid(const cyd_wifi_config_t *c) {
    if (!c || c->version != CYD_WIFI_CONFIG_VERSION || c->count > CYD_WIFI_PROFILES ||
        c->preferred < -1 || c->preferred >= (int)c->count || c->station_enabled > 1 ||
        c->ap_enabled > 1 || !cyd_wifi_valid_ssid(c->ap_ssid) ||
        !memchr(c->ap_password, 0, sizeof(c->ap_password)) ||
        !cyd_wifi_valid_password(c->ap_password, true))
        return false;
    if ((c->count == 0) != (c->preferred == -1))
        return false;
    for (unsigned i = 0; i < c->count; i++) {
        if (!cyd_wifi_valid_ssid(c->profiles[i].ssid) ||
            !cyd_wifi_valid_password(c->profiles[i].password, false))
            return false;
        for (unsigned j = 0; j < i; j++)
            if (!strcmp(c->profiles[i].ssid, c->profiles[j].ssid))
                return false;
    }
    return true;
}
bool cyd_wifi_remember(cyd_wifi_config_t *c, const cyd_wifi_profile_t *p) {
    if (!cyd_wifi_config_valid(c) || !p || !cyd_wifi_valid_ssid(p->ssid) ||
        !cyd_wifi_valid_password(p->password, false))
        return false;
    int i = cyd_wifi_find(c, p->ssid);
    if (i < 0) {
        if (c->count == CYD_WIFI_PROFILES)
            return false;
        i = c->count++;
    }
    c->profiles[i] = *p;
    c->preferred = (int8_t)i;
    c->station_enabled = 1;
    return true;
}
bool cyd_wifi_forget(cyd_wifi_config_t *c, unsigned i) {
    if (!cyd_wifi_config_valid(c) || i >= c->count)
        return false;
    for (unsigned j = i; j + 1 < c->count; j++)
        c->profiles[j] = c->profiles[j + 1];
    c->count--;
    memset(&c->profiles[c->count], 0, sizeof(c->profiles[0]));
    if (!c->count) {
        c->preferred = -1;
        c->station_enabled = 0;
    } else if (c->preferred == (int)i)
        c->preferred = 0;
    else if (c->preferred > (int)i)
        c->preferred--;
    return true;
}
