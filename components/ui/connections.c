#include "pager.h"
#include "connections.h"
#include "cyd_network.h"
#include "cyd_access.h"
#include "cyd_ota.h"
#include <stdio.h>
#include <string.h>

typedef enum {
    PAGE_MAIN,
    PAGE_SCAN,
    PAGE_SAVED,
    PAGE_JOIN,
    PAGE_AP,
    PAGE_EDIT,
    PAGE_FORGET,
    PAGE_DEVICE,
    PAGE_TOKEN_CONFIRM
} page_t;
typedef enum { EDIT_SSID, EDIT_PASSWORD, EDIT_AP_SSID, EDIT_AP_PASSWORD } field_t;
static void connection_text(lv_obj_t *label,const char *text) {
    if(strcmp(lv_label_get_text(label),text))lv_label_set_text(label,text);
}
static lv_obj_t *view, *body, *note, *link_label, *editor, *edit_title, *os_label;
static bool show_token, device_root;
static unsigned width, height;
static page_t page;
static field_t field;
static char join_ssid[33], join_password[65], ap_ssid[33], ap_password[64];
static bool secured, show_ap_password, waiting_scan;
static uint32_t scan_revision, notice_started, notice_duration;
static cyd_wifi_scan_t rendered_scan[CYD_WIFI_SCAN_MAX];
static char rendered_saved[CYD_WIFI_PROFILES][33];
static void render(page_t next);
static void wipe(void *p, size_t n) {
    volatile unsigned char *s = p;
    while (n--)
        *s++ = 0;
}
static lv_obj_t *text(lv_obj_t *parent, const char *s) {
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, s);
    lv_obj_set_width(l, LV_PCT(100));
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    return l;
}
static lv_obj_t *button(lv_obj_t *parent, const char *title, lv_event_cb_t cb, int value) {
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_set_width(b, LV_PCT(100));
    lv_obj_set_height(b, 44);
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, title);
    lv_obj_set_width(l, LV_PCT(100));
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_obj_center(l);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, (void *)(intptr_t)value);
    return b;
}
static void tell(const char *message, unsigned duration) {
    if (note)
        lv_label_set_text(note, message);
    notice_started = lv_tick_get();
    notice_duration = duration;
}
static void feedback(esp_err_t e) {
    tell(e == ESP_OK                ? "Request queued..."
         : e == ESP_ERR_INVALID_ARG ? "Check SSID/password length and try again"
                                    : "Connection service busy; try again",
         e == ESP_OK ? 1000 : 6000);
}
static void back(lv_event_t *e) {
    (void)e;
    if (page == PAGE_MAIN || (page == PAGE_DEVICE && device_root))
        cyd_connections_close();
    else if (page == PAGE_TOKEN_CONFIRM)
        render(PAGE_DEVICE);
    else if (page == PAGE_FORGET)
        render(PAGE_JOIN);
    else if (page == PAGE_EDIT)
        render(field == EDIT_AP_SSID || field == EDIT_AP_PASSWORD ? PAGE_AP : PAGE_JOIN);
    else
        render(PAGE_MAIN);
}
static void open_page(lv_event_t *e) {
    page_t p = (page_t)(intptr_t)lv_event_get_user_data(e);
    if (p == PAGE_DEVICE)
        show_token = false;
    if (p == PAGE_AP) {
        cyd_network_ap_credentials(ap_ssid, sizeof(ap_ssid), ap_password, sizeof(ap_password));
        show_ap_password = false;
    }
    if (p == PAGE_JOIN) {
        join_ssid[0] = 0;
        wipe(join_password, sizeof(join_password));
        secured = false;
    }
    render(p);
}
static void scan(lv_event_t *e) {
    (void)e;
    cyd_net_status_t s;
    cyd_network_snapshot(&s);
    scan_revision = s.scan_revision;
    esp_err_t err = cyd_network_scan();
    waiting_scan = err == ESP_OK;
    render(PAGE_SCAN);
    feedback(err);
}
static void choose_scan(lv_event_t *e) {
    unsigned i = (unsigned)(uintptr_t)lv_event_get_user_data(e);
    if (i >= CYD_WIFI_SCAN_MAX)
        return;
    snprintf(join_ssid, sizeof(join_ssid), "%s", rendered_scan[i].ssid);
    secured = rendered_scan[i].secured;
    wipe(join_password, sizeof(join_password));
    render(PAGE_JOIN);
}
static void choose_saved(lv_event_t *e) {
    unsigned i = (unsigned)(uintptr_t)lv_event_get_user_data(e);
    if (i >= CYD_WIFI_PROFILES)
        return;
    snprintf(join_ssid, sizeof(join_ssid), "%s", rendered_saved[i]);
    secured = true;
    wipe(join_password, sizeof(join_password));
    render(PAGE_JOIN);
}
static void connect_saved(lv_event_t *e) {
    (void)e;
    esp_err_t err = cyd_network_join_saved(join_ssid);
    wipe(join_password, sizeof(join_password));
    render(PAGE_MAIN);
    feedback(err);
}
static void connect_entered(lv_event_t *e) {
    (void)e;
    if (!cyd_wifi_valid_ssid(join_ssid) || !cyd_wifi_valid_password(join_password, false) ||
        (secured && !join_password[0])) {
        tell("Enter an SSID and password (8-63 characters, or a 64-digit hex key)", 6000);
        return;
    }
    esp_err_t err = cyd_network_join(join_ssid, join_password);
    wipe(join_password, sizeof(join_password));
    render(PAGE_MAIN);
    feedback(err);
}
static void ask_forget(lv_event_t *e) {
    (void)e;
    render(PAGE_FORGET);
}
static void forget(lv_event_t *e) {
    (void)e;
    esp_err_t err = cyd_network_forget(join_ssid);
    wipe(join_password, sizeof(join_password));
    render(PAGE_MAIN);
    feedback(err);
}
static void station_action(lv_event_t *e) {
    bool reconnect = (intptr_t)lv_event_get_user_data(e);
    feedback(reconnect ? cyd_network_reconnect() : cyd_network_disconnect());
}
static void clear_editor(lv_event_t *e) {
    (void)e;
    lv_textarea_set_text(editor, "");
}
static void edit(lv_event_t *e) {
    field = (field_t)(intptr_t)lv_event_get_user_data(e);
    render(PAGE_EDIT);
}
static void keyboard(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CANCEL) {
        render(field == EDIT_AP_SSID || field == EDIT_AP_PASSWORD ? PAGE_AP : PAGE_JOIN);
        return;
    }
    if (code != LV_EVENT_READY)
        return;
    const char *v = lv_textarea_get_text(editor);
    bool ap = field == EDIT_AP_SSID || field == EDIT_AP_PASSWORD;
    if (field == EDIT_SSID || field == EDIT_AP_SSID) {
        if (!cyd_wifi_valid_ssid(v)) {
            lv_label_set_text(edit_title, "SSID must be 1-32 bytes");
            return;
        }
        snprintf(ap ? ap_ssid : join_ssid, 33, "%s", v);
    } else {
        if (!cyd_wifi_valid_password(v, ap)) {
            lv_label_set_text(edit_title,
                              ap ? "AP password needs 12-63 characters"
                                 : "Use 8-63 characters, 64 hex digits, or blank for open Wi-Fi");
            return;
        }
        snprintf(ap ? ap_password : join_password, ap ? sizeof(ap_password) : sizeof(join_password),
                 "%s", v);
    }
    render(ap ? PAGE_AP : PAGE_JOIN);
}
static void ap_action(lv_event_t *e) {
    int action = (intptr_t)lv_event_get_user_data(e);
    cyd_net_status_t s;
    cyd_network_snapshot(&s);
    bool enabled = action == 0 ? s.ap_enabled : action == 1 ? !s.ap_enabled : true;
    esp_err_t err = cyd_network_set_ap(ap_ssid, ap_password, enabled, action == 2);
    render(PAGE_MAIN);
    feedback(err);
    wipe(ap_password, sizeof(ap_password));
}
static void reveal(lv_event_t *e) {
    (void)e;
    show_ap_password = !show_ap_password;
    render(PAGE_AP);
}
static void token_toggle(lv_event_t *e) {
    (void)e;
    show_token = !show_token;
    render(PAGE_DEVICE);
}
static void token_rotate(lv_event_t *e) {
    (void)e;
    esp_err_t err = cyd_access_rotate();
    show_token = err == ESP_OK;
    render(PAGE_DEVICE);
    tell(err == ESP_OK ? "New token saved. Sign in again in Safari."
                       : "Could not change the token during maintenance",
         6000);
}
static void os_action(lv_event_t *e) {
    int action = (intptr_t)lv_event_get_user_data(e);
    esp_err_t err = action == 0   ? cyd_ota_reboot()
                    : action == 1 ? cyd_ota_cancel()
                                  : cyd_ota_rollback();
    render(PAGE_DEVICE);
    feedback(err);
}
static void update_os(void) {
    if (!os_label)
        return;
    cyd_ota_status_t s;
    cyd_ota_snapshot(&s);
    char b[320];
    snprintf(b, sizeof(b), "CYD OS %s / %s\n%s\nUpload: %u / %u KiB", s.version, s.running_slot,
             s.message, (unsigned)s.received / 1024, (unsigned)s.total / 1024);
    connection_text(os_label, b);
}
static void update_links(const cyd_net_status_t *s) {
    if (!link_label)
        return;
    char b[320];
    const char *state = s->state == CYD_NET_CONNECTED    ? "connected"
                        : s->state == CYD_NET_CONNECTING ? "connecting"
                        : s->state == CYD_NET_FAILED     ? "connection failed"
                                                         : "offline";
    snprintf(b, sizeof(b), "Wi-Fi: %s%s%s\nWi-Fi IP: %s\nAP: %s%s%s\nAP IP: %s", state,
             s->ssid[0] ? " / " : "", s->ssid, s->station_ip[0] ? s->station_ip : "none",
             s->ap_enabled ? "on / " : "off", s->ap_enabled ? s->ap_ssid : "",
             s->ready ? "" : " (starting)", s->ap_enabled && s->ap_ip[0] ? s->ap_ip : "none");
    connection_text(link_label, b);
}
static void render(page_t next) {
    page = next;
    notice_duration = 0;
    lv_obj_clean(view);
    body = note = link_label = editor = edit_title = os_label = NULL;
    cyd_net_status_t s;
    cyd_network_snapshot(&s);
    if (page == PAGE_EDIT) {
        const bool ap = field == EDIT_AP_SSID || field == EDIT_AP_PASSWORD;
        const bool pass = field == EDIT_PASSWORD || field == EDIT_AP_PASSWORD;
        const char *title = pass ? (ap ? "AP password: 12-63 characters" : "Wi-Fi password")
                                 : (ap ? "Access point name" : "Network name (SSID)");
        edit_title = lv_label_create(view);
        lv_label_set_text(edit_title, title);
        lv_obj_set_pos(edit_title, 4, 1);
        lv_obj_set_width(edit_title, width - 8);
        lv_label_set_long_mode(edit_title, LV_LABEL_LONG_DOT);
        editor = lv_textarea_create(view);
        lv_obj_set_pos(editor, 4, 20);
        lv_obj_set_size(editor, width - 72, 44);
        lv_textarea_set_one_line(editor, true);
        lv_textarea_set_max_length(editor, pass ? (ap ? 63 : 64) : 32);
        lv_textarea_set_password_mode(editor, pass);
        lv_textarea_set_text(editor, pass ? (ap ? ap_password : join_password)
                                          : (ap ? ap_ssid : join_ssid));
        lv_obj_t *clear = lv_button_create(view);
        lv_obj_set_pos(clear, width - 64, 24);
        lv_obj_set_size(clear, 60, 36);
        lv_obj_set_style_pad_all(clear, 2, 0);
        lv_obj_t *clear_label = lv_label_create(clear);
        lv_label_set_text(clear_label, "Clear");
        lv_obj_center(clear_label);
        lv_obj_add_event_cb(clear, clear_editor, LV_EVENT_CLICKED, NULL);
        lv_obj_t *keys = lv_keyboard_create(view);
        lv_obj_set_align(keys, LV_ALIGN_TOP_LEFT);
        lv_obj_set_pos(keys, 0, 72);
        lv_obj_set_size(keys, width, height - 72);
        lv_keyboard_set_textarea(keys, editor);
        lv_obj_add_event_cb(keys, keyboard, LV_EVENT_ALL, NULL);
        return;
    }
    lv_obj_t *b = lv_button_create(view);
    lv_obj_set_pos(b, 4, 2);
    lv_obj_set_size(b, 62, 30);
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, "Up");
    lv_obj_center(l);
    lv_obj_add_event_cb(b, back, LV_EVENT_CLICKED, NULL);
    const char *title = page == PAGE_MAIN     ? "Connections"
                        : page == PAGE_SCAN   ? "Nearby networks"
                        : page == PAGE_SAVED  ? "Saved networks"
                        : page == PAGE_JOIN   ? "Join Wi-Fi"
                        : page == PAGE_FORGET ? "Forget network"
                        : page == PAGE_DEVICE ? "Device and OS"
                                              : "CYD access point";
    l = lv_label_create(view);
    lv_label_set_text(l, title);
    lv_obj_set_pos(l, 76, 8);
    lv_obj_set_width(l, width - 84);
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    body = lv_obj_create(view);
    lv_obj_set_pos(body, 0, 36);
    lv_obj_set_size(body, width, height - 36);
    lv_obj_set_style_pad_all(body, 8, 0);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(body, 8, 0);
    note = text(body, s.scanning     ? "Scanning..."
                      : s.message[0] ? s.message
                                     : "Starting Wi-Fi service...");
    if (page == PAGE_MAIN) {
        button(body, "Scan nearby networks", scan, 0);
        link_label = text(body, "");
        update_links(&s);
        button(body, "Saved networks", open_page, PAGE_SAVED);
        button(body, "Join hidden / other network", open_page, PAGE_JOIN);
        button(body, "CYD access point settings", open_page, PAGE_AP);
        button(body, "Device access / OS updates", open_page, PAGE_DEVICE);
        button(body, "Reconnect saved Wi-Fi", station_action, 1);
        button(body, "Disconnect Wi-Fi", station_action, 0);
        text(body, "Connect your iPad to the phone hotspot or the CYD AP. Open the matching IP "
                   "above in Safari.");
    } else if (page == PAGE_SCAN) {
        button(body, waiting_scan ? "Scan requested..." : "Scan again", scan, 0);
        if (!waiting_scan) {
            memcpy(rendered_scan, s.scan, sizeof(rendered_scan));
            for (unsigned i = 0; i < s.scan_count; i++) {
                char title[96];
                snprintf(title, sizeof(title), "%s / %d dBm / %s", s.scan[i].ssid, s.scan[i].rssi,
                         s.scan[i].supported ? (s.scan[i].secured ? "secured" : "open")
                                             : "unsupported");
                lv_obj_t *b = button(body, title, choose_scan, i);
                if (!s.scan[i].supported)
                    lv_obj_add_state(b, LV_STATE_DISABLED);
            }
            if (!s.scan_count)
                text(body,
                     "No supported networks found. Try scanning again, or enter a hidden SSID.");
        }
        button(body, "Enter network name", open_page, PAGE_JOIN);
    } else if (page == PAGE_SAVED) {
        memcpy(rendered_saved, s.saved, sizeof(rendered_saved));
        for (unsigned i = 0; i < s.saved_count; i++)
            button(body, s.saved[i], choose_saved, i);
        if (!s.saved_count)
            text(body, "No saved networks. Successful connections are remembered automatically.");
        text(body, "Up to five networks. Last successful network is tried first; other saved "
                   "networks are tried if it is unavailable.");
    } else if (page == PAGE_JOIN) {
        text(body, join_ssid[0] ? join_ssid : "Enter an SSID to join a hidden or open network.");
        button(body, join_ssid[0] ? "Edit network name" : "Enter network name", edit, EDIT_SSID);
        button(body, join_password[0] ? "Password entered (tap to edit)" : "Enter Wi-Fi password",
               edit, EDIT_PASSWORD);
        bool saved = false;
        for (unsigned i = 0; i < s.saved_count; i++)
            if (!strcmp(s.saved[i], join_ssid))
                saved = true;
        if (saved)
            button(body, "Connect using saved password", connect_saved, 0);
        button(body, "Connect and save on success", connect_entered, 0);
        if (saved) {
            button(body, "Forget this saved network", ask_forget, 0);
            text(body, "Forgetting removes its credentials and disconnects it if active.");
        }
        text(body, "Use the keyboard checkmark to accept text. Leave the password blank only for "
                   "an open network.");
    } else if (page == PAGE_DEVICE) {
        os_label = text(body, "");
        update_os();
        char access[CYD_ACCESS_MAX_TOKEN + 1] = {0};
        if (show_token) {
            cyd_access_get_token(access, sizeof(access));
            text(body, access);
            wipe(access, sizeof(access));
        }
        button(body, show_token ? "Hide dashboard token" : "Show dashboard deployment token",
               token_toggle, 0);
        text(body, "Changing the token signs out existing dashboard/CLI sessions.");
        button(body, "Generate a new deployment token", open_page, PAGE_TOKEN_CONFIRM);
        cyd_ota_status_t os;
        cyd_ota_snapshot(&os);
        if (os.stage == CYD_OS_READY) {
            button(body, "Restart into the verified OS", os_action, 0);
            button(body, "Cancel staged OS update", os_action, 1);
        }
        if (os.boot_confirmed && os.rollback_available && os.stage == CYD_OS_IDLE) {
            text(body, "Restore previous OS restarts the device and keeps SD data.");
            button(body, "Restore previous confirmed OS", os_action, 2);
        }
        text(body, "Upload OS application .bin files from Safari's dashboard. They are separate "
                   "from Lua .cydapp packages.");
    } else if (page == PAGE_TOKEN_CONFIRM) {
        text(body, "Replace the dashboard token? Existing Safari and CLI sessions will need the "
                   "new token.");
        button(body, "Replace token", token_rotate, 0);
        button(body, "Cancel", back, 0);
    } else if (page == PAGE_FORGET) {
        text(body, join_ssid);
        text(body, "Remove this network's saved credentials? An active connection will stop. You "
                   "will need its password to save it again.");
        button(body, "Forget network", forget, 0);
        button(body, "Cancel", back, 0);
    } else if (page == PAGE_AP) {
        char b[180];
        snprintf(b, sizeof(b), "Name: %s\nPassword: %s", ap_ssid,
                 show_ap_password ? ap_password : "hidden");
        text(body, b);
        button(body, "Edit access point name", edit, EDIT_AP_SSID);
        button(body, "Edit access point password", edit, EDIT_AP_PASSWORD);
        button(body, show_ap_password ? "Hide password" : "Show password for your iPad", reveal, 0);
        button(body, "Save AP settings", ap_action, 0);
        button(body, s.ap_enabled ? "Stop CYD access point" : "Start CYD access point", ap_action,
               1);
        button(body, "Use CYD AP only (disconnect Wi-Fi)", ap_action, 2);
        text(body, "WPA2 protected; password needs 12-63 characters. AP address is normally "
                   "192.168.4.1. AP changes can disconnect clients. AP mode provides a local "
                   "network, not internet routing.");
    }
    cyd_pager_attach(body);
}
static void poll(lv_timer_t *timer) {
    (void)timer;
    if (lv_obj_has_flag(view, LV_OBJ_FLAG_HIDDEN) || page == PAGE_EDIT)
        return;
    cyd_net_status_t s;
    cyd_network_snapshot(&s);
    if (waiting_scan && s.scan_revision != scan_revision) {
        waiting_scan = false;
        if (page == PAGE_SCAN) {
            render(PAGE_SCAN);
            return;
        }
    }
    if (note && lv_tick_elaps(notice_started) >= notice_duration)
        connection_text(note, s.scanning ? "Scanning nearby networks..." : s.message);
    update_links(&s);
    update_os();
    cyd_pager_refresh(body);
}
void cyd_connections_init(lv_obj_t *parent, unsigned w, unsigned h) {
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
    lv_timer_create(poll, 400, NULL);
}
void cyd_connections_open(void) {
    device_root = false;
    waiting_scan = false;
    wipe(join_password, sizeof(join_password));
    wipe(ap_password, sizeof(ap_password));
    render(PAGE_MAIN);
    lv_obj_remove_flag(view, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(view);
}
void cyd_connections_close(void) {
    if (!view)
        return;
    lv_obj_add_flag(view, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clean(view);
    body = note = link_label = editor = edit_title = os_label = NULL;
    wipe(join_password, sizeof(join_password));
    wipe(ap_password, sizeof(ap_password));
}

void cyd_connections_open_device(void) {
    cyd_connections_open();
    device_root = true;
    show_token = false;
    render(PAGE_DEVICE);
}
void cyd_connections_resize(unsigned w, unsigned h) {
    width = w;
    height = h - 56;
    lv_obj_set_size(view, width, height);
    if (!lv_obj_has_flag(view, LV_OBJ_FLAG_HIDDEN))
        render(page);
}

bool cyd_connections_nav(int direction,cyd_nav_state_t *state) {
    if(!view || lv_obj_has_flag(view,LV_OBJ_FLAG_HIDDEN))return false;
    cyd_pager_refresh(body);
    if(direction && !cyd_pager_move(body,direction) && direction<0)back(NULL);
    cyd_pager_state(body,state);state->active=true;state->back=true;return true;
}
