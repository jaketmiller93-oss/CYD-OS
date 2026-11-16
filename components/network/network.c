#include "sdkconfig.h"
#if !defined(CONFIG_CYD_PAIRED_SYSTEM) || !defined(CONFIG_IDF_TARGET_ESP32S3)
#include "cyd.h"
#include "cyd_network.h"
#include "cyd_ota.h"
#include "sdkconfig.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "esp_check.h"
#include "nvs.h"
#include "mdns.h"
#include "freertos/queue.h"
#include <stdio.h>
#include <string.h>
#include <stdatomic.h>

typedef enum {
    JOB_SCAN,
    JOB_JOIN,
    JOB_SAVED,
    JOB_FORGET,
    JOB_DISCONNECT,
    JOB_RECONNECT,
    JOB_AP,
    JOB_IP,
    JOB_LOST_IP
} job_kind_t;
typedef struct {
    job_kind_t kind;
    cyd_wifi_profile_t profile;
    bool enabled, ap_only;
    uint32_t address, generation;
} job_t;
static QueueHandle_t jobs;
static SemaphoreHandle_t guard;
static esp_netif_t *station_netif, *ap_netif;
static nvs_handle_t store;
static cyd_wifi_config_t config;
static cyd_net_status_t status;
static cyd_wifi_profile_t candidate;
static bool connecting, manual_join, driver_started;
static atomic_bool service_started;
static atomic_uint link_generation;
static int next_profile;
static int64_t deadline, next_retry;
static int64_t now_ms(void) {
    return esp_timer_get_time() / 1000;
}
static void clear_private(void *p, size_t n) {
    volatile unsigned char *b = p;
    while (n--)
        *b++ = 0;
}
static void message(cyd_net_state_t state, const char *text) {
    xSemaphoreTake(guard, portMAX_DELAY);
    status.state = state;
    snprintf(status.message, sizeof(status.message), "%s", text);
    status.revision++;
    xSemaphoreGive(guard);
}
static void publish_config(void) {
    xSemaphoreTake(guard, portMAX_DELAY);
    status.station_enabled = config.station_enabled;
    status.ap_enabled = config.ap_enabled;
    status.saved_count = config.count;
    snprintf(status.ap_ssid, sizeof(status.ap_ssid), "%s", config.ap_ssid);
    memset(status.saved, 0, sizeof(status.saved));
    for (unsigned i = 0; i < config.count; i++)
        snprintf(status.saved[i], 33, "%s", config.profiles[i].ssid);
    esp_netif_ip_info_t ip = {0};
    status.ap_ip[0] = 0;
    if (config.ap_enabled && esp_netif_get_ip_info(ap_netif, &ip) == ESP_OK)
        snprintf(status.ap_ip, sizeof(status.ap_ip), IPSTR, IP2STR(&ip.ip));
    status.revision++;
    xSemaphoreGive(guard);
}
void cyd_network_snapshot(cyd_net_status_t *out) {
    memset(out, 0, sizeof(*out));
    out->state = CYD_NET_STARTING;
    if (!guard)
        return;
    xSemaphoreTake(guard, portMAX_DELAY);
    *out = status;
    xSemaphoreGive(guard);
}
void cyd_network_ap_credentials(char *ssid, size_t ns, char *pass, size_t np) {
    if (!guard) {
        if (ns)
            ssid[0] = 0;
        if (np)
            pass[0] = 0;
        return;
    }
    xSemaphoreTake(guard, portMAX_DELAY);
    snprintf(ssid, ns, "%s", config.ap_ssid);
    snprintf(pass, np, "%s", config.ap_password);
    xSemaphoreGive(guard);
}
bool cyd_network_connected(void) {
    cyd_net_status_t s;
    cyd_network_snapshot(&s);
    return s.ready && (s.state == CYD_NET_CONNECTED || s.ap_enabled);
}
void cyd_network_address(char *b, size_t n) {
    cyd_net_status_t s;
    cyd_network_snapshot(&s);
    const char *ip = s.state == CYD_NET_CONNECTED ? s.station_ip : s.ap_enabled ? s.ap_ip : "";
    snprintf(b, n, "%s", *ip ? ip : "offline");
}
static esp_err_t post(const job_t *j) {
    if (cyd_maintenance_active() && j->kind != JOB_IP && j->kind != JOB_LOST_IP)
        return ESP_ERR_INVALID_STATE;
    if (!jobs || !atomic_load(&service_started))
        return ESP_ERR_INVALID_STATE;
    return xQueueSend(jobs, j, 0) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}
