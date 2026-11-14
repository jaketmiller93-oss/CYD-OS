#pragma once
#include "fake_sdk.h"
#include "cJSON.h"
#define CYD_ROOT "/tmp/cyd-catalog-test"
#define CYD_MAX_PATH 128
#define CYD_MAX_FILES 64
#define CYD_MAX_PACKAGE (512 * 1024)
typedef struct {
    char name[CYD_MAX_PATH];
    const uint8_t *data;
    size_t size;
} cyd_zip_file_t;
typedef struct {
    cyd_zip_file_t files[CYD_MAX_FILES];
    unsigned count;
} cyd_zip_t;
bool cyd_safe_id(const char *);
bool cyd_safe_path(const char *);
uint32_t cyd_crc32(const uint8_t *, size_t);
cJSON *cyd_json_parse(const void *, size_t, size_t);
bool cyd_maintenance_begin(void);
void cyd_maintenance_end(void);
bool cyd_maintenance_active(void);
const char *cyd_system_version(void);
esp_err_t cyd_runtime_stop(void);
void cyd_log(const char *, const char *);
bool cyd_storage_ready(void);
esp_err_t cyd_read_file(const char *, uint8_t **, size_t *, size_t);
esp_err_t cyd_write_file(const char *, const void *, size_t);
cJSON *cyd_versions(void);
#define CYD_MAX_SCRIPT (256 * 1024)
#ifdef CYD_UI_PREVIEW
#include "lvgl.h"
esp_err_t cyd_ui_init(void),cyd_ui_apply_rotation(unsigned),cyd_board_set_brightness(unsigned),cyd_run_revision(const char *);
void cyd_ui_lock(void),cyd_ui_unlock(void),cyd_ui_show_app(bool),cyd_runtime_request_stop(void);
lv_obj_t *cyd_ui_app_root(void);
unsigned cyd_board_width(void),cyd_board_height(void);
void cyd_board_flush(lv_display_t *,const lv_area_t *,uint8_t *),cyd_board_touch(lv_indev_t *,lv_indev_data_t *),cyd_network_address(char *,size_t);
cJSON *cyd_logs(void);uint64_t cyd_storage_free(void);
#endif
