#include "cyd.h"
#include "cyd_device.h"
#include "cyd_network.h"
#include "cyd_ota.h"
#include "cyd_access.h"
#include "cyd_projects.h"
cyd_device_status_t preview_device = {
    .brightness = 80, .effective_brightness = 80, .dim_percent = 10, .clock_valid = true, .clock = "2026-11-14 10:09", .message = "Ready"};
esp_err_t cyd_runtime_stop(void) {
    return 0;
}
void cyd_runtime_request_stop(void) {
    cyd_ui_show_app(false);
}
bool cyd_storage_ready(void) {
    return true;
}
uint64_t cyd_storage_free(void) {
    return 32 * 1024 * 1024;
}
cJSON *cyd_logs(void) {
    return cJSON_Parse("[\"Ready for wireless development\"]");
}
esp_err_t cyd_run_revision(const char *rev) {
    (void)rev;
    return 0;
}
void cyd_network_address(char *out, size_t n) {
    snprintf(out, n, "192.168.4.1");
}
void cyd_network_snapshot(cyd_net_status_t *s) {
    memset(s, 0, sizeof(*s));
    s->ready = true;
    s->state = CYD_NET_CONNECTED;
    s->station_enabled = true;
    s->ap_enabled = true;
    strcpy(s->ssid, "Phone hotspot");
    strcpy(s->station_ip, "192.168.1.42");
    strcpy(s->ap_ssid, "CYDAP");
    strcpy(s->ap_ip, "192.168.4.1");
    strcpy(s->message, "Connected");
}
esp_err_t cyd_network_scan(void) {
    return 0;
}
esp_err_t cyd_network_reconnect(void) {
    return 0;
}
esp_err_t cyd_network_disconnect(void) {
    return 0;
}
esp_err_t cyd_network_join(const char *s, const char *p) {
    (void)s;
    (void)p;
    return 0;
}
esp_err_t cyd_network_join_saved(const char *s) {
    (void)s;
    return 0;
}
esp_err_t cyd_network_forget(const char *s) {
    (void)s;
    return 0;
}
esp_err_t cyd_network_set_ap(const char *s, const char *p, bool a, bool o) {
    (void)s;
    (void)p;
    (void)a;
    (void)o;
    return 0;
}
void cyd_network_ap_credentials(char *s, size_t sn, char *p, size_t pn) {
    snprintf(s, sn, "CYDAP");
    snprintf(p, pn, "demo-password");
}
void cyd_ota_health_heartbeat(unsigned b) {
    (void)b;
}
void cyd_ota_snapshot(cyd_ota_status_t *s) {
    memset(s, 0, sizeof(*s));
    s->boot_confirmed = true;
    s->stage = CYD_OS_IDLE;
    strcpy(s->version, "0.3.0");
    strcpy(s->message, "Startup confirmed. Upload OS updates in Safari.");
}
esp_err_t cyd_ota_cancel(void) {
    return 0;
}
esp_err_t cyd_ota_reboot(void) {
    return 0;
}
esp_err_t cyd_ota_rollback(void) {
    return 0;
}
void cyd_access_get_token(char *out, size_t n) {
    snprintf(out, n, "demo-token-only-not-a-real-device-token");
}
esp_err_t cyd_access_rotate(void) {
    return 0;
}
void cyd_device_snapshot(cyd_device_status_t *s) {
    *s = preview_device;
}
esp_err_t cyd_device_brightness(unsigned p, bool save) {
    (void)save;
    preview_device.brightness = p;
    return 0;
}
esp_err_t cyd_device_rotation(unsigned r) {
    preview_device.rotation = r;
    preview_device.mode = CYD_DISPLAY_ROTATION_PREVIEW;
    preview_device.seconds_left = 15;
    return cyd_ui_apply_rotation(r);
}
esp_err_t cyd_device_confirm(void) {
    preview_device.mode = CYD_DISPLAY_NORMAL;
    return 0;
}
esp_err_t cyd_device_revert(void) {
    preview_device.mode = CYD_DISPLAY_NORMAL;
    return 0;
}
esp_err_t cyd_device_calibrate(void) {
    preview_device.mode = CYD_DISPLAY_CALIBRATING;
    preview_device.target_x = 24;
    preview_device.target_y = 24;
    preview_device.seconds_left = 60;
    return 0;
}
esp_err_t cyd_device_reset_display(void) {
    return 0;
}
esp_err_t cyd_device_set_local_clock(const char *s) {
    (void)s;
    return 0;
}
esp_err_t cyd_device_set_offset(int n) {
    preview_device.utc_offset_minutes = n;
    return 0;
}
cJSON *cyd_projects_catalog(void) {
    return cJSON_Parse(
        "{\"folders\":[{\"id\":\"1234abcd\",\"name\":\"Tools\"}],\"projects\":[{\"package_id\":"
        "\"org.example.hello\",\"name\":\"Hello\",\"folder\":\"1234abcd\",\"versions\":[{"
        "\"version\":\"1.1\",\"revision\":\"1111111111111111\"},{\"version\":\"1.2\",\"revision\":"
        "\"2222222222222222\"}]}]}");
}
esp_err_t cyd_project_folder_create(const char *s, char id[9]) {
    (void)s;
    snprintf(id, 9, "abcd1234");
    return 0;
}
esp_err_t cyd_project_folder_rename(const char *a, const char *b) {
    (void)a;
    (void)b;
    return 0;
}
esp_err_t cyd_project_folder_delete(const char *a) {
    (void)a;
    return 0;
}
esp_err_t cyd_project_move(const char *a, const char *b) {
    (void)a;
    (void)b;
    return 0;
}
esp_err_t cyd_board_set_brightness(unsigned p) {
    (void)p;
    return 0;
}
void cyd_board_touch(lv_indev_t *i, lv_indev_data_t *d) {
    (void)i;
    d->state = LV_INDEV_STATE_RELEASED;
}

esp_err_t cyd_device_preferences(unsigned s, unsigned p, bool h) {
    preview_device.dim_seconds=s; preview_device.dim_percent=p; preview_device.clock_12h=h; return ESP_OK;
}

#include "cyd_peer.h"
void cyd_peer_snapshot(cyd_peer_info_t *out) {
    *out = (cyd_peer_info_t){.role = CYD_LINK_COMPANION, .echo_result = -1,
        .message = "Local operation; board link disabled pending qualified wiring"};
}
