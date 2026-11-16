#include "cyd_pair.h"
#include "pair_ops.h"
#include "cyd_pair_ble.h"
#include "cyd_pair_manager.h"
#ifdef ESP_PLATFORM
#include "freertos/task.h"
#endif
#include "pair_internal.h"
#include "pair_ui.h"
#include "esp_heap_caps.h"
#include "mbedtls/sha256.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static lv_obj_t *view, *body, *title, *notice, *edit;
static unsigned width, height, drive;
static bool busy;
static char path[181], selected[181], draft[241], filename[49];
static int editing;
static size_t read_offset, next_offset;
static atomic_bool cancel_copy;
static void draw(void);
static bool text_chunk(const uint8_t *p, size_t n, bool more, size_t *consumed) {
    *consumed = n;
    for (size_t i = 0; i < n;) {
        uint8_t a = p[i];
        size_t length = 1;
        if (a < 32 && a != 9 && a != 10 && a != 13)
            return false;
        if (a >= 128) {
            if (a >= 194 && a <= 223)
                length = 2;
            else if (a >= 224 && a <= 239)
                length = 3;
            else if (a >= 240 && a <= 244)
                length = 4;
            else
                return false;
            if (length > n - i) {
                if (more && i) {
                    *consumed = i;
                    return true;
                }
                return false;
            }
            uint8_t b = p[i + 1];
            if ((a == 224 && b < 160) || (a == 237 && b > 159) || (a == 240 && b < 144) ||
                (a == 244 && b > 143))
                return false;
            for (size_t j = 1; j < length; j++)
                if (p[i + j] < 128 || p[i + j] > 191)
                    return false;
        }
        i += length;
    }
    return true;
}

