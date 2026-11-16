#define main legacy_memory_main
#include "ui_memory_host.c"
#undef main
#include "cyd_pair.h"
#include "pair_ops.h"
#include "cyd_pair_manager.h"
#include "cyd_pair_ble.h"
#include "cyd_builtin.h"
#include "pair_ui.h"
#include <math.h>
static unsigned launched = 99, deletes, renames, copies, commits, rpc_calls;
static char last_path[181], last_target[181];
static bool fail_copy, file_long;
static unsigned last_read_offset;
static const char text[] = "Saved note\n";
unsigned cyd_builtin_count(void) {
    return CYD_PAIR_HOST ? 5 : 1;
}
const char *cyd_builtin_name(unsigned n) {
    (void)n;
    return "Fixture";
}
const char *cyd_builtin_key(unsigned n) {
    (void)n;
    return "fixture";
}
esp_err_t cyd_builtin_launch(unsigned n) {
    launched = n;
    return ESP_OK;
}
cJSON *cyd_pair_json(unsigned op, const cJSON *request) {
    rpc_calls++;
    (void)request;
    if (op == CYD_PAIR_SETTINGS_GET)
        return cJSON_Parse(
            "{\"brightness\":70,\"rotation\":0,\"dim_seconds\":0,\"dim_percent\":10}");
    if (op == CYD_PAIR_LOGS)
        return cJSON_Parse("[\"Host fixture\"]");
    return cJSON_CreateObject();
}
esp_err_t cyd_pair_manager_remember(const cJSON *o) {
    (void)o;
    return ESP_OK;
}
cJSON *cyd_pair_ble_status(void) {
    return cJSON_Parse("{\"message\":\"BLE fixture\",\"devices\":[]}");
}
esp_err_t cyd_pair_ble_scan(void) {
    return 0;
}
esp_err_t cyd_pair_ble_connect(unsigned n) {
    (void)n;
    return 0;
}
esp_err_t cyd_pair_ble_disconnect(void) {
    return 0;
}
static esp_err_t service(unsigned op, const void *in, size_t n, void *out, size_t cap,
                         size_t *got) {
    *got = 0;
    if (op == CYD_PAIR_FILE_PUT) {
        assert(n >= 6);
        const uint8_t *p = in;
        unsigned k = p[4] | (unsigned)p[5] << 8;
        assert(k == strlen("documents/note.txt"));
        assert(!memcmp(p + 6, "documents/note.txt", k));
        assert(n - 6 - k == sizeof(text) - 1);
        assert(!memcmp(p + 6 + k, text, sizeof(text) - 1));
        copies++;
        return fail_copy ? ESP_ERR_TIMEOUT : ESP_OK;
    }
    cJSON *o = cJSON_ParseWithLength(in, n);
    assert(o);
    const cJSON *p = cJSON_GetObjectItemCaseSensitive(o, "path");
    assert(cJSON_IsString(p));
    snprintf(last_path, sizeof(last_path), "%s", p->valuestring);
    const char *answer = NULL;
    if (op == CYD_PAIR_FILE_LIST) {
        if (!*last_path)
            answer = "[{\"name\":\"documents\",\"directory\":true}]";
        else
            answer = "[{\"name\":\"note.txt\",\"size\":11,\"directory\":false}]";
    } else if (op == CYD_PAIR_FILE_READ) {
        const cJSON *off = cJSON_GetObjectItemCaseSensitive(o, "offset");
        assert(off);
        last_read_offset = (unsigned)off->valueint;
        if (file_long) {
            unsigned char data[2053];
            memset(data, 'A', 2046);
            memcpy(data + 2046, "\xe2\x82\xactail", 7);
            size_t take = sizeof(data) > last_read_offset ? sizeof(data) - last_read_offset : 0;
            if (take > 2048)
                take = 2048;
            assert(take <= cap);
            memcpy(out, data + last_read_offset, take);
            *got = take;
        } else if (off->valueint == 0) {
            assert(cap >= sizeof(text) - 1);
            memcpy(out, text, sizeof(text) - 1);
            *got = sizeof(text) - 1;
        }
    }

    else if (op == CYD_PAIR_FILE_REMOVE)
        deletes++;
    else if (op == CYD_PAIR_FILE_RENAME) {
        renames++;
        const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, "target");
        assert(cJSON_IsString(v));
        snprintf(last_target, sizeof(last_target), "%s", v->valuestring);
    } else if (op == CYD_PAIR_FILE_COMMIT)
        commits++;
    else
        assert(op == CYD_PAIR_FILE_WRITE);
    if (answer) {
        *got = strlen(answer);
        assert(*got <= cap);
        memcpy(out, answer, *got);
    }
    cJSON_Delete(o);
    return ESP_OK;
}
esp_err_t cyd_pair_rpc(unsigned op, const void *p, size_t n, void *out, size_t cap, size_t *got) {
    return service(op, p, n, out, cap, got);
}
esp_err_t cyd_pair_dispatch(unsigned op, const void *p, size_t n, void *out, size_t cap,
                            size_t *got) {
    return service(op, p, n, out, cap, got);
}
const char *cyd_pair_string(const cJSON *o, const char *key) {
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, key);
    return cJSON_IsString(v) ? v->valuestring : "";
}
bool cyd_pair_number(const cJSON *o, const char *key, int64_t a, int64_t b, int64_t *value) {
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, key);
    if (!cJSON_IsNumber(v) || v->valuedouble < a || v->valuedouble > b ||
        floor(v->valuedouble) != v->valuedouble)
        return false;
    *value = v->valuedouble;
    return true;
}
static void traced_click(const char *text, bool footer) {
    fprintf(stderr, "Click: %s (%s)\n", text, footer ? "footer" : "view");
    click(text, footer);
}
#define click traced_click
static lv_obj_t *find_type(lv_obj_t *o, const lv_obj_class_t *type) {
    if (lv_obj_check_type(o, type))
        return o;
    for (unsigned i = 0; i < lv_obj_get_child_count(o); i++) {
        lv_obj_t *r = find_type(lv_obj_get_child(o, i), type);
        if (r)
            return r;
    }
    return NULL;
}
static lv_obj_t *find_text(lv_obj_t *o, const char *value) {
    if (lv_obj_check_type(o, &lv_label_class) && !strcmp(lv_label_get_text(o), value))
        return o;
    for (unsigned i = 0; i < lv_obj_get_child_count(o); i++) {
        lv_obj_t *v = find_text(lv_obj_get_child(o, i), value);
        if (v)
            return v;
    }
    return NULL;
}
static void open_note(void) {
    click(LV_SYMBOL_DIRECTORY, false);
    click("System drive (S3)", false);
    click(LV_SYMBOL_DIRECTORY " documents", false);
    click("note.txt", false);
    if (!file_long) {
        lv_obj_t *text_label = find_text(lv_screen_active(), text);
        assert(text_label && visible(text_label));
    }
}
int main(void) {
    assert(cyd_ui_init() == ESP_OK);
    extern bool host_has_task(const char *);
    assert(host_has_task("app_folders") == (bool)CYD_PAIR_HOST);
    for (unsigned rotation = 0; rotation < 4; rotation++) {
        assert(!cyd_ui_apply_rotation(rotation));
        render();
        footer_bounds();
        if (CYD_PAIR_HOST) {
            click(LV_SYMBOL_PLUS, false);
            assert(launched == 0);
            click(LV_SYMBOL_LIST, false);
            assert(launched == 1);
            unsigned calls_before = rpc_calls;
            click(LV_SYMBOL_WARNING, false); /* S3 local recovery without a C5. */
            lv_obj_t *log_text = find_text(lv_screen_active(), "Ready for wireless development\n");
            assert(log_text && visible(log_text) && rpc_calls == calls_before);
            click("Home", true);
            char shot[64];
            snprintf(shot, sizeof(shot), "paired-host-home-r%u", rotation);
            save(shot);
        } else {
            assert(find_button(lv_screen_active(), LV_SYMBOL_EYE_OPEN, false));
            click(LV_SYMBOL_EYE_OPEN,false);cyd_nav_state_t nav;assert(cyd_settings_nav(0,&nav)&&nav.page==1);click("Back",true);assert(!cyd_settings_nav(0,&nav));
            click(LV_SYMBOL_SETTINGS,false);click("Diagnostics",false);click("Back",true);assert(cyd_settings_nav(0,&nav));click("Home",true);

            char shot[64];
            snprintf(shot, sizeof(shot), "paired-manager-home-r%u", rotation);
            save(shot);
            open_note();
            snprintf(shot, sizeof(shot), "paired-manager-files-r%u", rotation);
            save(shot);
            click("Delete...", false);
            click("Cancel", false);
            assert(!deletes);
            click("Home", true);
            open_note();
            click("Copy to other drive", false);
            assert(copies == rotation * 2 + 1 && commits == rotation + 1 && !deletes);
            fail_copy = true;
            click("Copy to other drive", false);
            assert(copies == rotation * 2 + 2 && commits == rotation + 1 && !deletes);
            fail_copy = false;
            click("Rename", false);
            lv_obj_t *ta = find_type(lv_screen_active(), &lv_textarea_class),
                     *kb = find_type(lv_screen_active(), &lv_keyboard_class);
            assert(ta && kb);
            lv_textarea_set_text(ta, "renamed.txt");
            lv_obj_send_event(kb, LV_EVENT_READY, NULL);
            host_run_task("shell_actions");
            render();
            assert(renames == rotation + 1);
            assert(!strcmp(last_path, "documents/note.txt") &&
                   !strcmp(last_target, "documents/renamed.txt"));
            click("Home", true);
            file_long = true;
            open_note();
            click("Next 2 KiB", false);
            assert(last_read_offset == 2046);
            file_long = false;
            click("Home", true);
            click(LV_SYMBOL_SETTINGS, false);
            click("S3 display settings", false);
            assert(find_button(lv_screen_active(), "Brightness +10%", false));
            click("Home", true);
        }
    }
    lv_deinit();
    free(buffers[0]);
    free(buffers[1]);
    puts("PASS paired role launchers, both-drive file routes, copy failure, rename and delete "
         "confirmation across rotations");
    return 0;
}
