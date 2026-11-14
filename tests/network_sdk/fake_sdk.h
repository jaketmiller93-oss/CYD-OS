#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_NO_MEM 0x101
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_TIMEOUT 0x107
#define ESP_ERR_NOT_FOUND 0x105
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
#define ESP_RETURN_ON_ERROR(call, tag, msg)                                                        \
    do {                                                                                           \
        esp_err_t result = (call);                                                                 \
        if (result)                                                                                \
            return result;                                                                         \
    } while (0)
#define CONFIG_SOC_WIFI_SUPPORT_5G 1
#define WIFI_BAND_MODE_AUTO 3
#define WIFI_BAND_MODE_2G_ONLY 1
esp_err_t esp_wifi_set_band_mode(int mode);
#define CONFIG_CYD_WIFI_SSID "Home"
#define CONFIG_CYD_WIFI_PASSWORD "home-password"
#define CONFIG_CYD_AP_PASSWORD ""
#define portMAX_DELAY 0xffffffffu
#define pdTRUE 1
#define pdPASS 1
#define pdMS_TO_TICKS(ms) (ms)
typedef void *SemaphoreHandle_t;
typedef void *QueueHandle_t;
SemaphoreHandle_t xSemaphoreCreateMutex(void);
int xSemaphoreTake(SemaphoreHandle_t, unsigned);
int xSemaphoreGive(SemaphoreHandle_t);
QueueHandle_t xQueueCreate(unsigned, size_t);
int xQueueSend(QueueHandle_t, const void *, unsigned);
int xQueueReceive(QueueHandle_t, void *, unsigned);
int xTaskCreate(void (*)(void *), const char *, unsigned, void *, unsigned, void *);
void vTaskDelay(unsigned);
int64_t esp_timer_get_time(void);
uint32_t esp_random(void);
void cyd_log(const char *, const char *);
typedef const char *esp_event_base_t;
extern const char mock_ip_event[];
#define IP_EVENT mock_ip_event
#define IP_EVENT_STA_GOT_IP 1
#define IP_EVENT_STA_LOST_IP 2
#define ESP_EVENT_ANY_ID -1
typedef struct {
    uint32_t addr;
} esp_ip4_addr_t;
typedef struct {
    esp_ip4_addr_t ip;
} esp_netif_ip_info_t;
typedef struct {
    int id;
    esp_netif_ip_info_t info;
} esp_netif_t;
typedef struct {
    esp_netif_t *esp_netif;
    esp_netif_ip_info_t ip_info;
} ip_event_got_ip_t;
#define IPSTR "%u.%u.%u.%u"
#define IP2STR(ip)                                                                                 \
    (unsigned)((ip)->addr & 255), (unsigned)(((ip)->addr >> 8) & 255),                             \
        (unsigned)(((ip)->addr >> 16) & 255), (unsigned)(((ip)->addr >> 24) & 255)
esp_err_t esp_netif_init(void);
esp_netif_t *esp_netif_create_default_wifi_sta(void);
esp_netif_t *esp_netif_create_default_wifi_ap(void);
esp_err_t esp_netif_get_ip_info(esp_netif_t *, esp_netif_ip_info_t *);
esp_err_t esp_event_loop_create_default(void);
esp_err_t esp_event_handler_register(esp_event_base_t, int,
                                     void (*)(void *, esp_event_base_t, int32_t, void *), void *);
typedef enum { WIFI_MODE_NULL, WIFI_MODE_STA, WIFI_MODE_AP, WIFI_MODE_APSTA } wifi_mode_t;
#define WIFI_IF_STA 0
#define WIFI_IF_AP 1
#define WIFI_STORAGE_RAM 0
#define WIFI_ALL_CHANNEL_SCAN 1
#define WIFI_AUTH_OPEN 0
#define WIFI_AUTH_WPA_PSK 1
#define WIFI_AUTH_WPA2_PSK 2
#define WIFI_AUTH_WPA_WPA2_PSK 3
#define WIFI_AUTH_WPA3_PSK 4
#define WIFI_AUTH_WPA2_WPA3_PSK 5
typedef struct {
    uint8_t ssid[33];
    int8_t rssi;
    int authmode;
} wifi_ap_record_t;
typedef struct {
    bool show_hidden;
} wifi_scan_config_t;
typedef struct {
    struct {
        uint8_t ssid[32], password[64];
        int scan_method;
        struct {
            bool capable;
        } pmf_cfg;
    } sta;
    struct {
        uint8_t ssid[32], password[64];
        int ssid_len, authmode, max_connection, channel;
    } ap;
} wifi_config_t;
typedef struct {
    int unused;
} wifi_init_config_t;
#define WIFI_INIT_CONFIG_DEFAULT() ((wifi_init_config_t){0})
esp_err_t esp_wifi_init(const wifi_init_config_t *);
esp_err_t esp_wifi_set_storage(int);
esp_err_t esp_wifi_set_mode(wifi_mode_t);
esp_err_t esp_wifi_set_config(int, const wifi_config_t *);
esp_err_t esp_wifi_start(void);
esp_err_t esp_wifi_stop(void);
esp_err_t esp_wifi_connect(void);
esp_err_t esp_wifi_disconnect(void);
esp_err_t esp_wifi_sta_get_ap_info(wifi_ap_record_t *);
esp_err_t esp_wifi_scan_start(const wifi_scan_config_t *, bool);
esp_err_t esp_wifi_scan_get_ap_records(uint16_t *, wifi_ap_record_t *);
esp_err_t esp_wifi_clear_ap_list(void);
typedef int nvs_handle_t;
#define NVS_READWRITE 1
esp_err_t nvs_open(const char *, int, nvs_handle_t *);
esp_err_t nvs_get_blob(nvs_handle_t, const char *, void *, size_t *);
esp_err_t nvs_set_blob(nvs_handle_t, const char *, const void *, size_t);
esp_err_t nvs_commit(nvs_handle_t);
esp_err_t mdns_init(void);
esp_err_t mdns_hostname_set(const char *);
esp_err_t mdns_instance_name_set(const char *);
esp_err_t mdns_service_add(const char *, const char *, const char *, int, void *, size_t);

bool cyd_maintenance_active(void);
const char *cyd_system_version(void);
