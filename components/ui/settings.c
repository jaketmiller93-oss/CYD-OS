#include "pager.h"
#include "cyd.h"
#include "cyd_peer.h"
#include "sdkconfig.h"
#include "cyd_device.h"
#include "settings.h"
#ifdef CONFIG_CYD_PAIRED_SYSTEM
#include "cyd_pair.h"
#include "pair_ui.h"
#endif
#include "connections.h"
#include <stdio.h>
#include <string.h>
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_psram.h"
#include "display_geometry.h"
#ifndef CYD_UI_PREVIEW
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#endif
static void text_if_changed(lv_obj_t *label, const char *value) {
    if (strcmp(lv_label_get_text(label), value)) lv_label_set_text(label, value);
}
static lv_obj_t *view, *body, *notice, *clock_label, *wizard, *cross, *wizard_note, *editor,
    *preview;
static unsigned width, height;
static lv_obj_t *live, *touch_pad, *touch_dot;
static bool frozen, home_diagnostics;
static uint32_t sampled_at;
static unsigned monitor_interval = 1000, test_color;
static const unsigned dim_times[] = {0, 30, 60, 120, 300};
static const unsigned dim_levels[] = {10, 25, 50};
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
    if (a >= 200 && a <= 209) { draw(a - 200); return; }
    if (a == 210) { frozen = !frozen; if (notice) lv_label_set_text(notice, frozen ? "Readings paused" : "Readings live"); return; }
    if (a == 211) { monitor_interval = monitor_interval == 1000 ? 250 : monitor_interval == 250 ? 2000 : 1000; sampled_at = 0; return; }
    if (a == 212 && touch_pad) {
        static const uint32_t colors[] = {0xff0000, 0x00ff00, 0x0000ff, 0xffffff, 0x000000};
        test_color = (test_color + 1) % 5;
        lv_obj_set_style_bg_color(touch_pad, lv_color_hex(colors[test_color]), 0); return;
    }
#ifdef CONFIG_CYD_PAIRED_SYSTEM
    if(CYD_PAIR_MANAGER && a==260) {cyd_settings_close();cyd_ui_post_action(8);return;}
