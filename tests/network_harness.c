/* Native test double for radio, NVS and task scheduling. No real hardware calls. */
#include "network_sdk/fake_sdk.h"
#include "../components/network/network.c"
const char mock_ip_event[] = "IP_EVENT";
static struct {
    size_t size;
    unsigned head, count;
    unsigned char data[16][512];
} queue;
static unsigned char committed[2048], pending[2048];
static size_t committed_size, pending_size;
static bool commit_fail, associated, radio_started, delay_disconnect;
static int64_t mock_clock;
static esp_netif_t mock_sta = {.id = 0}, mock_ap = {.id = 1};
static wifi_mode_t mock_mode;
static wifi_config_t mock_station, mock_access_point;
static wifi_ap_record_t mock_association;
static unsigned attempts;
SemaphoreHandle_t xSemaphoreCreateMutex(void) {
    return (void *)1;
}
int xSemaphoreTake(SemaphoreHandle_t m, unsigned t) {
    (void)m;
    (void)t;
    return pdTRUE;
}
int xSemaphoreGive(SemaphoreHandle_t m) {
    (void)m;
    return pdTRUE;
}
QueueHandle_t xQueueCreate(unsigned n, size_t size) {
    assert(n == 16 && size <= 512);
    memset(&queue, 0, sizeof(queue));
    queue.size = size;
    return &queue;
}
int xQueueSend(QueueHandle_t q, const void *p, unsigned t) {
    (void)q;
    (void)t;
    if (queue.count == 16)
        return 0;
    memcpy(queue.data[(queue.head + queue.count) % 16], p, queue.size);
    queue.count++;
    return pdTRUE;
}
int xQueueReceive(QueueHandle_t q, void *p, unsigned t) {
    (void)q;
    (void)t;
    if (!queue.count)
        return 0;
    memcpy(p, queue.data[queue.head], queue.size);
    queue.head = (queue.head + 1) % 16;
    queue.count--;
    return pdTRUE;
}
int xTaskCreate(void (*fn)(void *), const char *name, unsigned stack, void *a, unsigned pri,
                void *h) {
    (void)fn;
    (void)name;
    (void)stack;
    (void)a;
    (void)pri;
    (void)h;
    return pdPASS;
}
void vTaskDelay(unsigned ticks) {
    mock_clock += ticks;
}
int64_t esp_timer_get_time(void) {
    return mock_clock * 1000;
}
uint32_t esp_random(void) {
    return 0xa1b2c3d4;
}
void cyd_log(const char *context, const char *message) {
    (void)context;
    assert(!strstr(message, "home-password"));
}
esp_err_t esp_netif_init(void) {
    return ESP_OK;
}
esp_netif_t *esp_netif_create_default_wifi_sta(void) {
    return &mock_sta;
}
esp_netif_t *esp_netif_create_default_wifi_ap(void) {
    mock_ap.info.ip.addr = 0x0104a8c0;
    return &mock_ap;
}
esp_err_t esp_netif_get_ip_info(esp_netif_t *n, esp_netif_ip_info_t *out) {
    *out = n->info;
    return ESP_OK;
}
esp_err_t esp_event_loop_create_default(void) {
    return ESP_OK;
}
esp_err_t esp_event_handler_register(esp_event_base_t b, int i,
                                     void (*f)(void *, esp_event_base_t, int32_t, void *),
                                     void *a) {
    (void)b;
    (void)i;
    (void)f;
    (void)a;
    return ESP_OK;
}
esp_err_t esp_wifi_set_band_mode(int mode) {
    assert(radio_started);
    assert(mode == WIFI_BAND_MODE_AUTO || mode == WIFI_BAND_MODE_2G_ONLY);
    return ESP_OK;
}
esp_err_t esp_wifi_init(const wifi_init_config_t *i) {
    (void)i;
    return ESP_OK;
}
esp_err_t esp_wifi_set_storage(int s) {
    (void)s;
    return ESP_OK;
}
esp_err_t esp_wifi_set_mode(wifi_mode_t m) {
    mock_mode = m;
    return ESP_OK;
}
esp_err_t esp_wifi_set_config(int iface, const wifi_config_t *c) {
    if (iface == WIFI_IF_STA)
        mock_station = *c;
    else
        mock_access_point = *c;
    return ESP_OK;
}
esp_err_t esp_wifi_start(void) {
    radio_started = true;
    return ESP_OK;
}
esp_err_t esp_wifi_stop(void) {
    radio_started = false;
    associated = false;
    mock_sta.info.ip.addr = 0;
    return ESP_OK;
}
esp_err_t esp_wifi_connect(void) {
    assert(radio_started && (mock_mode == WIFI_MODE_STA || mock_mode == WIFI_MODE_APSTA));
    attempts++;
    return ESP_OK;
}
esp_err_t esp_wifi_disconnect(void) {
    if (delay_disconnect)
        return ESP_OK;
    associated = false;
    mock_sta.info.ip.addr = 0;
    return ESP_OK;
}
esp_err_t esp_wifi_sta_get_ap_info(wifi_ap_record_t *a) {
    if (!associated)
        return ESP_FAIL;
    *a = mock_association;
    return ESP_OK;
}
esp_err_t esp_wifi_scan_start(const wifi_scan_config_t *c, bool block) {
    (void)c;
    assert(block);
    assert(mock_mode == WIFI_MODE_STA || mock_mode == WIFI_MODE_APSTA);
    return ESP_OK;
}
esp_err_t esp_wifi_scan_get_ap_records(uint16_t *n, wifi_ap_record_t *out) {
    const wifi_ap_record_t records[] = {
        {.ssid = "Home", .rssi = -30, .authmode = WIFI_AUTH_WPA2_PSK},
        {.ssid = "Phone", .rssi = -40, .authmode = WIFI_AUTH_WPA2_PSK},
        {.ssid = "Home", .rssi = -60, .authmode = WIFI_AUTH_WPA2_PSK},
        {.ssid = "Cafe", .rssi = -70, .authmode = WIFI_AUTH_OPEN}};
    unsigned count = *n < 4 ? *n : 4;
    memcpy(out, records, count * sizeof(*out));
    *n = count;
    return ESP_OK;
}
esp_err_t esp_wifi_clear_ap_list(void) {
    return ESP_OK;
}
esp_err_t nvs_open(const char *ns, int mode, nvs_handle_t *h) {
    (void)ns;
    (void)mode;
    *h = 1;
    return ESP_OK;
}
esp_err_t nvs_get_blob(nvs_handle_t h, const char *k, void *out, size_t *n) {
    (void)h;
    (void)k;
    if (!committed_size)
        return ESP_ERR_NOT_FOUND;
    if (*n < committed_size)
        return ESP_FAIL;
    memcpy(out, committed, committed_size);
    *n = committed_size;
    return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t h, const char *k, const void *p, size_t n) {
    (void)h;
    (void)k;
    assert(n <= sizeof(pending));
    memcpy(pending, p, n);
    pending_size = n;
    return ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t h) {
    (void)h;
    if (commit_fail)
        return ESP_FAIL;
    memcpy(committed, pending, pending_size);
    committed_size = pending_size;
    return ESP_OK;
}
esp_err_t mdns_init(void) {
    return ESP_OK;
}
esp_err_t mdns_hostname_set(const char *s) {
    (void)s;
    return ESP_OK;
}
esp_err_t mdns_instance_name_set(const char *s) {
    (void)s;
    return ESP_OK;
}
esp_err_t mdns_service_add(const char *a, const char *b, const char *c, int p, void *d, size_t n) {
    (void)a;
    (void)b;
    (void)c;
    (void)p;
    (void)d;
    (void)n;
    return ESP_OK;
}
void host_pump(void) {
    job_t j;
    unsigned count = 0;
    while (xQueueReceive(jobs, &j, 0)) {
        assert(count++ < 100);
        process(&j);
        clear_private(&j, sizeof(j));
    }
}
int host_boot(bool retain) {
    if (!retain) {
        committed_size = pending_size = 0;
        memset(committed, 0, sizeof(committed));
    }
    commit_fail = false;
    delay_disconnect = false;
    associated = false;
    radio_started = false;
    mock_clock = 0;
    attempts = 0;
    memset(&config, 0, sizeof(config));
    memset(&status, 0, sizeof(status));
    memset(&candidate, 0, sizeof(candidate));
    memset(&mock_station, 0, sizeof(mock_station));
    connecting = manual_join = driver_started = false;
    atomic_store(&service_started, false);
    guard = NULL;
    jobs = NULL;
    esp_err_t e = cyd_network_init();
    if (e)
        return e;
    e = configure_radio();
    next_profile = config.preferred;
    next_retry = 0;
    return e;
}
void host_tick(unsigned ms) {
    mock_clock += ms;
    maintain_connection();
}
void host_ip(const char *ssid) {
    associated = true;
    memset(&mock_association, 0, sizeof(mock_association));
    snprintf((char *)mock_association.ssid, 33, "%s", ssid);
    mock_sta.info.ip.addr = 0x6401a8c0;
    ip_event_got_ip_t got = {.esp_netif = &mock_sta, .ip_info = mock_sta.info};
    event(NULL, IP_EVENT, IP_EVENT_STA_GOT_IP, &got);
    host_pump();
}
void host_drop(void) {
    associated = false;
    mock_sta.info.ip.addr = 0;
    maintain_connection();
}
void host_config(cyd_wifi_config_t *out) {
    *out = config;
}
void host_fail_commit(bool fail) {
    commit_fail = fail;
}
unsigned host_attempts(void) {
    return attempts;
}
int host_mode(void) {
    return mock_mode;
}
size_t host_config_size(void) {
    return sizeof(config);
}
size_t host_status_size(void) {
    return sizeof(status);
}

void host_delayed_disconnect(bool delayed) {
    delay_disconnect = delayed;
}
void host_queue_ip(const char *ssid, uint32_t address) {
    associated = true;
    memset(&mock_association, 0, sizeof(mock_association));
    snprintf((char *)mock_association.ssid, 33, "%s", ssid);
    mock_sta.info.ip.addr = address;
    ip_event_got_ip_t got = {.esp_netif = &mock_sta, .ip_info = mock_sta.info};
    event(NULL, IP_EVENT, IP_EVENT_STA_GOT_IP, &got);
}

bool cyd_maintenance_active(void) {
    return false;
}
void cyd_ota_health_heartbeat(unsigned bit) {
    (void)bit;
}
const char *cyd_system_version(void) {
    return "0.3.0";
}
