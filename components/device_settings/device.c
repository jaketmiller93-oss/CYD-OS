#include "cyd.h"
#include "cyd_device.h"
#include "display_geometry.h"
#include "sdkconfig.h"
#include "freertos/queue.h"
#include "nvs.h"
#include "esp_timer.h"
#include "esp_netif_sntp.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <stdatomic.h>
typedef struct {
    uint32_t format;
    unsigned brightness, rotation;
    int offset;
    float matrix[6];
} device_config_t;
typedef enum {
    BRIGHTNESS,
    ROTATION,
    CONFIRM,
    REVERT,
    CALIBRATE,
    CALIBRATED,
    RESET_DISPLAY,
    CLOCK,
    OFFSET
} device_job_kind_t;
typedef struct {
    device_job_kind_t kind;
    int value;
    bool save;
    int64_t epoch;
    float matrix[6];
} device_job_t;
static device_config_t config, saved, defaults;
static SemaphoreHandle_t guard;
static QueueHandle_t jobs;
static nvs_handle_t store;
static esp_err_t (*set_backlight)(unsigned), (*set_rotation)(unsigned);
static unsigned native_w, native_h;
static cyd_display_mode_t mode;
static uint32_t deadline, press_started;
static bool owns_maintenance, was_pressed, calibration_posted;
static unsigned target_index, samples;
static double sum_x, sum_y;
static float raw_points[5][2], target_points[5][2];
static char message[128] = "Ready";
static atomic_bool clock_valid;
static uint32_t now(void) {
    return (uint32_t)(esp_timer_get_time() / 1000);
}
static bool config_valid(const device_config_t *c) {
    if (c->format != 1 || c->brightness < 10 || c->brightness > 100 || c->rotation > 3 ||
        c->offset < -720 || c->offset > 840)
        return false;
    for (int i = 0; i < 6; i++)
        if (!isfinite(c->matrix[i]) || fabsf(c->matrix[i]) > 10000)
            return false;
    return fabsf(c->matrix[0] * c->matrix[4] - c->matrix[1] * c->matrix[3]) > 1e-9;
}
static void note(const char *s) {
    xSemaphoreTake(guard, portMAX_DELAY);
    snprintf(message, sizeof(message), "%s", s);
    xSemaphoreGive(guard);
}
static esp_err_t persist(void) {
    device_config_t next;
    xSemaphoreTake(guard, portMAX_DELAY);
    next = config;
    xSemaphoreGive(guard);
    esp_err_t e = nvs_set_blob(store, "display", &next, sizeof(next));
    if (!e)
        e = nvs_commit(store);
    if (!e) {
        xSemaphoreTake(guard, portMAX_DELAY);
        saved = next;
        xSemaphoreGive(guard);
    }
    return e;
}
static esp_err_t post(const device_job_t *j) {
    if (!jobs)
        return ESP_ERR_INVALID_STATE;
    return xQueueSend(jobs, j, pdMS_TO_TICKS(30)) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}