esp_err_t cyd_network_scan(void) {
    job_t j = {.kind = JOB_SCAN};
    return post(&j);
}
esp_err_t cyd_network_join(const char *ssid, const char *pass) {
    if (!cyd_wifi_valid_ssid(ssid) || !cyd_wifi_valid_password(pass, false))
        return ESP_ERR_INVALID_ARG;
    job_t j = {.kind = JOB_JOIN};
    snprintf(j.profile.ssid, 33, "%s", ssid);
    snprintf(j.profile.password, 65, "%s", pass);
    esp_err_t e = post(&j);
    clear_private(&j, sizeof(j));
    return e;
}
static esp_err_t name_job(job_kind_t kind, const char *ssid) {
    if (!cyd_wifi_valid_ssid(ssid))
        return ESP_ERR_INVALID_ARG;
    job_t j = {.kind = kind};
    snprintf(j.profile.ssid, 33, "%s", ssid);
    return post(&j);
}
esp_err_t cyd_network_join_saved(const char *s) {
    return name_job(JOB_SAVED, s);
}
esp_err_t cyd_network_forget(const char *s) {
    return name_job(JOB_FORGET, s);
}
esp_err_t cyd_network_disconnect(void) {
    job_t j = {.kind = JOB_DISCONNECT};
    return post(&j);
}
esp_err_t cyd_network_reconnect(void) {
    job_t j = {.kind = JOB_RECONNECT};
    return post(&j);
}
esp_err_t cyd_network_set_ap(const char *ssid, const char *pass, bool enabled, bool only) {
    if (!cyd_wifi_valid_ssid(ssid) || !cyd_wifi_valid_password(pass, true))
        return ESP_ERR_INVALID_ARG;
    job_t j = {.kind = JOB_AP, .enabled = enabled, .ap_only = only};
    snprintf(j.profile.ssid, 33, "%s", ssid);
    snprintf(j.profile.password, 65, "%s", pass);
    esp_err_t e = post(&j);
    clear_private(&j, sizeof(j));
    return e;
}
static esp_err_t persist(const cyd_wifi_config_t *next) {
    if (!cyd_wifi_config_valid(next))
        return ESP_ERR_INVALID_ARG;
    esp_err_t e = nvs_set_blob(store, "settings", next, sizeof(*next));
    if (!e)
        e = nvs_commit(store);
    if (!e) {
        xSemaphoreTake(guard, portMAX_DELAY);
        config = *next;
        xSemaphoreGive(guard);
        publish_config();
    }
    return e;
}
static void clear_link(void) {
    atomic_fetch_add(&link_generation, 1);
    connecting = false;
    xSemaphoreTake(guard, portMAX_DELAY);
    status.station_ip[0] = 0;
    status.ssid[0] = 0;
    status.revision++;
    xSemaphoreGive(guard);
}
static esp_err_t configure_radio(void) {
    clear_link();
    if (driver_started) {
        esp_err_t e = esp_wifi_stop();
        if (e)
            return e;
        driver_started = false;
    }
    wifi_mode_t mode = config.station_enabled
                           ? (config.ap_enabled ? WIFI_MODE_APSTA : WIFI_MODE_STA)
                           : (config.ap_enabled ? WIFI_MODE_AP : WIFI_MODE_NULL);
    esp_err_t e = esp_wifi_set_mode(mode);
    if (e)
        return e;
    if (config.ap_enabled) {
        wifi_config_t ap = {0};
        size_t n = strlen(config.ap_ssid);
        memcpy(ap.ap.ssid, config.ap_ssid, n);
        ap.ap.ssid_len = n;
        memcpy(ap.ap.password, config.ap_password, strlen(config.ap_password));
        ap.ap.authmode = WIFI_AUTH_WPA2_PSK;
        ap.ap.max_connection = 3;
        ap.ap.channel = 1;
        e = esp_wifi_set_config(WIFI_IF_AP, &ap);
        if (e)
            return e;
    }
    if (mode != WIFI_MODE_NULL) {
        e = esp_wifi_start();
        if (e)
            return e;
        driver_started = true;
#ifdef CONFIG_SOC_WIFI_SUPPORT_5G
        /* C5 scans either band; an AP-only network stays on the 2.4 GHz band. */
        esp_err_t band_error = esp_wifi_set_band_mode(
            config.station_enabled ? WIFI_BAND_MODE_AUTO : WIFI_BAND_MODE_2G_ONLY);
        if (band_error)
            ESP_LOGW("network", "Using default Wi-Fi band configuration");
#endif
    }
    xSemaphoreTake(guard, portMAX_DELAY);
    status.ready = true;
    xSemaphoreGive(guard);
    publish_config();
    message(CYD_NET_OFFLINE, "Network settings applied");
    return ESP_OK;
}
static esp_err_t attempt(const cyd_wifi_profile_t *p, bool manual) {
    /* The worker owns candidate credentials until DHCP succeeds or times out. */
    candidate = *p;
    manual_join = manual;
    clear_link();
    if (!driver_started || !config.station_enabled) {
        /* Connecting doesn't persist a new/changed profile before success. */
        xSemaphoreTake(guard, portMAX_DELAY);
        config.station_enabled = 1;
        xSemaphoreGive(guard);
        esp_err_t e = configure_radio();
        if (e)
            return e;
    }
    esp_wifi_disconnect();
    wifi_config_t station = {0};
    memcpy(station.sta.ssid, p->ssid, strlen(p->ssid));
    memcpy(station.sta.password, p->password, strlen(p->password));
    station.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    station.sta.pmf_cfg.capable = true;
    esp_err_t e = esp_wifi_set_config(WIFI_IF_STA, &station);
    if (e)
        return e;
    xSemaphoreTake(guard, portMAX_DELAY);
    snprintf(status.ssid, 33, "%s", p->ssid);
    status.station_enabled = true;
    xSemaphoreGive(guard);
    e = esp_wifi_connect();
    if (e)
        return e;
    connecting = true;
    deadline = now_ms() + 15000;
    message(CYD_NET_CONNECTING, "Connecting; waiting for an IP address");
    return ESP_OK;
}
static void accept_ip(const job_t *job) {
    if (!job->address || job->generation != atomic_load(&link_generation))
        return;
    wifi_ap_record_t ap;
    if (esp_wifi_sta_get_ap_info(&ap) || strcmp((char *)ap.ssid, job->profile.ssid))
        return;
    if (!connecting) {
        if (status.state == CYD_NET_CONNECTED && !strcmp(status.ssid, job->profile.ssid)) {
            esp_ip4_addr_t ip = {.addr = job->address};
            xSemaphoreTake(guard, portMAX_DELAY);
            snprintf(status.station_ip, 16, IPSTR, IP2STR(&ip));
            status.revision++;
            xSemaphoreGive(guard);
        }
        return;
    }
    if (strcmp(job->profile.ssid, candidate.ssid))
        return;
    bool saved = true;
    if (manual_join) {
        cyd_wifi_config_t next = config;
        saved = cyd_wifi_remember(&next, &candidate) && persist(&next) == ESP_OK;
        clear_private(&next, sizeof(next));
    } else {
        int i = cyd_wifi_find(&config, candidate.ssid);
        if (i >= 0 && i != config.preferred) {
            cyd_wifi_config_t next = config;
            next.preferred = i;
            saved = persist(&next) == ESP_OK;
            clear_private(&next, sizeof(next));
        }
    }
    connecting = false;
    next_retry = 0;
    esp_ip4_addr_t ip = {.addr = job->address};
    xSemaphoreTake(guard, portMAX_DELAY);
    snprintf(status.station_ip, 16, IPSTR, IP2STR(&ip));
    xSemaphoreGive(guard);
    message(CYD_NET_CONNECTED, saved ? "Connected" : "Connected, but saving credentials failed");
    clear_private(&candidate, sizeof(candidate));
    cyd_log("network", saved ? "Wi-Fi connected" : "Wi-Fi connected; NVS save failed");
}
static void scan_networks(void) {
    bool resume = config.station_enabled;
    xSemaphoreTake(guard, portMAX_DELAY);
    status.scanning = true;
    status.scan_count = 0;
    status.revision++;
    xSemaphoreGive(guard);
    if (!resume) {
        xSemaphoreTake(guard, portMAX_DELAY);
        config.station_enabled = 1;
        xSemaphoreGive(guard);
        configure_radio();
    }
    if (connecting) {
        esp_wifi_disconnect();
        clear_link();
        message(CYD_NET_OFFLINE, "Connection paused for scan");
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    wifi_scan_config_t sc = {.show_hidden = false};
    esp_err_t e = esp_wifi_scan_start(&sc, true);
    wifi_ap_record_t found[32];
    uint16_t count = 32;
    if (!e)
        e = esp_wifi_scan_get_ap_records(&count, found);
    if (!e) {
        xSemaphoreTake(guard, portMAX_DELAY);
        for (unsigned i = 0; i < count && status.scan_count < CYD_WIFI_SCAN_MAX; i++) {
            found[i].ssid[32] = 0;
            if (!cyd_wifi_valid_ssid((char *)found[i].ssid))
                continue;
            bool duplicate = false;
            for (unsigned j = 0; j < status.scan_count; j++)
                if (!strcmp(status.scan[j].ssid, (char *)found[i].ssid))
                    duplicate = true;
            if (duplicate)
                continue;
            cyd_wifi_scan_t *p = &status.scan[status.scan_count++];
            snprintf(p->ssid, 33, "%s", (char *)found[i].ssid);
            p->rssi = found[i].rssi;
            p->secured = found[i].authmode != WIFI_AUTH_OPEN;
            p->supported = found[i].authmode == WIFI_AUTH_OPEN ||
                           found[i].authmode == WIFI_AUTH_WPA_PSK ||
                           found[i].authmode == WIFI_AUTH_WPA2_PSK ||
                           found[i].authmode == WIFI_AUTH_WPA_WPA2_PSK ||
                           found[i].authmode == WIFI_AUTH_WPA3_PSK ||
                           found[i].authmode == WIFI_AUTH_WPA2_WPA3_PSK;
        }
        xSemaphoreGive(guard);
    } else
        esp_wifi_clear_ap_list();
    if (!resume) {
        xSemaphoreTake(guard, portMAX_DELAY);
        config.station_enabled = 0;
        xSemaphoreGive(guard);
        configure_radio();
    }
    xSemaphoreTake(guard, portMAX_DELAY);
    status.scanning = false;
    status.scan_revision++;
    status.revision++;
    xSemaphoreGive(guard);
    if (e)
        message(status.state, "Scan failed; try again after connecting stops");
    next_retry = now_ms() + 3000;
}
static void process(job_t *j) {
    if (cyd_maintenance_active() && j->kind != JOB_IP && j->kind != JOB_LOST_IP)
        return;
    esp_err_t e = ESP_OK;
    cyd_wifi_config_t next;
    switch (j->kind) {
    case JOB_SCAN:
        scan_networks();
        return;
    case JOB_JOIN:
        if (config.count == CYD_WIFI_PROFILES && cyd_wifi_find(&config, j->profile.ssid) < 0) {
            message(status.state, "Five networks saved; forget one first");
            return;
        }
        e = attempt(&j->profile, true);
        break;
    case JOB_SAVED: {
        int i = cyd_wifi_find(&config, j->profile.ssid);
        if (i < 0) {
            message(status.state, "Saved network not found");
            return;
        }
        e = attempt(&config.profiles[i], true);
        break;
    }
    case JOB_FORGET: {
        int i = cyd_wifi_find(&config, j->profile.ssid);
        next = config;
        if (i < 0 || !cyd_wifi_forget(&next, i)) {
            message(status.state, "Saved network not found");
            return;
        }
        bool active = !strcmp(status.ssid, j->profile.ssid);
        e = persist(&next);
        if (!e && active) {
            e = configure_radio();
            next_retry = now_ms();
        }
        break;
    }
    case JOB_DISCONNECT:
        next = config;
        next.station_enabled = 0;
        e = persist(&next);
        if (!e)
            e = configure_radio();
        if (!e)
            message(CYD_NET_OFFLINE, "Wi-Fi disconnected; saved networks retained");
        break;
    case JOB_RECONNECT:
        if (!config.count) {
            message(status.state, "No saved networks; scan or join one first");
            return;
        }
        next = config;
        next.station_enabled = 1;
        e = persist(&next);
        if (!e) {
            e = configure_radio();
            next_profile = config.preferred;
            next_retry = now_ms();
        }
        break;
    case JOB_AP:
        next = config;
        next.ap_enabled = j->enabled;
        snprintf(next.ap_ssid, 33, "%s", j->profile.ssid);
        snprintf(next.ap_password, 64, "%.63s", j->profile.password);
        if (j->ap_only)
            next.station_enabled = 0;
        e = persist(&next);
        if (!e)
            e = configure_radio();
        if (!e) {
            next_retry = now_ms();
            message(CYD_NET_OFFLINE, j->enabled ? "Protected AP started; no internet routing"
                                                : "Access point stopped");
        }
        break;
    case JOB_IP:
        accept_ip(j);
        return;
    case JOB_LOST_IP:
        if (status.state == CYD_NET_CONNECTED) {
            clear_link();
            message(CYD_NET_OFFLINE, "IP address lost; reconnecting");
            next_retry = now_ms() + 3000;
        }
        return;
    }
    clear_private(&next, sizeof(next));
    if (e) {
        message(CYD_NET_FAILED, "Network operation failed; check settings and try again");
        connecting = false;
        next_retry = now_ms() + 30000;
    }
}
static void maintain_connection(void) {
    esp_err_t e;
    if (connecting) {
        if (now_ms() >= deadline) {
            esp_wifi_disconnect();
            connecting = false;
            message(CYD_NET_FAILED,
                    manual_join
                        ? "Could not connect. Check password and signal; saved settings retained"
                        : "Saved network unavailable; trying other saved networks");
            clear_private(&candidate, sizeof(candidate));
            next_retry = now_ms() + (manual_join ? 30000 : 3000);
        }
    } else if (status.state == CYD_NET_CONNECTED) {
        wifi_ap_record_t ap;
        if (esp_wifi_sta_get_ap_info(&ap)) {
            clear_link();
            message(CYD_NET_OFFLINE, "Connection lost; reconnecting to saved networks");
            next_retry = now_ms() + 3000;
            next_profile = config.preferred;
        }
    } else if (status.ready && config.station_enabled && config.count && now_ms() >= next_retry) {
        if (next_profile < 0 || next_profile >= (int)config.count)
            next_profile = config.preferred;
        cyd_wifi_profile_t p = config.profiles[next_profile];
        next_profile = (next_profile + 1) % config.count;
        e = attempt(&p, false);
        clear_private(&p, sizeof(p));
        if (e) {
            message(CYD_NET_FAILED, "Connection attempt failed; retrying later");
            next_retry = now_ms() + 3000;
        }
    }
}
static void worker(void *arg) {
    (void)arg;
    esp_err_t e = configure_radio();
    xSemaphoreTake(guard, portMAX_DELAY);
    status.ready = e == ESP_OK;
    xSemaphoreGive(guard);
    if (e)
        message(CYD_NET_FAILED, "Wi-Fi initialization failed");
    else
        message(CYD_NET_OFFLINE, "Ready");
    next_profile = config.preferred;
    next_retry = now_ms();
    for (;;) {
        job_t job;
        if (xQueueReceive(jobs, &job, pdMS_TO_TICKS(250)) == pdTRUE) {
            xSemaphoreTake(guard, portMAX_DELAY);
            status.busy = true;
            xSemaphoreGive(guard);
            process(&job);
            clear_private(&job, sizeof(job));
            xSemaphoreTake(guard, portMAX_DELAY);
            status.busy = false;
            status.revision++;
            xSemaphoreGive(guard);
        }
        if (!cyd_maintenance_active())
            maintain_connection();
        if (status.ready)
            cyd_ota_health_heartbeat(CYD_HEALTH_NETWORK);
    }
}
static void event(void *arg, esp_event_base_t base, int32_t id, void *data) {
    (void)arg;
    if (base != IP_EVENT)
        return;
    if (id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *got = data;
        wifi_ap_record_t ap;
        if (got->esp_netif != station_netif || esp_wifi_sta_get_ap_info(&ap))
            return;
        job_t j = {.kind = JOB_IP,
                   .address = got->ip_info.ip.addr,
                   .generation = atomic_load(&link_generation)};
        memcpy(j.profile.ssid, ap.ssid, 32);
        j.profile.ssid[32] = 0;
        post(&j);
    } else if (id == IP_EVENT_STA_LOST_IP) {
        job_t j = {.kind = JOB_LOST_IP};
        post(&j);
    }
}
static esp_err_t initialize(void) {
    guard = xSemaphoreCreateMutex();
    jobs = xQueueCreate(16, sizeof(job_t));
    if (!guard || !jobs)
        return ESP_ERR_NO_MEM;
    ESP_RETURN_ON_ERROR(nvs_open("cyd_wifi", NVS_READWRITE, &store), "network", "NVS open");
    size_t n = sizeof(config);
    esp_err_t e = nvs_get_blob(store, "settings", &config, &n);
    if (e || n != sizeof(config) || !cyd_wifi_config_valid(&config)) {
        memset(&config, 0, sizeof(config));
        config.version = CYD_WIFI_CONFIG_VERSION;
        config.preferred = -1;
        snprintf(config.ap_ssid, 33, "CYD-OS");
        if (cyd_wifi_valid_password(CONFIG_CYD_AP_PASSWORD, true))
            snprintf(config.ap_password, 64, "%s", CONFIG_CYD_AP_PASSWORD);
        else
            snprintf(config.ap_password, 64, "%08lx%08lx", (unsigned long)esp_random(),
                     (unsigned long)esp_random());
        if (cyd_wifi_valid_ssid(CONFIG_CYD_WIFI_SSID) &&
            cyd_wifi_valid_password(CONFIG_CYD_WIFI_PASSWORD, false)) {
            cyd_wifi_profile_t p = {0};
            snprintf(p.ssid, 33, "%s", CONFIG_CYD_WIFI_SSID);
            snprintf(p.password, 65, "%s", CONFIG_CYD_WIFI_PASSWORD);
            cyd_wifi_remember(&config, &p);
            clear_private(&p, sizeof(p));
        }
        config.ap_enabled = !config.count || cyd_wifi_valid_password(CONFIG_CYD_AP_PASSWORD, true);
        ESP_RETURN_ON_ERROR(nvs_set_blob(store, "settings", &config, sizeof(config)), "network",
                            "NVS seed");
        ESP_RETURN_ON_ERROR(nvs_commit(store), "network", "NVS commit");
    }
    ESP_RETURN_ON_ERROR(esp_netif_init(), "network", "netif init");
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), "network", "event loop");
    station_netif = esp_netif_create_default_wifi_sta();
    ap_netif = esp_netif_create_default_wifi_ap();
    if (!station_netif || !ap_netif)
        return ESP_ERR_NO_MEM;
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&init), "network", "Wi-Fi init");
    ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), "network", "Wi-Fi storage");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, event, NULL),
                        "network", "IP handler");
    ESP_RETURN_ON_ERROR(mdns_init(), "network", "mDNS init");
    mdns_hostname_set("cyd");
    mdns_instance_name_set("CYD OS");
    mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
    if (xTaskCreate(worker, "connections", 12288, NULL, 3, NULL) != pdPASS)
        return ESP_ERR_NO_MEM;
    atomic_store(&service_started, true);
    return ESP_OK;
}

esp_err_t cyd_network_init(void) {
    esp_err_t e = initialize();
    if (e && guard)
        message(CYD_NET_FAILED, "Wi-Fi service unavailable; restart or inspect startup logs");
    return e;
}

#endif
