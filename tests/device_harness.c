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
    if (which == 9) {
        CHECK(cyd_device_preferences(30, 10, true) == 0); pump();
        CHECK(cyd_device_init(320, 240, light, rotate) == 0);
        cyd_device_status_t s; cyd_device_snapshot(&s);
        CHECK(s.dim_seconds == 30 && s.dim_percent == 10 && s.clock_12h);
        CHECK(config.brightness == 100 && saved.rotation == 0);
        return 0;
    }
    if (which == 10) {
        cyd_device_preferences(30, 10, false); pump();
        host_tick(29999); pump(); CHECK(effective_brightness == 100);
        host_tick(1); pump(); CHECK(applied_brightness == 10 && effective_brightness == 10);
        int x, y;
        CHECK(cyd_device_touch(700, 1100, true, &x, &y)); pump();
        CHECK(applied_brightness == 100);
        CHECK(cyd_device_touch(700, 1100, true, &x, &y));
        CHECK(!cyd_device_touch(0, 0, false, &x, &y));
        CHECK(!cyd_device_touch(800, 1200, true, &x, &y));
        cyd_device_status_t s; cyd_device_snapshot(&s);
        CHECK(s.touch_presses == 2 && s.touch_pressed && s.touch_raw_x == 800);
        CHECK(s.touch_x == x && s.touch_y == y);
        cyd_device_touch(0, 0, false, &x, &y);
        cyd_device_status_t released; cyd_device_snapshot(&released);
        CHECK(!released.touch_pressed && released.touch_x == s.touch_x);
        CHECK(saved.brightness == 100);
        return 0;
    }
    if (which == 11) {
        cyd_device_preferences(30, 50, false); pump();
        cyd_device_brightness(25, true); pump();
        host_tick(31000); pump(); CHECK(effective_brightness == 25);
        cyd_device_brightness(90, true); pump();
        host_tick(31000); pump(); CHECK(effective_brightness == 50);
        cyd_device_preferences(0, 10, false); pump(); CHECK(effective_brightness == 90);
        host_tick(500000); pump(); CHECK(effective_brightness == 90);
        return 0;
    }
    if (which == 12) {
        CHECK(cyd_device_preferences(15, 10, false) != 0);
        CHECK(cyd_device_preferences(30, 0, false) != 0);
        host_fail_save(true);
        cyd_device_preferences(30, 25, true); pump();
        CHECK(preferences.dim_seconds == 0 && !preferences.clock_12h);
        return 0;
    }
    if (which == 13) {
        cyd_device_preferences(30, 10, false); pump();
        host_tick(31000); pump(); CHECK(effective_brightness == 10);
        CHECK(cyd_maintenance_begin()); pump(); CHECK(effective_brightness == 100);
        CHECK(cyd_device_preferences(60, 25, false) != 0);
        cyd_maintenance_end(); pump(); CHECK(effective_brightness == 10);
        return 0;
    }
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