esp_err_t cyd_device_brightness(unsigned p, bool save) {
    if (p < 10 || p > 100 || cyd_maintenance_active())
        return ESP_ERR_INVALID_ARG;
    device_job_t j = {.kind = BRIGHTNESS, .value = p, .save = save};
    return post(&j);
}
esp_err_t cyd_device_rotation(unsigned r) {
    if (r > 3 || cyd_maintenance_active())
        return ESP_ERR_INVALID_STATE;
    device_job_t j = {.kind = ROTATION, .value = r};
    return post(&j);
}
esp_err_t cyd_device_confirm(void) {
    device_job_t j = {.kind = CONFIRM};
    return post(&j);
}
esp_err_t cyd_device_revert(void) {
    device_job_t j = {.kind = REVERT};
    return post(&j);
}
esp_err_t cyd_device_calibrate(void) {
    if (cyd_maintenance_active())
        return ESP_ERR_INVALID_STATE;
    device_job_t j = {.kind = CALIBRATE};
    return post(&j);
}
esp_err_t cyd_device_reset_display(void) {
    device_job_t j = {.kind = RESET_DISPLAY};
    return post(&j);
}
esp_err_t cyd_device_set_clock(int64_t epoch, int offset) {
    if (epoch < 1704067200 || epoch > 4102444799LL || offset < -720 || offset > 840 ||
        cyd_maintenance_active())
        return ESP_ERR_INVALID_ARG;
    device_job_t j = {.kind = CLOCK, .epoch = epoch, .value = offset};
    return post(&j);
}
esp_err_t cyd_device_set_offset(int minutes) {
    if (minutes < -720 || minutes > 840 || cyd_maintenance_active())
        return ESP_ERR_INVALID_ARG;
    device_job_t j = {.kind = OFFSET, .value = minutes};
    return post(&j);
}
esp_err_t cyd_device_set_local_clock(const char *text) {
    int offset;
    xSemaphoreTake(guard, portMAX_DELAY);
    offset = config.offset;
    xSemaphoreGive(guard);
    int64_t epoch;
    if (!cyd_clock_parse_local(text, offset, &epoch))
        return ESP_ERR_INVALID_ARG;
    return cyd_device_set_clock(epoch, offset);
}
void cyd_device_snapshot(cyd_device_status_t *out) {
    memset(out, 0, sizeof(*out));
    if (!guard)
        return;
    xSemaphoreTake(guard, portMAX_DELAY);
    out->brightness = config.brightness;
    out->rotation = config.rotation;
    out->utc_offset_minutes = config.offset;
    out->mode = mode;
    out->target_index = target_index;
    out->seconds_left =
        mode == CYD_DISPLAY_NORMAL
            ? 0
            : ((int32_t)(deadline - now()) > 0 ? (deadline - now() + 999) / 1000 : 0);
    if (target_index < 5) {
        int x, y;
        cyd_display_to_logical(config.rotation, native_w, native_h,
                               (int)target_points[target_index][0],
                               (int)target_points[target_index][1], &x, &y);
        out->target_x = x;
        out->target_y = y;
    }
    snprintf(out->message, sizeof(out->message), "%s", message);
    xSemaphoreGive(guard);
    out->clock_valid = atomic_load(&clock_valid);
    time_t stamp = time(NULL) + (time_t)out->utc_offset_minutes * 60;
    struct tm t;
    if (out->clock_valid && gmtime_r(&stamp, &t))
        strftime(out->clock, sizeof(out->clock), "%Y-%m-%d %H:%M", &t);
    else
        snprintf(out->clock, sizeof(out->clock), "Time not synchronized");
}
bool cyd_device_touch(int rx, int ry, bool pressed, int *x, int *y) {
    if (!guard) {
        *x = rx;
        *y = ry;
        return false;
    }
    xSemaphoreTake(guard, portMAX_DELAY);
    if (mode == CYD_DISPLAY_CALIBRATING) {
        if (pressed) {
            if (!was_pressed) {
                press_started = now();
                samples = 0;
                sum_x = sum_y = 0;
            }
            if (samples < 128) {
                sum_x += rx;
                sum_y += ry;
                samples++;
            }
            if (now() - press_started > 2000 && !calibration_posted) {
                device_job_t j = {.kind = REVERT};
                post(&j);
                calibration_posted = true;
            }
        } else if (was_pressed && samples && !calibration_posted && target_index < 5) {
            raw_points[target_index][0] = sum_x / samples;
            raw_points[target_index][1] = sum_y / samples;
            target_index++;
            if (target_index == 5) {
                device_job_t j = {.kind = CALIBRATED};
                if (cyd_touch_fit(raw_points, target_points, j.matrix)) {
                    post(&j);
                } else {
                    j.kind = REVERT;
                    post(&j);
                }
                calibration_posted = true;
            }
        }
        was_pressed = pressed;
        xSemaphoreGive(guard);
        return true;
    }
    float px = config.matrix[0] * rx + config.matrix[1] * ry + config.matrix[2],
          py = config.matrix[3] * rx + config.matrix[4] * ry + config.matrix[5];
    xSemaphoreGive(guard);
    *x = (int)lroundf(px);
    *y = (int)lroundf(py);
    if (*x < 0)
        *x = 0;
    if (*y < 0)
        *y = 0;
    if (*x >= (int)native_w)
        *x = native_w - 1;
    if (*y >= (int)native_h)
        *y = native_h - 1;
    return false;
}
static void release_preview(void) {
    xSemaphoreTake(guard, portMAX_DELAY);
    mode = CYD_DISPLAY_NORMAL;
    xSemaphoreGive(guard);
    if (owns_maintenance) {
        owns_maintenance = false;
        cyd_maintenance_end();
    }
}
static void restore(void) {
    device_config_t previous;
    xSemaphoreTake(guard, portMAX_DELAY);
    previous = saved;
    config = saved;
    xSemaphoreGive(guard);
    if (set_rotation)
        set_rotation(previous.rotation);
    if (set_backlight)
        set_backlight(previous.brightness);
    release_preview();
    note("Previous display settings restored");
}
static bool own_preview(void) {
    if (owns_maintenance)
        return true;
    if (!cyd_maintenance_begin())
        return false;
    if (cyd_runtime_stop()) {
        cyd_maintenance_end();
        return false;
    }
    owns_maintenance = true;
    return true;
}
static void apply_job(const device_job_t *j) {
    if (cyd_maintenance_active() && !owns_maintenance)
        return;
    if (owns_maintenance && (j->kind == BRIGHTNESS || j->kind == CLOCK || j->kind == OFFSET))
        return;
    esp_err_t e = ESP_OK;
    switch (j->kind) {
    case BRIGHTNESS:
        if (set_backlight)
            e = set_backlight(j->value);
        if (!e) {
            xSemaphoreTake(guard, portMAX_DELAY);
            config.brightness = j->value;
            xSemaphoreGive(guard);
            if (j->save)
                e = persist();
        }
        break;
    case ROTATION:
        if (!own_preview())
            return;
        xSemaphoreTake(guard, portMAX_DELAY);
        config.rotation = j->value;
        mode = CYD_DISPLAY_ROTATION_PREVIEW;
        deadline = now() + 15000;
        xSemaphoreGive(guard);
        e = set_rotation ? set_rotation(j->value) : ESP_ERR_INVALID_STATE;
        if (e) {
            restore();
            break;
        }
        note("Test orientation, then Keep within 15 seconds");
        break;
    case CONFIRM:
        if (mode != CYD_DISPLAY_ROTATION_PREVIEW && mode != CYD_DISPLAY_CALIBRATION_PREVIEW)
            return;
        e = persist();
        if (!e) {
            release_preview();
            note("Display settings saved");
        }
        break;
    case REVERT:
        if (owns_maintenance)
            restore();
        break;
    case CALIBRATE: {
        if (!own_preview())
            return;
        xSemaphoreTake(guard, portMAX_DELAY);
        unsigned w = config.rotation & 1 ? native_h : native_w,
                 h = config.rotation & 1 ? native_w : native_h;
        int points[5][2] = {{24, 24}, {w - 25, 24}, {w - 25, h - 25}, {24, h - 25}, {w / 2, h / 2}};
        for (int i = 0; i < 5; i++) {
            int x, y;
            cyd_display_to_native(config.rotation, native_w, native_h, points[i][0], points[i][1],
                                  &x, &y);
            target_points[i][0] = x;
            target_points[i][1] = y;
        }
        target_index = 0;
        was_pressed = calibration_posted = false;
        mode = CYD_DISPLAY_CALIBRATING;
        deadline = now() + 60000;
        xSemaphoreGive(guard);
        note("Tap and release each crosshair. Hold anywhere 2 seconds to cancel");
        break;
    }
    case CALIBRATED:
        if (mode != CYD_DISPLAY_CALIBRATING)
            return;
        xSemaphoreTake(guard, portMAX_DELAY);
        memcpy(config.matrix, j->matrix, sizeof(config.matrix));
        mode = CYD_DISPLAY_CALIBRATION_PREVIEW;
        deadline = now() + 15000;
        xSemaphoreGive(guard);
        note("Test touch, then Keep within 15 seconds");
        break;
    case RESET_DISPLAY:
        if (!own_preview())
            return;
        xSemaphoreTake(guard, portMAX_DELAY);
        int offset = config.offset;
        config = defaults;
        config.offset = offset;
        xSemaphoreGive(guard);
        if (set_rotation)
            e = set_rotation(0);
        if (!e && set_backlight)
            e = set_backlight(config.brightness);
        if (!e)
            e = persist();
        release_preview();
        note("Display defaults restored; Wi-Fi and apps kept");
        break;
    case CLOCK: {
        struct timeval tv = {.tv_sec = j->epoch};
        if (settimeofday(&tv, NULL))
            e = ESP_FAIL;
        else {
            atomic_store(&clock_valid, true);
            xSemaphoreTake(guard, portMAX_DELAY);
            config.offset = j->value;
            xSemaphoreGive(guard);
            e = persist();
            note("Clock set");
        }
        break;
    }
    case OFFSET:
        xSemaphoreTake(guard, portMAX_DELAY);
        config.offset = j->value;
        xSemaphoreGive(guard);
        e = persist();
        break;
    }
    if (e)
        note("Could not apply/save the setting; previous saved values retained");
}
static void device_tick(void) {
    if (owns_maintenance && mode != CYD_DISPLAY_NORMAL && (int32_t)(now() - deadline) >= 0)
        restore();
}
static void device_worker(void *arg) {
    (void)arg;
    device_job_t j;
    for (;;) {
        if (xQueueReceive(jobs, &j, pdMS_TO_TICKS(100)) == pdTRUE)
            apply_job(&j);
        device_tick();
    }
}
static void ntp_sync(struct timeval *tv) {
    (void)tv;
    atomic_store(&clock_valid, true);
}
void cyd_device_clock_start(void) {
    esp_sntp_config_t c = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    c.sync_cb = ntp_sync;
    esp_netif_sntp_init(&c);
}
esp_err_t cyd_device_init(unsigned w, unsigned h, esp_err_t (*backlight)(unsigned),
                          esp_err_t (*rotation)(unsigned)) {
    native_w = w;
    native_h = h;
    set_backlight = backlight;
    set_rotation = rotation;
    guard = xSemaphoreCreateMutex();
    jobs = xQueueCreate(8, sizeof(device_job_t));
    if (!guard || !jobs)
        return ESP_ERR_NO_MEM;
    memset(&defaults, 0, sizeof(defaults));
    defaults.format = 1;
    defaults.brightness = 100;
#ifdef CONFIG_IDF_TARGET_ESP32C5
    defaults.matrix[1] = (float)w / CONFIG_CYD_C5_TOUCH_X_SPAN;
    defaults.matrix[2] = -CONFIG_CYD_C5_TOUCH_X_MIN * defaults.matrix[1];
    defaults.matrix[3] = (float)h / CONFIG_CYD_C5_TOUCH_Y_SPAN;
    defaults.matrix[5] = -CONFIG_CYD_C5_TOUCH_Y_MIN * defaults.matrix[3];
#else
    defaults.matrix[0] = defaults.matrix[4] = 1;
#endif
    esp_err_t e = nvs_open("cyd_device", NVS_READWRITE, &store);
    if (e)
        return e;
    size_t n = sizeof(config);
    e = nvs_get_blob(store, "display", &config, &n);
    if (e == ESP_ERR_NVS_NOT_FOUND) {
        config = saved = defaults;
        /* Preserve the USB bootstrap build's working calibration before a generic OTA image
         * arrives. */
        e = persist();
        if (e)
            return e;
    } else if (e || n != sizeof(config) || !config_valid(&config)) {
        return ESP_ERR_INVALID_STATE;
    } else
        saved = config;
    time_t stamp = time(NULL);
    atomic_store(&clock_valid, stamp >= 1704067200 && stamp <= 4102444799LL);
    return xTaskCreate(device_worker, "device_settings", 8192, NULL, 3, NULL) == pdPASS
               ? ESP_OK
               : ESP_ERR_NO_MEM;
}
