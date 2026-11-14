#include "cyd.h"
#include "cyd_device.h"
#include "settings.h"
#include "connections.h"
#include <stdio.h>
static lv_obj_t *view, *body, *notice, *clock_label, *wizard, *cross, *wizard_note, *editor,
    *preview;
static unsigned width, height;
static int page;
static cyd_display_mode_t last_mode = CYD_DISPLAY_NORMAL;
static void draw(int next);
static lv_obj_t *text(const char *s) {
    lv_obj_t *l = lv_label_create(body);
    lv_label_set_text(l, s);
    lv_obj_set_width(l, LV_PCT(100));
    return l;
}
static void action(lv_event_t *e) {
    int a = (int)(intptr_t)lv_event_get_user_data(e);
    esp_err_t err = ESP_OK;
    switch (a) {
    case 0:
        cyd_settings_close();
        return;
    case 100:
        draw(0);
        return;
    case 1:
        draw(1);
        return;
    case 2:
        draw(2);
        return;
    case 3:
        cyd_connections_open();
        return;
    case 4:
        cyd_connections_open_device();
        return;
    case 5:
        err = cyd_device_calibrate();
        break;
    case 6:
        err = cyd_device_confirm();
        break;
    case 7:
        err = cyd_device_revert();
        break;
    case 8:
        err = cyd_device_reset_display();
        break;
    case 9:
        draw(3);
        return;
    case 10:
    case 11: {
        cyd_device_status_t s;
        cyd_device_snapshot(&s);
        err = cyd_device_set_offset(s.utc_offset_minutes + (a == 10 ? -30 : 30));
        break;
    }
    default:
        if (a >= 20 && a <= 23)
            err = cyd_device_rotation(a - 20);
        break;
    }
    if (notice)
        lv_label_set_text(notice, err ? "Request unavailable; finish the current operation first"
                                      : "Request queued");
}
static lv_obj_t *button_on(lv_obj_t *parent, const char *s, int a) {
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_set_size(b, LV_PCT(100), 42);
    lv_obj_set_style_radius(b, 18, 0);
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, s);
    lv_obj_center(l);
    lv_obj_add_event_cb(b, action, LV_EVENT_CLICKED, (void *)(intptr_t)a);
    return b;
}
static void button(const char *s, int a) {
    button_on(body, s, a);
}
static void brightness(lv_event_t *e) {
    unsigned n = lv_slider_get_value(lv_event_get_target(e));
    cyd_device_brightness(n, lv_event_get_code(e) == LV_EVENT_RELEASED);
}
static void keyboard(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CANCEL) {
        draw(2);
        return;
    }
    if (lv_event_get_code(e) == LV_EVENT_READY) {
        esp_err_t err = cyd_device_set_local_clock(lv_textarea_get_text(editor));
        if (err) {
            lv_textarea_set_placeholder_text(editor, "Use YYYY-MM-DD HH:MM");
            return;
        }
        draw(2);
    }
}
static void draw(int next) {
    page = next;
    lv_obj_clean(view);
    notice = clock_label = editor = preview = NULL;
    body = NULL;
    if (page == 3) {
        editor = lv_textarea_create(view);
        lv_textarea_set_one_line(editor, true);
        lv_textarea_set_max_length(editor, 16);
        lv_obj_set_pos(editor, 4, 20);
        lv_obj_set_width(editor, width - 8);
        lv_textarea_set_placeholder_text(editor, "YYYY-MM-DD HH:MM");
        lv_obj_t *k = lv_keyboard_create(view);
        lv_obj_set_align(k, LV_ALIGN_TOP_LEFT);
        lv_obj_set_pos(k, 0, 72);
        lv_obj_set_size(k, width, height - 72);
        lv_keyboard_set_textarea(k, editor);
        lv_obj_add_event_cb(k, keyboard, LV_EVENT_ALL, NULL);
        return;
    }
    lv_obj_t *back = button_on(view, "Back", page ? 100 : 0);
    lv_obj_set_pos(back, 4, 2);
    lv_obj_set_size(back, 64, 30);
    lv_obj_t *title = lv_label_create(view);
    lv_label_set_text(title, page == 1 ? "Display" : page == 2 ? "Clock" : "Settings");
    lv_obj_set_pos(title, 80, 8);
    body = lv_obj_create(view);
    lv_obj_set_pos(body, 0, 36);
    lv_obj_set_size(body, width, height - 36);
    lv_obj_set_style_pad_all(body, 12, 0);
    lv_obj_set_style_pad_row(body, 12, 0);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    notice = text("");
    if (page == 0) {
        button("Connections", 3);
        button("Display & touch", 1);
        button("Clock", 2);
        button("Device access & OS updates", 4);
    }
    if (page == 1) {
        cyd_device_status_t s;
        cyd_device_snapshot(&s);
        preview = text("");
        if (s.mode == CYD_DISPLAY_ROTATION_PREVIEW || s.mode == CYD_DISPLAY_CALIBRATION_PREVIEW) {
            button("Keep display changes", 6);
            button("Revert display changes", 7);
        }
        text("Brightness");
        lv_obj_t *slider = lv_slider_create(body);
        lv_obj_set_width(slider, LV_PCT(92));
        lv_obj_set_height(slider, 18);
        lv_slider_set_range(slider, 10, 100);
        lv_slider_set_value(slider, s.brightness, LV_ANIM_OFF);
        lv_obj_add_event_cb(slider, brightness, LV_EVENT_VALUE_CHANGED, NULL);
        lv_obj_add_event_cb(slider, brightness, LV_EVENT_RELEASED, NULL);
        text("Orientation (15-second preview)");
        button("0 degrees", 20);
        button("90 degrees", 21);
        button("180 degrees", 22);
        button("270 degrees", 23);
        button("Calibrate touch", 5);
        text("Calibration: tap 5 targets, then Keep. Hold anywhere for 2 seconds to cancel.");
        button("Reset display to board defaults", 8);
    }
    if (page == 2) {
        clock_label = text("");
        text("Internet time sync runs when connected. After power loss, sync again or set the "
             "time.");
        button("Set date & time", 9);
        button("UTC offset -30 minutes", 10);
        button("UTC offset +30 minutes", 11);
        text("For exact iPad time and timezone, use Sync clock in Safari's device settings.");
    }
}
static void poll(lv_timer_t *t) {
    (void)t;
    cyd_device_status_t s;
    cyd_device_snapshot(&s);
    if (lv_obj_has_flag(view, LV_OBJ_FLAG_HIDDEN))
        return;
    if (s.mode == CYD_DISPLAY_CALIBRATING) {
        lv_obj_remove_flag(wizard, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(wizard);
        lv_obj_set_pos(cross, s.target_x - 12, s.target_y - 12);
        char b[96];
        snprintf(b, sizeof(b), "Tap target %u/5\nHold anywhere 2s to cancel (%us)",
                 s.target_index + 1, s.seconds_left);
        lv_label_set_text(wizard_note, b);
    } else {
        lv_obj_add_flag(wizard, LV_OBJ_FLAG_HIDDEN);
        if (last_mode != s.mode && (page == 1 || last_mode == CYD_DISPLAY_CALIBRATING))
            draw(1);
    }
    last_mode = s.mode;
    if (notice)
        lv_label_set_text(notice, s.message);
    if (preview) {
        char b[100];
        if (s.mode == CYD_DISPLAY_NORMAL)
            snprintf(b, sizeof(b), "%u%% brightness / %u degrees", s.brightness, s.rotation * 90);
        else
            snprintf(b, sizeof(b), "Keep changes within %us", s.seconds_left);
        lv_label_set_text(preview, b);
    }
    if (clock_label) {
        char b[96];
        snprintf(b, sizeof(b), "%s\nUTC offset: %+d minutes", s.clock, s.utc_offset_minutes);
        lv_label_set_text(clock_label, b);
    }
}
void cyd_settings_init(lv_obj_t *parent, unsigned w, unsigned h) {
    width = w;
    height = h - 56;
    view = lv_obj_create(parent);
    lv_obj_set_pos(view, 0, 0);
    lv_obj_set_size(view, width, height);
    lv_obj_set_style_pad_all(view, 0, 0);
    lv_obj_set_style_border_width(view, 0, 0);
    lv_obj_set_style_radius(view, 0, 0);
    lv_obj_set_style_bg_opa(view, LV_OPA_COVER, 0);
    lv_obj_remove_flag(view, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(view, LV_OBJ_FLAG_HIDDEN);
    wizard = lv_obj_create(parent);
    lv_obj_set_size(wizard, w, h);
    lv_obj_set_style_bg_color(wizard, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(wizard, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(wizard, 0, 0);
    lv_obj_set_style_border_width(wizard, 0, 0);
    lv_obj_remove_flag(wizard, LV_OBJ_FLAG_SCROLLABLE);
    cross = lv_obj_create(wizard);
    lv_obj_remove_style_all(cross);
    lv_obj_set_size(cross, 24, 24);
    lv_obj_remove_flag(cross, LV_OBJ_FLAG_SCROLLABLE);
    for (unsigned axis = 0; axis < 2; axis++) {
        lv_obj_t *line = lv_obj_create(cross);
        lv_obj_remove_style_all(line);
        lv_obj_set_size(line, axis ? 2 : 24, axis ? 24 : 2);
        lv_obj_set_style_bg_color(line, lv_color_hex(0x9bff47), 0);
        lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
        lv_obj_center(line);
    }
    wizard_note = lv_label_create(wizard);
    lv_obj_set_width(wizard_note, w - 64);
    lv_obj_align(wizard_note, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_align(wizard_note, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_flag(wizard, LV_OBJ_FLAG_HIDDEN);
    lv_timer_create(poll, 200, NULL);
}
void cyd_settings_open(void) {
    draw(0);
    lv_obj_remove_flag(view, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(view);
}
void cyd_settings_close(void) {
    if (!view)
        return;
    lv_obj_add_flag(view, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(wizard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clean(view);
    notice = clock_label = preview = editor = NULL;
}
void cyd_settings_resize(unsigned w, unsigned h) {
    width = w;
    height = h - 56;
    lv_obj_set_size(view, width, height);
    lv_obj_set_size(wizard, w, h);
    lv_obj_set_width(wizard_note, w - 64);
    lv_obj_align(wizard_note, LV_ALIGN_CENTER, 0, 0);
    if (!lv_obj_has_flag(view, LV_OBJ_FLAG_HIDDEN))
        draw(page);
}