#endif
    switch (a) {
    case 0:
        cyd_settings_close();
        return;
    case 100:
        if(home_diagnostics){cyd_settings_close();return;}
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
        err = cyd_device_set_offset(s.utc_offset_minutes + (a == 10 ? -15 : 15));
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
    lv_obj_set_style_radius(b, 12, 0);
    lv_obj_set_style_pad_all(b, 6, 0);
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, s);
    lv_obj_set_width(l, LV_PCT(100));
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
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
static lv_obj_t *choice_buttons[3];
static unsigned choice_values[3];
static const char *preference_caption(unsigned field,unsigned value) {
    static const char *items[3][5]={{"Never", "30 seconds", "1 minute", "2 minutes", "5 minutes"},
        {"10%", "25%", "50%"},{"24 hour", "12 hour"}};
    return items[field][value];
}
static void preference_changed(lv_event_t *e) {
    unsigned field=(unsigned)(uintptr_t)lv_event_get_user_data(e);
    unsigned count=field==0?5:field==1?3:2;
    unsigned value=(choice_values[field]+1)%count;
    cyd_device_status_t status;cyd_device_snapshot(&status);
    unsigned seconds=status.dim_seconds,percent=status.dim_percent;
    bool hours=status.clock_12h;
    if(page==1) {
        seconds=dim_times[field==0?value:choice_values[0]];
        percent=dim_levels[field==1?value:choice_values[1]];
    } else hours=value==1;
    esp_err_t err=cyd_device_preferences(seconds,percent,hours);
    if(!err) {
        choice_values[field]=value;
        lv_label_set_text(lv_obj_get_child(lv_event_get_target(e),0),preference_caption(field,value));
    }
    lv_label_set_text(notice,err?"Could not queue preference":"Saving preference...");
}
static void choice(const char *label,const char *options,unsigned selected,unsigned field) {
    (void)options;text(label);choice_values[field]=selected;
    lv_obj_t *b=lv_button_create(body);choice_buttons[field]=b;
    lv_obj_set_size(b,LV_PCT(100),42);lv_obj_set_style_pad_all(b,6,0);
    lv_obj_t *l=lv_label_create(b);lv_label_set_text(l,preference_caption(field,selected));lv_obj_center(l);
    lv_obj_add_event_cb(b,preference_changed,LV_EVENT_CLICKED,(void *)(uintptr_t)field);
}
static void monitor(const cyd_device_status_t *status) {
    if (!live || frozen) return;
    uint32_t stamp = (uint32_t)(esp_timer_get_time() / 1000);
    if (sampled_at && stamp - sampled_at < monitor_interval) return;
    sampled_at = stamp;
    char b[1024], ip[48];
    cyd_network_address(ip, sizeof(ip));
    if (page == 9) {
        cyd_peer_info_t peer; cyd_peer_snapshot(&peer);
        int used = snprintf(b, sizeof(b), "%s\nProtocol: %u\n%s\nUART running: %s\nPeer: %s\nEcho result: %d / %lums\nBad / dropped frames: %lu / %lu\nPeer restarts: %lu",
            peer.role == CYD_LINK_MAIN ? "S3 application host" : "C5 companion",
            CYD_LINK_PROTOCOL, peer.message, peer.running ? "yes" : "no",
            peer.connected ? "connected" : "disconnected", peer.echo_result,
            (unsigned long)peer.latency_ms, (unsigned long)peer.errors, (unsigned long)peer.drops, (unsigned long)peer.resets);
        if (peer.remote_valid && used > 0 && (size_t)used < sizeof(b))
            snprintf(b + used, sizeof(b) - used, "\nRemote %s / age %lums\nHeap %lu B / SD %s\nNetwork address: %s", peer.remote.version,
                (unsigned long)peer.age_ms, (unsigned long)peer.remote.free_heap,
                peer.remote.sd_ready ? "ready" : "unavailable", peer.remote.network_address ? "available" : "unavailable");
    } else if (page == 4) {
        snprintf(b, sizeof(b),
            "Live / %ums refresh\nUptime: %lus\nHeap free: %u B\nLargest block: %u B\nInternal largest: %u B\nPSRAM detected: %u B\nSD: %s\nNetwork: %s\nBacklight: %u%% (set %u%%)\nTouch presses: %lu",
            monitor_interval, (unsigned long)(stamp / 1000), (unsigned)esp_get_free_heap_size(),
            (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
            (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
            (unsigned)esp_psram_get_size(), cyd_storage_ready() ? "ready" : "unavailable", ip,
            status->effective_brightness, status->brightness, (unsigned long)status->touch_presses);
    } else {
        snprintf(b, sizeof(b), "Touch: %s\nRaw: %d, %d\nNative pixels: %d, %d\nPresses: %lu / age: %lums",
            status->touch_pressed ? "DOWN" : "released", status->touch_raw_x, status->touch_raw_y,
            status->touch_x, status->touch_y, (unsigned long)status->touch_presses,
            (unsigned long)status->touch_age_ms);
        if (touch_dot && touch_pad && status->touch_presses) {
            int x, y;
            cyd_display_to_logical(status->rotation, cyd_board_width(), cyd_board_height(),
                status->touch_x, status->touch_y, &x, &y);
            lv_obj_update_layout(touch_pad);
            int px, py;
            lv_area_t area; lv_obj_get_coords(touch_pad, &area);
            px = x - area.x1; py = y - area.y1;
            if (px < 0) px = 0;
            if (py < 0) py = 0;
            int maxx = lv_obj_get_width(touch_pad) - 12, maxy = lv_obj_get_height(touch_pad) - 12;
            if (px > maxx) px = maxx;
            if (py > maxy) py = maxy;
            lv_obj_set_pos(touch_dot, px, py);
        }
    }
#ifdef CONFIG_CYD_PAIRED_SYSTEM
    if(CYD_PAIR_MANAGER && page==4) {
        cyd_peer_info_t peer;cyd_peer_snapshot(&peer);size_t used=strlen(b);
        if(peer.remote_valid)snprintf(b+used,sizeof(b)-used,"\nS3: %s / heap %lu B / SD %s",peer.remote.version,(unsigned long)peer.remote.free_heap,peer.remote.sd_ready?"ready":"unavailable");
        else snprintf(b+used,sizeof(b)-used,"\nS3: unavailable (local manager remains active)");
    }
#endif
    if (strcmp(lv_label_get_text(live), b)) lv_label_set_text(live, b);
}
static void draw(int next) {
    page = next;
    lv_obj_clean(view);
    notice = clock_label = editor = preview = live = touch_pad = touch_dot = NULL;
    sampled_at = 0; frozen = false;
    memset(choice_buttons,0,sizeof(choice_buttons));
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
    lv_obj_t *back = button_on(view, "Up", page ? 100 : 0);
    lv_obj_set_pos(back, 4, 2);
    lv_obj_set_size(back, 64, 30);
    lv_obj_t *title = lv_label_create(view);
    static const char *titles[] = {"Settings", "Display", "Clock", "Set time", "Diagnostics", "Touch test", "Screen test", "Developer", "About", "Board link"};
    lv_label_set_text(title, titles[page]);
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
#ifdef CONFIG_CYD_PAIRED_SYSTEM
        if(CYD_PAIR_MANAGER)button("S3 display settings",260);
#endif
        button("Clock", 2);
        button("Diagnostics", 204);
        button("Touch input test", 205);
        button("Screen color test", 206);
        button("Developer & modules", 207);
        button("About this tablet", 208);
        button("Connected boards", 209);
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
        unsigned selected = 0;
        for (unsigned i = 0; i < 5; i++) if (dim_times[i] == s.dim_seconds) selected = i;
        choice("Dim after no touch", "Never\n30 seconds\n1 minute\n2 minutes\n5 minutes", selected, 0);
        selected = 0;
        for (unsigned i = 0; i < 3; i++) if (dim_levels[i] == s.dim_percent) selected = i;
        choice("Idle brightness", "10%\n25%\n50%", selected, 1);
        text("Dimming keeps apps running. First touch restores brightness; lift and tap again to act.");
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
        cyd_device_status_t s; cyd_device_snapshot(&s);
        choice("Clock format", "24 hour\n12 hour", s.clock_12h ? 1 : 0, 2);
        text("Internet time sync runs when connected. After power loss, sync again or set the "
             "time.");
        button("Set date & time", 9);
        button("UTC offset -15 minutes", 10);
        button("UTC offset +15 minutes", 11);
        text("For exact iPad time and timezone, use Sync clock in Safari's device settings.");
    }
    if (page == 4 || page == 5 || page == 9) {
        button("Pause / resume readings", 210);
        button("Cycle refresh: 1s / 250ms / 2s", 211);
        live = text("");
        if (page == 9) {
#ifdef CONFIG_CYD_PAIRED_SYSTEM
            text("One tablet: S3 applications and System drive; C5 connections, settings and Device drive.");
#else
            text("Health and echo only. No app/network offload or authenticated pairing.");
#endif
        }
        if (page == 5) {
            text("Drag in the pad. Raw values are controller samples, not voltage readings.");
            touch_pad = lv_obj_create(body);
            lv_obj_set_size(touch_pad, LV_PCT(100), 100);
            lv_obj_set_style_pad_all(touch_pad, 0, 0);
            lv_obj_set_style_border_width(touch_pad, 0, 0);
            lv_obj_remove_flag(touch_pad, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_remove_flag(touch_pad, LV_OBJ_FLAG_SCROLL_CHAIN_VER);
            touch_dot = lv_obj_create(touch_pad);
            lv_obj_remove_style_all(touch_dot);
            lv_obj_set_size(touch_dot, 12, 12);
            lv_obj_set_style_bg_color(touch_dot, lv_color_hex(0x9bff47), 0);
            lv_obj_set_style_bg_opa(touch_dot, LV_OPA_COVER, 0);
            lv_obj_remove_flag(touch_dot, LV_OBJ_FLAG_CLICKABLE);
        }
    }
    if (page == 6) {
        text("Check for missing colors, flicker or stuck pixels. Cycle red, green, blue, white and black.");
        button("Next test color", 212);
        touch_pad = lv_obj_create(body);
        lv_obj_set_size(touch_pad, LV_PCT(100), 110);
        lv_obj_set_style_bg_color(touch_pad, lv_color_hex(0xff0000), 0);
        lv_obj_set_style_bg_opa(touch_pad, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(touch_pad, 0, 0);
        lv_obj_set_style_border_width(touch_pad, 0, 0);
        test_color = 0;
    }
    if (page == 7) {
        text("App playground");
        text("Create Lua apps as .cydapp packages. Upload using Safari > Apps. Home / Stop exits an app. Logs show print() and runtime errors.");
        text("Available: touch UI, app storage, timers, HTTP, TCP, MQTT and protected credentials with manifest permissions.");
        text("Modules & pin reference");
#ifdef CONFIG_IDF_TARGET_ESP32C5
        text("C5 shared SPI: SCK 6, MISO 2, MOSI 7\nLCD: CS 23, DC 24, light 25\nTouch: CS 1\nSD: CS 10\nBOOT: 28 (reserved)\nUSB and flash/PSRAM pins are reserved too.");
#else
        text("S3 touch/control I2C: SDA 15, SCL 16\nRGB: clock 39, sync 40/41, DE 42; data 21/47/48/45/38/9/10/11/12/13/14/7/17/18/3/46\nSD: MOSI 6, MISO 4, SCK 5; CS tied low (no MCU CS).");
#endif
        text("Pin map describes the compiled profile. Verify the board schematic and connector before wiring. No free header pin is assumed. GPIO/ADC/I2C/UART app access is not enabled yet.");
        text("For network modules now: use HTTP or MQTT over Wi-Fi. Direct wired drivers need reserved-pin checks, bus ownership, timeouts and cleanup when an app stops.");
#ifdef CONFIG_CYD_PAIRED_SYSTEM
        text("Apps run on S3. C5 handles Wi-Fi, Bluetooth LE and shared-drive management. UART reserves C5 TX5/RX4 and S3 TX8/RX2. See Connected boards for live link state.");
#else
        text("See Settings > Connected boards for link status. Both boards run their local apps independently; remote app/network services are not available yet.");
#endif
    }
    if (page == 8) {
        char b[512];
#ifndef CYD_UI_PREVIEW
        esp_chip_info_t chip; esp_chip_info(&chip);
        uint32_t flash = 0; esp_flash_get_size(NULL, &flash);
        snprintf(b, sizeof(b), "CYD OS %s\n%s\nCPU cores: %u / revision: %u\nFlash: %lu MiB\nPSRAM: %u MiB\nReset reason: %d\nESP-IDF: %s\nNative panel: %ux%u", cyd_system_version(), cyd_board_model(),
            (unsigned)chip.cores, (unsigned)chip.revision, (unsigned long)(flash / 1048576),
            (unsigned)(esp_psram_get_size() / 1048576), (int)esp_reset_reason(), esp_get_idf_version(), cyd_board_width(), cyd_board_height());
#else
        snprintf(b, sizeof(b), "CYD OS host preview\n%s", cyd_board_model());
#endif
        text(b);
        text("Local apps work offline. Wi-Fi enables upload and network modules. No battery gauge or analog sensor is fitted by this OS profile.");
    }
    cyd_pager_attach(body);
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
    if (notice && page != 4 && page != 5 && page != 9)
        text_if_changed(notice, s.message);
    if (page == 4 || page == 5 || page == 9) monitor(&s);
    if (preview) {
        char b[100];
        if (s.mode == CYD_DISPLAY_NORMAL)
            snprintf(b, sizeof(b), "%u%% brightness / %u degrees", s.brightness, s.rotation * 90);
        else
            snprintf(b, sizeof(b), "Keep changes within %us", s.seconds_left);
        text_if_changed(preview, b);
    }
    if (clock_label) {
        char b[96];
        snprintf(b, sizeof(b), "%s\nUTC offset: %+d minutes", s.clock, s.utc_offset_minutes);
        text_if_changed(clock_label, b);
    }
    cyd_pager_refresh(body);
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
    home_diagnostics=false;
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
    body=NULL;home_diagnostics=false;
    notice = clock_label = preview = editor = live = touch_pad = touch_dot = NULL;
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

void cyd_settings_open_diagnostics(void) {
    home_diagnostics=false;
#ifdef CONFIG_CYD_PAIRED_SYSTEM
    home_diagnostics=CYD_PAIR_MANAGER;
#endif
    draw(4);
    lv_obj_remove_flag(view, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(view);
}

bool cyd_settings_nav(int direction, cyd_nav_state_t *state) {
    if(!view || lv_obj_has_flag(view,LV_OBJ_FLAG_HIDDEN))return false;
    cyd_pager_refresh(body);
    if(direction && !cyd_pager_move(body,direction) && direction<0) {
        if(home_diagnostics&&page==4)cyd_settings_close();else if(page==3)draw(2);else if(page)draw(0);else cyd_settings_close();
    }
    cyd_pager_state(body,state);state->active=true;state->back=true;return true;
}
