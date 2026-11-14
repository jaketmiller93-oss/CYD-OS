#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "cJSON.h"
#include "lvgl.h"
#define CYD_ROOT "/sdcard/cydos"
#define CYD_MAX_PACKAGE (512 * 1024)
#define CYD_MAX_FILES 48
#define CYD_MAX_PATH 96
#define CYD_MAX_SCRIPT (64 * 1024)
#define CYD_VERSION "0.3.2"
const char *cyd_system_version(void);
bool cyd_maintenance_active(void);
bool cyd_maintenance_begin(void);
void cyd_maintenance_end(void);
typedef struct {
    char id[65], name[65], version[33], entry[CYD_MAX_PATH];
    bool storage;
} cyd_manifest_t;
typedef struct {
    char name[CYD_MAX_PATH];
    const uint8_t *data;
    size_t size;
} cyd_zip_file_t;
typedef struct {
    cyd_zip_file_t files[CYD_MAX_FILES];
    unsigned count;
} cyd_zip_t;
/* ZIP parser is portable and returns 0 on success. Only stored ZIP is supported. */
int cyd_zip_parse(const uint8_t *data, size_t size, cyd_zip_t *out);
bool cyd_safe_path(const char *path);
bool cyd_safe_id(const char *id);
uint32_t cyd_crc32(const uint8_t *,size_t);
cJSON *cyd_json_parse(const void *,size_t,size_t);
esp_err_t cyd_board_init(void);
esp_err_t cyd_board_set_brightness(unsigned percent);
esp_err_t cyd_ui_apply_rotation(unsigned rotation);
unsigned cyd_board_width(void);
unsigned cyd_board_height(void);
const char *cyd_board_model(void);
esp_err_t cyd_board_sd_mount(void);
void cyd_board_touch(lv_indev_t *, lv_indev_data_t *);
void cyd_board_flush(lv_display_t *, const lv_area_t *, uint8_t *);
esp_err_t cyd_storage_init(void);
bool cyd_storage_ready(void);
esp_err_t cyd_write_file(const char *, const void *, size_t);
esp_err_t cyd_read_file(const char *, uint8_t **, size_t *, size_t);
esp_err_t cyd_mkdirs(const char *);
void cyd_remove_tree(const char *);
uint64_t cyd_storage_free(void);
esp_err_t cyd_manager_init(void);
esp_err_t cyd_install(const uint8_t *, size_t, char *, size_t, char *, size_t);
esp_err_t cyd_run_revision(const char *revision);
esp_err_t cyd_delete_revision(const char *revision);
cJSON *cyd_versions(void);
esp_err_t cyd_manifest_parse(const void *, size_t, cyd_manifest_t *);
esp_err_t cyd_runtime_init(void);
esp_err_t cyd_runtime_start(const char *, const char *, const cyd_manifest_t *);
esp_err_t cyd_runtime_stop(void);
void cyd_runtime_request_stop(void);
void cyd_runtime_event(int ref);
cJSON *cyd_runtime_status(void);
void cyd_log(const char *context, const char *message);
cJSON *cyd_logs(void);
esp_err_t cyd_ui_init(void);
void cyd_ui_lock(void);
void cyd_ui_unlock(void);
lv_obj_t *cyd_ui_app_root(void);
void cyd_ui_show_app(bool);
esp_err_t cyd_network_init(void);
void cyd_network_address(char *, size_t);
bool cyd_network_connected(void);
esp_err_t cyd_web_start(void);
