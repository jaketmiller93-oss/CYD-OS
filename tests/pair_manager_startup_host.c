/* Actual manager init with allocation/NVS/task failure doubles. */
#include "cyd.h"
cJSON *cyd_logs(void);
esp_err_t cyd_write_file(const char *, const void *, size_t);
#include "../components/paired_system/manager.c"
#include <assert.h>
#include <setjmp.h>
static bool reject_mutex, reject_task;
static esp_err_t open_error;
static unsigned opens, closes, deletes, tasks, writes;
static char last_log[160];
static bool boot_ok = true, peer_present, probe;
static unsigned snapshots, remote_calls;
static jmp_buf collection_step;
bool cyd_ota_boot_confirmed(void) { return boot_ok; }
SemaphoreHandle_t xSemaphoreCreateMutex(void) { return reject_mutex ? NULL : (void *)1; }
int xSemaphoreTake(SemaphoreHandle_t h, unsigned t) { (void)t; assert(h); return pdTRUE; }
int xSemaphoreGive(SemaphoreHandle_t h) { assert(h); return pdTRUE; }
void vSemaphoreDelete(SemaphoreHandle_t h) { assert(h); deletes++; }
int xTaskCreate(void (*fn)(void *), const char *name, unsigned stack, void *arg,
                unsigned priority, void *handle) {
    (void)arg; (void)priority; (void)handle;
    assert(fn == collect && !strcmp(name, "device_reports") && stack == 4096);
    tasks++; return reject_task ? 0 : pdPASS;
}
void vTaskDelay(unsigned ms) {
    assert(ms == (boot_ok ? 30000u : 250u));
    if (probe) longjmp(collection_step, 1);
}
void vTaskDelete(void *p) { (void)p; }
void cyd_log(const char *tag, const char *message) {
    assert(!strcmp(tag, "pair")); snprintf(last_log, sizeof(last_log), "%s", message);
}
esp_err_t nvs_open(const char *name, int mode, nvs_handle_t *handle) {
    assert(!strcmp(name, "cyd_pairprefs") && mode == NVS_READWRITE); opens++;
    if (open_error) return open_error;
    *handle = 1; return ESP_OK;
}
esp_err_t nvs_get_str(nvs_handle_t h, const char *key, char *value, size_t *n) {
    assert(h == 1 && !strcmp(key, "host_display") && *n == 512);
    (void)value; return ESP_ERR_NVS_NOT_FOUND;
}
esp_err_t nvs_set_str(nvs_handle_t h, const char *key, const char *value) {
    (void)h; (void)key; (void)value; writes++; return ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t h) { (void)h; writes++; return ESP_OK; }
void nvs_close(nvs_handle_t h) { assert(h == 1); closes++; }
bool cyd_maintenance_active(void) { return false; }
bool cyd_storage_ready(void) { return false; }
esp_err_t cyd_mkdirs(const char *p) { (void)p; return ESP_OK; }
esp_err_t cyd_write_file(const char *p, const void *data, size_t n) {
    (void)p; (void)data; (void)n; return ESP_OK;
}
void cyd_peer_snapshot(cyd_peer_info_t *p) {
    snapshots++; memset(p, 0, sizeof(*p)); p->connected = peer_present;
}
cJSON *cyd_pair_json(unsigned op, const cJSON *o) { (void)op; (void)o; remote_calls++; return NULL; }
cJSON *cyd_pair_status(void) { return cJSON_CreateObject(); }
cJSON *cyd_logs(void) { return cJSON_CreateArray(); }
void cyd_device_snapshot(cyd_device_status_t *s) { memset(s, 0, sizeof(*s)); }
esp_err_t cyd_device_set_clock(int64_t t, int offset) { (void)t; (void)offset; return ESP_OK; }
bool cyd_pair_number(const cJSON *o, const char *key, int64_t lo, int64_t hi, int64_t *n) {
    (void)o; (void)key; (void)lo; (void)hi; (void)n; return false;
}
int main(void) {
    reject_mutex = true;
    assert(cyd_pair_manager_start() == ESP_ERR_NO_MEM && !opens && !tasks);
    assert(strstr(last_log, "settings mutex"));
    reject_mutex = false; open_error = ESP_ERR_NVS_NOT_ENOUGH_SPACE;
    assert(cyd_pair_manager_start() == ESP_ERR_NVS_NOT_ENOUGH_SPACE);
    assert(!guard && deletes == 1 && !closes && !tasks && !writes);
    assert(strstr(last_log, "settings NVS (0x1105)"));
    open_error = ESP_OK; reject_task = true;
    assert(cyd_pair_manager_start() == ESP_ERR_NO_MEM);
    assert(!guard && deletes == 2 && closes == 1 && tasks == 1 && !writes);
    assert(strstr(last_log, "diagnostic reports task (4096-byte internal stack)"));
    reject_task = false;
    assert(cyd_pair_manager_start() == ESP_OK);
    assert(guard && tasks == 2 && !host_profile[0] && !writes);
    probe = true;
    for (unsigned phase = 0; phase < 3; phase++) {
        boot_ok = phase > 0; peer_present = phase == 2;
        snapshots = remote_calls = 0;
        if (!setjmp(collection_step)) collect(NULL);
        assert(snapshots == (phase ? 1u : 0u));
        assert(remote_calls == (phase == 2 ? 3u : 0u) && !writes);
    }
    nvs_close(store); vSemaphoreDelete(guard); guard = NULL;
    puts("PASS manager exact NVS error, task failure cleanup and retained settings");
    return 0;
}