static void post(int action) {
    if (busy)
        return;
    busy = true;
    if (notice) {
        lv_label_set_text(notice, "Working...");
        cyd_pager_refresh(body);
    }
    if (!cyd_ui_post_action(action)) {
        busy = false;
        lv_label_set_text(notice, "Action queue busy; try again");
    }
}
static void click(lv_event_t *e) {
    if (busy)
        return;
    int action = (int)(intptr_t)lv_event_get_user_data(e);
    if (action == 700 || action == 701) {
        drive = (unsigned)(action - 700);
        path[0] = 0;
        post(710);
        return;
    }
    if (action == 702) {
        cyd_pair_ui_close();
        return;
    }
    if (action == 703) {
        char *slash = strrchr(path, '/');
        if (slash)
            *slash = 0;
        else
            path[0] = 0;
        post(710);
        return;
    }
    if (action == 715) {
        read_offset = next_offset;
        post(711);
        return;
    }
    if (action == 718) {
        read_offset = 0;
        post(711);
        return;
    }
    if (action == 704) {
        filename[0] = draft[0] = 0;
        editing = 1;
        draw();
        return;
    }
    if (action == 707) {
        editing = 4;
        draw();
        return;
    }
    if (action == 708) {
        editing = 5;
        snprintf(filename, sizeof(filename), "%.48s",
                 strrchr(selected, '/') ? strrchr(selected, '/') + 1 : selected);
        draw();
        return;
    }
    if (action == 705) {
        editing = 3;
        draw();
        return;
    }
    if (action == 706) {
        editing = 0;
        draw();
        return;
    }
    post(action);
}
static lv_obj_t *label(lv_obj_t *parent, const char *text) {
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, text);
    lv_obj_set_width(l, LV_PCT(100));
    return l;
}
static void button(lv_obj_t *parent, const char *text, int action) {
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_set_size(b, LV_PCT(100), 42);
    lv_obj_t *l = label(b, text);
    lv_obj_center(l);
    lv_obj_add_event_cb(b, click, LV_EVENT_CLICKED, (void *)(intptr_t)action);
}
typedef struct {
    bool directory;
    char name[97];
} file_entry_t;
static void entry_free(lv_event_t *e) {
    lv_free(lv_event_get_user_data(e));
}
static void entry(lv_event_t *e) {
    if (busy)
        return;
    const file_entry_t *item = lv_event_get_user_data(e);
    if (!item)
        return;
    if (snprintf(selected, sizeof(selected), "%s%s%s", path, path[0] ? "/" : "", item->name) >=
        (int)sizeof(selected))
        return;
    if (item->directory) {
        strcpy(path, selected);
        post(710);
    } else {
        read_offset = 0;
        post(711);
    }
}
static void keyboard(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CANCEL) {
        editing = 0;
        draw();
        return;
    }
    if (lv_event_get_code(e) != LV_EVENT_READY)
        return;
    if (editing == 1) {
        snprintf(filename, sizeof(filename), "%s", lv_textarea_get_text(edit));
        editing = 2;
        draw();
    } else if (editing == 5) {
        snprintf(filename, sizeof(filename), "%s", lv_textarea_get_text(edit));
        editing = 0;
        post(714);
    } else if (editing == 4) {
        snprintf(draft, sizeof(draft), "%s", lv_textarea_get_text(edit));
        editing = 0;
        post(716);
    } else if (editing == 2) {
        snprintf(draft, sizeof(draft), "%s", lv_textarea_get_text(edit));
        editing = 0;
        post(712);
    }
}
static void base(void) {
    lv_obj_clean(view);
    lv_obj_remove_flag(view, LV_OBJ_FLAG_HIDDEN);
    title = lv_label_create(view);
    lv_obj_set_pos(title, 8, 4);
    lv_obj_set_width(title, width - 16);
    body = lv_obj_create(view);
    lv_obj_set_pos(body, 0, 30);
    lv_obj_set_size(body, width, height - 30);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(body, 8, 0);
    lv_obj_set_style_pad_row(body, 8, 0);
    notice = label(body, "");
}
static void draw(void) {
    base();
    lv_label_set_text(title, drive == 0 ? "Files — System (S3)" : "Files — Device (C5)");
    if (editing == 1 || editing == 2 || editing == 4 || editing == 5) {
        lv_label_set_text(notice, (editing == 1 || editing == 5) ? "Lowercase filename"
                                                                 : "Text (up to 240 bytes)");
        edit = lv_textarea_create(body);
        lv_obj_set_width(edit, LV_PCT(100));
        lv_obj_set_height(edit, 50);
        lv_textarea_set_max_length(edit, (editing == 1 || editing == 5) ? 48 : 240);
        lv_textarea_set_text(edit, (editing == 1 || editing == 5) ? filename : draft);
        lv_obj_t *k = lv_keyboard_create(view);
        lv_obj_set_pos(k, 0, height / 2);
        lv_obj_set_size(k, width, height - height / 2);
        lv_keyboard_set_textarea(k, edit);
        lv_obj_add_event_cb(k, keyboard, LV_EVENT_ALL, NULL);
        return;
    }
    if (editing == 3) {
        label(body, "Delete this file? This cannot be undone.");
        label(body, selected);
        button(body, "Delete", 713);
        button(body, "Cancel", 706);
    } else {
        button(body, "System drive (S3)", 700);
        button(body, "Device drive (C5)", 701);
        button(body, "Close", 702);
    }
    cyd_pager_attach(body);
}
void cyd_pair_ui_init(lv_obj_t *parent, unsigned w, unsigned h) {
    width = w;
    height = h;
    view = lv_obj_create(parent);
    lv_obj_set_pos(view, 0, 32);
    lv_obj_set_size(view, w, h);
    lv_obj_remove_flag(view, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(view, 0, 0);
    lv_obj_add_flag(view, LV_OBJ_FLAG_HIDDEN);
}
void cyd_pair_ui_resize(unsigned w, unsigned h) {
    width = w;
    height = h;
    if (view) {
        lv_obj_set_size(view, w, h);
        cyd_pair_ui_close();
    }
}
void cyd_pair_ui_close(void) {
    atomic_store(&cancel_copy, true);
    if (view)
        lv_obj_add_flag(view, LV_OBJ_FLAG_HIDDEN);
}
bool cyd_pair_ui_nav(int direction, cyd_nav_state_t *state) {
    if (!view || lv_obj_has_flag(view, LV_OBJ_FLAG_HIDDEN))
        return false;
    if (direction && !busy && !cyd_pager_move(body, direction) && direction < 0) {
        cyd_pair_ui_close();
    }
    cyd_pager_state(body, state);
    state->active = true;
    state->back = true;
    return true;
}
static esp_err_t request(unsigned op, cJSON *o, void *out, size_t cap, size_t *n) {
    char *s = cJSON_PrintUnformatted(o);
    if (!s)
        return ESP_ERR_NO_MEM;
    esp_err_t e = drive == 0 ? cyd_pair_rpc(op, s, strlen(s), out, cap, n)
                             : cyd_pair_dispatch(op, s, strlen(s), out, cap, n);
    free(s);
    return e;
}
static esp_err_t drive_call(unsigned target, unsigned op, const void *in, size_t n, void *out,
                            size_t cap, size_t *got) {
    return target == 0 ? cyd_pair_rpc(op, in, n, out, cap, got)
                       : cyd_pair_dispatch(op, in, n, out, cap, got);
}
static esp_err_t copy_to_other_drive(void) {
    const char *base_name = strrchr(selected, '/');
    base_name = base_name ? base_name + 1 : selected;
    if (strlen(base_name) > 96)
        return ESP_ERR_INVALID_SIZE;
    char destination[128];
    snprintf(destination, sizeof(destination), "documents/%.96s", base_name);
    uint8_t *buffer = heap_caps_malloc(2240, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    uint8_t *data = heap_caps_malloc(2048, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buffer || !data) {
        free(buffer);
        free(data);
        return ESP_ERR_NO_MEM;
    }
    mbedtls_sha256_context hash;
    mbedtls_sha256_init(&hash);
    mbedtls_sha256_starts(&hash, 0);
    uint32_t offset = 0;
    esp_err_t e = ESP_OK;
    size_t got = 0, path_size = strlen(destination);
    bool first = true;
    for (;;) {
        if (atomic_load(&cancel_copy)) {
            e = ESP_ERR_TIMEOUT;
            break;
        }
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "path", selected);
        cJSON_AddNumberToObject(o, "offset", offset);
        cJSON_AddNumberToObject(o, "length", 2048);
        char *q = cJSON_PrintUnformatted(o);
        cJSON_Delete(o);
        if (!q) {
            e = ESP_ERR_NO_MEM;
            break;
        }
        e = drive_call(drive, CYD_PAIR_FILE_READ, q, strlen(q), data, 2048, &got);
        free(q);
        if (e)
            break;
        if (got > 2 * 1024 * 1024 - offset) {
            e = ESP_ERR_INVALID_SIZE;
            break;
        }
        if (!got && !first)
            break;
        memcpy(buffer, &offset, 4);
        buffer[4] = path_size;
        buffer[5] = path_size >> 8;
        memcpy(buffer + 6, destination, path_size);
        memcpy(buffer + 6 + path_size, data, got);
        size_t ignored = 0;
        e = drive_call(1 - drive, CYD_PAIR_FILE_PUT, buffer, 6 + path_size + got, NULL, 0,
                       &ignored);
        if (e)
            break;
        mbedtls_sha256_update(&hash, data, got);
        offset += (uint32_t)got;
        first = false;
        if (!got)
            break;
    }
    uint8_t digest[32];
    mbedtls_sha256_finish(&hash, digest);
    mbedtls_sha256_free(&hash);
    if (!e) {
        char hex[65];
        for (unsigned i = 0; i < 32; i++)
            snprintf(hex + i * 2, 3, "%02x", digest[i]);
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "path", destination);
        cJSON_AddNumberToObject(o, "size", offset);
        cJSON_AddStringToObject(o, "sha256", hex);
        char *q = cJSON_PrintUnformatted(o);
        cJSON_Delete(o);
        if (!q)
            e = ESP_ERR_NO_MEM;
        else {
            size_t ignored = 0;
            e = drive_call(1 - drive, CYD_PAIR_FILE_COMMIT, q, strlen(q), NULL, 0, &ignored);
            free(q);
        }
    }
    free(buffer);
    free(data);
    return e;
}
bool cyd_pair_ui_action(int action) {
    if (action == 8 || (action >= 820 && action <= 827)) {
        cJSON *settings = cyd_pair_json(CYD_PAIR_SETTINGS_GET, NULL);
        esp_err_t e = settings ? ESP_OK : ESP_ERR_INVALID_STATE;
        if (!e && action != 8) {
            cJSON *command = cJSON_CreateObject();
            int64_t value = 0;
            if (action == 820 || action == 821) {
                cyd_pair_number(settings, "brightness", 10, 100, &value);
                value += action == 820 ? -10 : 10;
                if (value < 10)
                    value = 10;
                if (value > 100)
                    value = 100;
                cJSON_AddStringToObject(command, "action", "brightness");
                cJSON_AddNumberToObject(command, "value", value);
            } else if (action == 822) {
                cyd_pair_number(settings, "rotation", 0, 3, &value);
                cJSON_AddStringToObject(command, "action", "rotation");
                cJSON_AddNumberToObject(command, "value", (value + 1) % 4);
            } else if (action == 825 || action == 826) {
                int64_t seconds = 0, percent = 10;
                cyd_pair_number(settings, "dim_seconds", 0, 300, &seconds);
                cyd_pair_number(settings, "dim_percent", 10, 50, &percent);
                if (action == 825) {
                    const int choices[] = {0, 30, 60, 120, 300};
                    for (unsigned i = 0; i < 5; i++)
                        if (seconds == choices[i]) {
                            seconds = choices[(i + 1) % 5];
                            break;
                        }
                } else
                    percent = percent == 10 ? 25 : percent == 25 ? 50 : 10;
                cJSON_AddStringToObject(command, "action", "preferences");
                cJSON_AddNumberToObject(command, "seconds", seconds);
                cJSON_AddNumberToObject(command, "percent", percent);
            } else
                cJSON_AddStringToObject(command, "action",
                                        action == 827   ? "calibrate"
                                        : action == 823 ? "confirm"
                                                        : "revert");
            cJSON *result = cyd_pair_json(CYD_PAIR_SETTINGS_SET, command);
            if (!result)
                e = ESP_FAIL;
            cJSON_Delete(result);
            cJSON_Delete(command);
            vTaskDelay(pdMS_TO_TICKS(300));
            cJSON_Delete(settings);
            settings = cyd_pair_json(CYD_PAIR_SETTINGS_GET, NULL);
            if (!e && settings &&
                (action == 820 || action == 821 || action == 823 || action == 825 || action == 826))
                e = cyd_pair_manager_remember(settings);
        }
        cyd_ui_lock();
        busy = false;
        base();
        lv_label_set_text(title, "S3 display settings");
        char caption[192];
        cJSON *b = cJSON_GetObjectItemCaseSensitive(settings, "brightness"),
              *rot = cJSON_GetObjectItemCaseSensitive(settings, "rotation");
        snprintf(caption, sizeof(caption), "Brightness: %d%%; orientation: %d degrees",
                 b ? b->valueint : 0, rot ? rot->valueint * 90 : 0);
        lv_label_set_text(
            notice, e ? "Request not confirmed. Check S3 link and C5 settings storage." : caption);
        button(body, "Brightness -10%", 820);
        button(body, "Brightness +10%", 821);
        button(body, "Rotate S3 (preview)", 822);
        button(body, "Keep S3 orientation", 823);
        button(body, "Revert S3 orientation", 824);
        button(body, "Cycle S3 idle delay", 825);
        button(body, "Cycle S3 idle brightness", 826);
        button(body, "Calibrate S3 touch", 827);
        button(body, "Refresh", 8);
        label(body, "Keep orientation within 15 seconds. C5 stores the S3 display profile; "
                    "hardware touch calibration remains on its board.");
        cyd_pager_attach(body);
        cyd_ui_unlock();
        cJSON_Delete(settings);
        return true;
    }
    if (action == 6 || action == 790 || action == 791 || (action >= 800 && action < 816)) {
        esp_err_t e = ESP_OK;
        if (action == 790)
            e = cyd_pair_ble_scan();
        else if (action == 791)
            e = cyd_pair_ble_disconnect();
        else if (action >= 800)
            e = cyd_pair_ble_connect((unsigned)(action - 800));
        cJSON *s = cyd_pair_ble_status();
        cyd_ui_lock();
        busy = false;
        base();
        lv_label_set_text(title, "Bluetooth LE");
        lv_label_set_text(notice, e ? "Request unavailable. Refresh after the controller is ready."
                                    : cyd_pair_string(s, "message"));
        button(body, "Scan (enable Bluetooth)", 790);
        button(body, "Refresh", 6);
        button(body, "Disconnect", 791);
        cJSON *a = cJSON_GetObjectItemCaseSensitive(s, "devices"), *v;
        cJSON_ArrayForEach(v, a) {
            cJSON *i = cJSON_GetObjectItemCaseSensitive(v, "index");
            char caption[96];
            snprintf(caption, sizeof(caption), "%s %s", cyd_pair_string(v, "name"),
                     cyd_pair_string(v, "address"));
            button(body, caption, 800 + (i ? i->valueint : 0));
        }
        label(body,
              "BLE links only. Device-specific services and passkey entry need their own drivers.");
        cyd_pager_attach(body);
        cyd_ui_unlock();
        cJSON_Delete(s);
        return true;
    }
    if (action == 5) {
        cyd_ui_lock();
        busy = false;
        editing = 0;
        draw();
        cyd_ui_unlock();
        return true;
    }
    if (action < 710 || action > 717)
        return false;
    if (action == 717) {
        atomic_store(&cancel_copy, false);
        esp_err_t e = copy_to_other_drive();
        cyd_ui_lock();
        busy = false;
        if (!atomic_load(&cancel_copy))
            lv_label_set_text(
                notice,
                e ? "Copy not confirmed. Check the destination, space and link before retrying."
                  : "Copied to Documents on the other drive");
        cyd_pager_refresh(body);
        cyd_ui_unlock();
        return true;
    }
    /* The UI marks itself busy before posting, so route/path cannot change until
     * this immutable operation completes. Never block with the UI lock held. */
    uint8_t *out = heap_caps_malloc(CYD_PAIR_RESPONSE_MAX + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    size_t n = 0;
    esp_err_t err = out ? ESP_OK : ESP_ERR_NO_MEM;
    cJSON *o = cJSON_CreateObject();
    const char *rel = action == 710 ? path : selected;
    char new_path[181];
    if (action == 712 || action == 714) {
        size_t k = strlen(path), f = strlen(filename);
        if (k + f + (k ? 1 : 0) >= sizeof(new_path)) {
            cyd_ui_lock();
            busy = false;
            lv_label_set_text(notice, "Path too long");
            cyd_ui_unlock();
            free(out);
            cJSON_Delete(o);
            return true;
        }
        memcpy(new_path, path, k);
        if (k)
            new_path[k++] = '/';
        memcpy(new_path + k, filename, f + 1);
        if (action == 712)
            rel = new_path;
    }
    cJSON_AddStringToObject(o, "path", rel);
    if (action == 711) {
        cJSON_AddNumberToObject(o, "offset", read_offset);
        cJSON_AddNumberToObject(o, "length", 2048);
    }
    if (action == 712 || action == 716) {
        if (action == 716)
            cJSON_AddBoolToObject(o, "replace", true);
        cJSON_AddNumberToObject(o, "offset", 0);
        cJSON_AddStringToObject(o, "text", draft);
    }
    if (action == 714)
        cJSON_AddStringToObject(o, "target", new_path);
    unsigned op = 0;
    switch (action) {
    case 710:
        op = CYD_PAIR_FILE_LIST;
        break;
    case 711:
        op = CYD_PAIR_FILE_READ;
        break;
    case 712:
    case 716:
        op = CYD_PAIR_FILE_WRITE;
        break;
    case 713:
        op = CYD_PAIR_FILE_REMOVE;
        break;
    case 714:
        op = CYD_PAIR_FILE_RENAME;
        break;
    default:
        err = ESP_ERR_NOT_SUPPORTED;
        break;
    }

    if (!err)
        err = request(op, o, out, CYD_PAIR_RESPONSE_MAX, &n);
    cJSON_Delete(o);
    cJSON *listing = NULL;
    if (!err && action == 710)
        listing = cyd_json_parse(out, n, CYD_PAIR_RESPONSE_MAX);
    cyd_ui_lock();
    busy = false;
    base();
    lv_label_set_text(title, drive == 0 ? "System (S3)" : "Device (C5)");
    if (err) {
        lv_label_set_text(notice, "Unavailable. Check link, SD, path or write permissions.");
        button(body, "Drives", 706);
    } else if (action == 710) {
        lv_label_set_text(notice,
                          path[0] ? path : "Choose a folder. Apps/data are read-only here.");
        if (path[0])
            button(body, "Up", 703);
        else {
            button(body, "System drive", 700);
            button(body, "Device drive", 701);
        }
        cJSON *v;
        cJSON_ArrayForEach(v, listing) {
            const char *name = cyd_pair_string(v, "name");
            bool dir = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(v, "directory"));
            lv_obj_t *b = lv_button_create(body);
            lv_obj_set_size(b, LV_PCT(100), 42);
            char caption[128];
            snprintf(caption, sizeof(caption), "%s%s", dir ? LV_SYMBOL_DIRECTORY " " : "", name);
            label(b, caption);
            file_entry_t *item = lv_malloc(sizeof(*item));
            if (!item)
                continue;
            item->directory = dir;
            snprintf(item->name, sizeof(item->name), "%.96s", name);
            lv_obj_add_event_cb(b, entry, LV_EVENT_CLICKED, item);
            lv_obj_add_event_cb(b, entry_free, LV_EVENT_DELETE, item);
        }
        if (!strncmp(path, "documents", 9) || !strncmp(path, "reports", 7))
            button(body, "New text file", 704);
    } else if (action == 711) {
        lv_label_set_text(notice, selected);
        size_t consumed = n;
        bool text = text_chunk(out, n, n == 2048, &consumed);
        out[text ? consumed : n] = 0;
        if (text) {
            lv_obj_t *reader = label(body, (char *)out);
            lv_obj_add_flag(reader, LV_OBJ_FLAG_USER_1);
        } else {
            char hex[769];
            size_t used = 0;
            for (size_t i = 0; i < n && i < 256; i++)
                used += (size_t)snprintf(hex + used, sizeof(hex) - used, "%02x ", out[i]);
            lv_obj_t *reader = label(body, hex);
            lv_obj_add_flag(reader, LV_OBJ_FLAG_USER_1);
        }
        next_offset = read_offset + (text ? consumed : n);
        if (read_offset)
            button(body, "Start of file", 718);
        if (n == 2048)
            button(body, "Next 2 KiB", 715);
        if (!strncmp(selected, "documents/", 10) || !strncmp(selected, "reports/", 8)) {
            if (text && n <= 240 && !read_offset) {
                memcpy(draft, out, n);
                draft[n] = 0;
                button(body, "Edit text", 707);
            }
            button(body, "Rename", 708);
            button(body, "Delete...", 705);
        }
        button(body, "Copy to other drive", 717);
        button(body, "Back to folder", 710);
    } else {
        lv_label_set_text(notice, action == 712 ? "Saved" : "Deleted");
        button(body, "Back to folder", 710);
    }
    cyd_pager_attach(body);
    cyd_ui_unlock();
    cJSON_Delete(listing);
    free(out);
    return true;
}
