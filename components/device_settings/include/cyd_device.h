#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef enum {
    CYD_DISPLAY_NORMAL,
    CYD_DISPLAY_ROTATION_PREVIEW,
    CYD_DISPLAY_CALIBRATING,
    CYD_DISPLAY_CALIBRATION_PREVIEW
} cyd_display_mode_t;
typedef struct {
    unsigned brightness, rotation;
    int utc_offset_minutes;
    bool clock_valid;
    cyd_display_mode_t mode;
    unsigned seconds_left, target_index;
    int target_x, target_y;
    char clock[24], message[128];
} cyd_device_status_t;
esp_err_t cyd_device_init(unsigned width, unsigned height, esp_err_t (*backlight)(unsigned),
                          esp_err_t (*rotation)(unsigned));
void cyd_device_clock_start(void);
void cyd_device_snapshot(cyd_device_status_t *out);
esp_err_t cyd_device_brightness(unsigned percent, bool save);
esp_err_t cyd_device_rotation(unsigned rotation);
esp_err_t cyd_device_confirm(void);
esp_err_t cyd_device_revert(void);
esp_err_t cyd_device_calibrate(void);
esp_err_t cyd_device_reset_display(void);
esp_err_t cyd_device_set_clock(int64_t epoch, int offset_minutes);
esp_err_t cyd_device_set_local_clock(const char *text);
esp_err_t cyd_device_set_offset(int minutes);
/* Driver calls this once per read. Returns true when calibration consumes touch. */
bool cyd_device_touch(int raw_x, int raw_y, bool pressed, int *x, int *y);
