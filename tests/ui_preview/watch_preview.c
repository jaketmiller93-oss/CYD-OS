#include "cyd.h"
#include "cyd_device.h"
#include "settings.h"
#include "connections.h"
#include "projects.h"
static unsigned native_w, native_h;
static uint16_t frame[800 * 800];
extern cyd_device_status_t preview_device;
void host_run_task(const char *);
unsigned cyd_board_width(void) {
    return native_w;
}
unsigned cyd_board_height(void) {
    return native_h;
}
void cyd_board_flush(lv_display_t *d, const lv_area_t *a, uint8_t *px) {
    uint16_t *p = (uint16_t *)px;
    assert(a->x1 >= 0 && a->y1 >= 0 && a->x2 < (int)native_w && a->y2 < (int)native_h);
    for (int y = a->y1; y <= a->y2; y++)
        for (int x = a->x1; x <= a->x2; x++)
            frame[y * native_w + x] = *p++;
    lv_display_flush_ready(d);
}
static void save(const char *name, unsigned r) {
    for (int i = 0; i < 24; i++) {
        host_tick(20);
        lv_timer_handler();
    }
    char path[160];
    snprintf(path, sizeof(path), "watch-%s-%u-r%u.ppm", name, native_w, r);
    FILE *f = fopen(path, "wb");
    assert(f);
    fprintf(f, "P6\n%u %u\n255\n", native_w, native_h);
    for (unsigned i = 0; i < native_w * native_h; i++) {
        uint16_t p = frame[i];
        uint8_t rgb[] = {((p >> 11) & 31) * 255 / 31, ((p >> 5) & 63) * 255 / 63,
                         (p & 31) * 255 / 31};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}
static lv_obj_t *find(lv_obj_t *obj, const char *text) {
    if (lv_obj_check_type(obj, &lv_label_class) && !strcmp(lv_label_get_text(obj), text))
        return lv_obj_get_parent(obj);
    for (unsigned i = 0; i < lv_obj_get_child_count(obj); i++) {
        lv_obj_t *found = find(lv_obj_get_child(obj, i), text);
        if (found)
            return found;
    }
    return NULL;
}
static void click(const char *text) {
    lv_obj_update_layout(lv_screen_active());
    lv_obj_t *b = find(lv_screen_active(), text);
    if (!b)
        fprintf(stderr, "Missing button: %s\n", text);
    assert(b);
    lv_obj_send_event(b, LV_EVENT_CLICKED, NULL);
}
int main(int argc, char **argv) {
    native_w = argc > 1 ? atoi(argv[1]) : 320;
    native_h = native_w == 320 ? 240 : 480;
    host_reset();
    assert(cyd_ui_init() == 0);
    for (unsigned r = 0; r < 4; r++) {
        preview_device.rotation = r;
        assert(cyd_ui_apply_rotation(r) == 0);
        cyd_connections_close();
        cyd_projects_ui_close();
        cyd_settings_close();
        save("home", r);
        cyd_settings_open();
        save("settings", r);
        click("Display & touch");
        save("display", r);
        click("Calibrate touch");
        save("calibrate", r);
        preview_device.mode = CYD_DISPLAY_CALIBRATION_PREVIEW;
        preview_device.seconds_left = 15;
        save("calibrate-confirm", r);
        preview_device.mode = CYD_DISPLAY_NORMAL;
        cyd_settings_close();
        cyd_projects_ui_open();
        host_run_task("app_folders");
        save("apps", r);
        click("Folder: Tools");
        click("Hello (2 versions)");
        save("versions", r);
        cyd_projects_ui_close();
        cyd_settings_open();
        click("Clock");
        click("Set date & time");
        save("clock-keyboard", r);
        cyd_settings_close();
    }
    puts("Actual LVGL shell/settings/apps rendered in all four orientations");
    return 0;
}
