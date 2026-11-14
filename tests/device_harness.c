#include "system_sdk/cyd.h"
#include "../components/device_settings/device.c"
esp_err_t cyd_runtime_stop(void) {
    return 0;
}
static unsigned applied_rotation, applied_brightness;
static esp_err_t rotate(unsigned n) {
    applied_rotation = n;
    return 0;
}
static esp_err_t light(unsigned n) {
    applied_brightness = n;
    return 0;
}
static void pump(void) {
    device_job_t j;
    while (xQueueReceive(jobs, &j, 0))
        apply_job(&j);
    device_tick();
}
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x))                                                                                  \
            return __LINE__;                                                                       \
    } while (0)
int host_device_case(int which) {
    host_reset();
    mode = CYD_DISPLAY_NORMAL;
    owns_maintenance = false;
    CHECK(cyd_device_init(320, 240, light, rotate) == 0);
    if (which == 8) {
        size_t n = sizeof(device_config_t);
        device_config_t stored;
        CHECK(nvs_get_blob(store, "display", &stored, &n) == 0);
        CHECK(!memcmp(stored.matrix, defaults.matrix, sizeof(defaults.matrix)));
        return 0;
    }
    if (which == 0) {
        CHECK(cyd_device_rotation(1) == 0);
        pump();
        CHECK(applied_rotation == 1 && mode == CYD_DISPLAY_ROTATION_PREVIEW);
        host_tick(16000);
        pump();
        CHECK(applied_rotation == 0 && mode == CYD_DISPLAY_NORMAL && !cyd_maintenance_active());
        return 0;
    }
    if (which == 1) {
        cyd_device_rotation(3);
        pump();
        cyd_device_confirm();
        pump();
        CHECK(saved.rotation == 3);
        CHECK(cyd_device_init(320, 240, light, rotate) == 0);
        CHECK(config.rotation == 3);
        return 0;
    }
    if (which == 2) {
        cyd_device_rotation(2);
        pump();
        host_fail_save(true);
        cyd_device_confirm();
        pump();
        CHECK(saved.rotation == 0 && mode == CYD_DISPLAY_ROTATION_PREVIEW);
        host_tick(16000);
        pump();
        CHECK(config.rotation == 0);
        return 0;
    }
    if (which == 3) {
        cyd_device_calibrate();
        pump();
        CHECK(mode == CYD_DISPLAY_CALIBRATING);
        int x, y;
        cyd_device_touch(100, 200, true, &x, &y);
        host_tick(2100);
        cyd_device_touch(100, 200, true, &x, &y);
        pump();
        CHECK(mode == CYD_DISPLAY_NORMAL && !cyd_maintenance_active());
        return 0;
    }
    if (which == 4) {
        cyd_device_calibrate();
        pump();
        int x, y;
        for (unsigned i = 0; i < 5; i++) {
            int rx = target_points[i][1] * 10 + 200, ry = target_points[i][0] * 10 + 100;
            cyd_device_touch(rx, ry, true, &x, &y);
            cyd_device_touch(rx, ry, false, &x, &y);
        }
        pump();
        CHECK(mode == CYD_DISPLAY_CALIBRATION_PREVIEW);
        cyd_device_touch(700, 1100, true, &x, &y);
        CHECK(abs(x - 100) <= 1 && abs(y - 50) <= 1);
        cyd_device_confirm();
        pump();
        CHECK(mode == CYD_DISPLAY_NORMAL);
        CHECK(cyd_device_init(320, 240, light, rotate) == 0);
        cyd_device_touch(700, 1100, true, &x, &y);
        CHECK(abs(x - 100) <= 1 && abs(y - 50) <= 1);
        return 0;
    }
    if (which == 5) {
        cyd_device_calibrate();
        pump();
        host_tick(61000);
        pump();
        CHECK(mode == CYD_DISPLAY_NORMAL);
        return 0;
    }
    if (which == 6) {
        cyd_device_brightness(35, true);
        pump();
        CHECK(applied_brightness == 35 && saved.brightness == 35);
        CHECK(cyd_device_brightness(0, true) != 0);
        return 0;
    }
    if (which == 7) {
        cyd_device_rotation(1);
        pump();
        cyd_device_brightness(20, true);
        pump();
        CHECK(saved.rotation == 0);
        cyd_device_revert();
        pump();
        CHECK(config.rotation == 0);
        return 0;
    }
    return 0;
}
