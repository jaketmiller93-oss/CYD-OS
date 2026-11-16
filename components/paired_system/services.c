#include "pair_internal.h"
#include "cyd_peer.h"
#include "cyd_device.h"
#include "cyd_ota.h"
#include "cyd_projects.h"
#include "cyd_network.h"
#include "cyd_credentials.h"
#include "esp_app_desc.h"
#include "esp_psram.h"
#include "cyd_pair_ble.h"
#if CYD_PAIR_HOST
extern void cyd_settings_open(void);
#endif
#include "esp_heap_caps.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
void cyd_pair_elf_sha256(char out[65]) {
    const uint8_t *hash = esp_app_get_description()->app_elf_sha256;
    for (unsigned i = 0; i < 32; i++)
        snprintf(out + i * 2, 3, "%02x", hash[i]);
}
cJSON *cyd_pair_status(void) {
    cJSON *o = cJSON_CreateObject();
    if (!o)
        return NULL;
    cJSON_AddStringToObject(o, "system_version", cyd_system_version());
    cJSON_AddNumberToObject(o, "service_schema", 1);
    char elf[65], boot[17];
    cyd_pair_elf_sha256(elf);
    cyd_peer_info_t peer;
    cyd_peer_snapshot(&peer);
    snprintf(boot, sizeof(boot), "%016llx", (unsigned long long)peer.local_boot);
    cJSON_AddStringToObject(o, "elf_sha256", elf);
    cJSON_AddStringToObject(o, "boot_id", boot);
    cJSON_AddStringToObject(o, "role", CYD_PAIR_HOST ? "application_host" : "device_manager");
    cJSON_AddStringToObject(o, "drive", CYD_PAIR_HOST ? "system" : "device");
    cJSON_AddBoolToObject(o, "sd", cyd_storage_ready());
    cJSON_AddNumberToObject(o, "storage_free", cyd_storage_free());
    cJSON_AddNumberToObject(o, "heap_free", heap_caps_get_free_size(MALLOC_CAP_8BIT));
    cJSON_AddNumberToObject(o, "largest_block", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    cJSON_AddNumberToObject(o, "psram_total", esp_psram_get_size());
    cJSON_AddNumberToObject(o, "internal_heap_free",
                            heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    cJSON_AddNumberToObject(
        o, "internal_largest_block",
        heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    cJSON_AddNumberToObject(o, "psram_free", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    cJSON_AddStringToObject(o, "model", cyd_board_model());
    cJSON_AddItemToObject(o, "app", cyd_runtime_status());
#if CYD_PAIR_MANAGER
    cyd_net_status_t net;
    cyd_network_snapshot(&net);
    cJSON *ns = cJSON_AddObjectToObject(o, "network");
    cJSON_AddStringToObject(ns, "station_ip", net.station_ip);
    cJSON_AddStringToObject(ns, "ap_ip", net.ap_ip);
    cJSON_AddStringToObject(ns, "station_ssid", net.ssid);
    cJSON_AddStringToObject(ns, "ap_ssid", net.ap_ssid);
    cJSON_AddItemToObject(o, "bluetooth", cyd_pair_ble_status());
    cJSON_AddBoolToObject(ns, "connected", net.state == CYD_NET_CONNECTED);
    cJSON_AddBoolToObject(ns, "ap_enabled", net.ap_enabled);
#endif
    cyd_device_status_t d;
    cyd_device_snapshot(&d);
    cJSON_AddBoolToObject(o, "clock_valid", d.clock_valid);
    cJSON_AddNumberToObject(o, "epoch", (double)time(NULL));
    cJSON_AddNumberToObject(o, "offset", d.utc_offset_minutes);
    cJSON_AddBoolToObject(o, "clock_12h", d.clock_12h);
    cyd_ota_status_t u;
    cyd_ota_snapshot(&u);
    cJSON_AddBoolToObject(o, "boot_confirmed", u.boot_confirmed);
    cJSON_AddStringToObject(o, "firmware", cyd_system_version());
    return o;
}
static uint8_t *app_upload;
static size_t app_total, app_received;
static void app_clear(void) {
    free(app_upload);
    app_upload = NULL;
    app_total = app_received = 0;
}
esp_err_t cyd_pair_dispatch(unsigned op, const void *bytes, size_t n, void *out, size_t cap,
                            size_t *size) {
    *size = 0;
    if (op >= CYD_PAIR_FILE_LIST && op <= CYD_PAIR_FILE_COMMIT)
        return cyd_pair_files(op, bytes, n, out, cap, size);
    if (op >= CYD_PAIR_UPDATE_BEGIN && op <= CYD_PAIR_BUNDLE_CANCEL)
        return cyd_pair_update(op, bytes, n, out, cap, size);
    if (op >= CYD_PAIR_NET_OPEN && op <= CYD_PAIR_NET_CLOSE) {
#if CYD_PAIR_MANAGER
        return cyd_pair_network(op, bytes, n, out, cap, size);
#else
        return ESP_ERR_NOT_SUPPORTED;
#endif
    }
#if CYD_PAIR_MANAGER
    if (op >= CYD_PAIR_CREDENTIAL_GET && op <= CYD_PAIR_CREDENTIAL_DELETE) {
        const uint8_t *p = bytes;
        if (n < 98 || n > 354 || !memchr(p, 0, 65) || !memchr(p + 65, 0, 33))
            return ESP_ERR_INVALID_ARG;
        const char *app = (const char *)p, *key = (const char *)p + 65;
        if (op == CYD_PAIR_CREDENTIAL_SET)
            return cyd_credentials_set(app, key, p + 98, n - 98);
        if (n != 98)
            return ESP_ERR_INVALID_ARG;
        if (op == CYD_PAIR_CREDENTIAL_DELETE)
            return cyd_credentials_delete(app, key);
        *size = cap > 256 ? 256 : cap;
        return cyd_credentials_get(app, key, out, size);
    }
#endif
    cJSON *a = NULL;
    if (op == CYD_PAIR_STATUS)
        return cyd_pair_encode(cyd_pair_status(), out, cap, size);
    if (op == CYD_PAIR_LOGS)
        return cyd_pair_encode(cyd_logs(), out, cap, size);
#if CYD_PAIR_HOST
    if (op == CYD_PAIR_VERSIONS)
        return cyd_pair_encode(cyd_versions(), out, cap, size);
    if (op == CYD_PAIR_CATALOG)
        return cyd_pair_encode(cyd_projects_catalog(), out, cap, size);
    if (op == CYD_PAIR_APP_WRITE) {
        if (!app_upload || n < 4)
            return ESP_ERR_INVALID_STATE;
        const uint8_t *p = bytes;
        size_t off = (size_t)p[0] | (size_t)p[1] << 8 | (size_t)p[2] << 16 | (size_t)p[3] << 24;
        if (off != app_received || n - 4 > app_total - app_received)
            return ESP_ERR_INVALID_ARG;
        memcpy(app_upload + app_received, p + 4, n - 4);
        app_received += n - 4;
        return ESP_OK;
    }
    if (op == CYD_PAIR_APP_FINISH) {
        if (!app_upload || app_received != app_total)
            return ESP_ERR_INVALID_STATE;
        char error[160] = "", rev[17];
        bool inspect = n == 1 && *(const uint8_t *)bytes == 1;
        esp_err_t e = ESP_OK;
        if (inspect) {
            a = cyd_package_inspect(app_upload, app_total, error, sizeof(error));
            if (!a)
                e = ESP_ERR_INVALID_ARG;
        } else {
            e = cyd_install(app_upload, app_total, rev, sizeof(rev), error, sizeof(error));
            if (!e) {
                a = cJSON_CreateObject();
                cJSON_AddStringToObject(a, "revision", rev);
                cJSON_AddStringToObject(a, "status", "installed");
            }
        }
        app_clear();
        if (e)
            return e;
        return cyd_pair_encode(a, out, cap, size);
    }
#endif
    a = n ? cyd_json_parse(bytes, n, CYD_PAIR_REQUEST_MAX) : cJSON_CreateObject();
    if (!cJSON_IsObject(a)) {
        cJSON_Delete(a);
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t e = ESP_ERR_NOT_SUPPORTED;
#if CYD_PAIR_HOST
    if (op == CYD_PAIR_RUN)
        e = cyd_run_revision(cyd_pair_string(a, "revision"));
    else if (op == CYD_PAIR_STOP)
        e = cyd_runtime_stop();
    else if (op == CYD_PAIR_DELETE)
        e = cyd_delete_revision(cyd_pair_string(a, "revision"));
    else if (op == CYD_PAIR_APP_BEGIN) {
        int64_t length;
        if (cyd_pair_number(a, "total", 22, CYD_MAX_PACKAGE, &length) &&
            !cyd_maintenance_active()) {
            app_clear();
            app_upload = heap_caps_malloc((size_t)length, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            app_total = (size_t)length;
            e = app_upload ? ESP_OK : ESP_ERR_NO_MEM;
        } else
            e = ESP_ERR_INVALID_ARG;
    } else if (op == CYD_PAIR_APP_READ) {
        const char *rev = cyd_pair_string(a, "revision");
        int64_t offset;
        bool valid = strlen(rev) == 16;
        for (unsigned i = 0; i < 16 && valid; i++)
            if (!((rev[i] >= '0' && rev[i] <= '9') || (rev[i] >= 'a' && rev[i] <= 'f')))
                valid = false;
        if (!valid || !cyd_pair_number(a, "offset", 0, CYD_MAX_PACKAGE, &offset)) {
            cJSON_Delete(a);
            return ESP_ERR_INVALID_ARG;
        }
        char path[256];
        snprintf(path, sizeof(path), CYD_ROOT "/projects/%s/package.cydapp", rev);
        FILE *f = fopen(path, "rb");
        if (!f) {
            cJSON_Delete(a);
            return ESP_ERR_NOT_FOUND;
        }
        e = fseek(f, (long)offset, SEEK_SET) ? ESP_FAIL : ESP_OK;
        if (!e) {
            size_t take = cap > 2048 ? 2048 : cap;
            *size = fread(out, 1, take, f);
            if (ferror(f))
                e = ESP_FAIL;
        }
        fclose(f);
        cJSON_Delete(a);
        return e;
    } else if (op == CYD_PAIR_CATALOG_EDIT) {
        const char *action = cyd_pair_string(a, "action"), *folder = cyd_pair_string(a, "folder"),
                   *name = cyd_pair_string(a, "name");
        char id[9];
        e = ESP_ERR_INVALID_ARG;
        if (!strcmp(action, "create"))
            e = cyd_project_folder_create(name, id);
        else if (!strcmp(action, "rename"))
            e = cyd_project_folder_rename(folder, name);
        else if (!strcmp(action, "delete"))
            e = cyd_project_folder_delete(folder);
        else if (!strcmp(action, "move"))
            e = cyd_project_move(cyd_pair_string(a, "package_id"), folder);
        if (!e) {
            cJSON_Delete(a);
            return cyd_pair_encode(cyd_projects_catalog(), out, cap, size);
        }
    }
#endif
    if (op == CYD_PAIR_SETTINGS_GET) {
        cyd_device_status_t d;
        cyd_device_snapshot(&d);
        cJSON *s = cJSON_CreateObject();
        cJSON_AddNumberToObject(s, "display_mode", d.mode);
        cJSON_AddNumberToObject(s, "seconds_left", d.seconds_left);
        cJSON_AddNumberToObject(s, "brightness", d.brightness);
        cJSON_AddNumberToObject(s, "rotation", d.rotation);
        cJSON_AddNumberToObject(s, "dim_seconds", d.dim_seconds);
        cJSON_AddNumberToObject(s, "dim_percent", d.dim_percent);
        cJSON_AddBoolToObject(s, "clock_12h", d.clock_12h);
        cJSON_AddNumberToObject(s, "offset", d.utc_offset_minutes);
        cJSON_Delete(a);
        return cyd_pair_encode(s, out, cap, size);
    }
    if (op == CYD_PAIR_SETTINGS_APPLY) {
        int64_t brightness, rotation, seconds, percent;
        if (!cyd_pair_number(a, "brightness", 10, 100, &brightness) ||
            !cyd_pair_number(a, "rotation", 0, 3, &rotation) ||
            !cyd_pair_number(a, "dim_seconds", 0, 300, &seconds) ||
            !cyd_pair_number(a, "dim_percent", 10, 50, &percent)) {
            cJSON_Delete(a);
            return ESP_ERR_INVALID_ARG;
        }
        if ((seconds != 0 && seconds != 30 && seconds != 60 && seconds != 120 && seconds != 300) ||
            (percent != 10 && percent != 25 && percent != 50)) {
            cJSON_Delete(a);
            return ESP_ERR_INVALID_ARG;
        }
        cyd_device_status_t d;
        cyd_device_snapshot(&d);
        e = cyd_device_brightness((unsigned)brightness, true);
        if (!e)
            e = cyd_device_preferences((unsigned)seconds, (unsigned)percent, d.clock_12h);
        if (!e && d.rotation != (unsigned)rotation) {
            e = cyd_device_rotation((unsigned)rotation);
            if (!e)
                e = cyd_device_confirm();
        }
    }
    if (op == CYD_PAIR_SETTINGS_SET) {
        const char *action = cyd_pair_string(a, "action");
        int64_t v, offset;
        e = ESP_ERR_INVALID_ARG;
        if (!strcmp(action, "brightness") && cyd_pair_number(a, "value", 10, 100, &v))
            e = cyd_device_brightness((unsigned)v, true);
        else if (!strcmp(action, "rotation") && cyd_pair_number(a, "value", 0, 3, &v))
            e = cyd_device_rotation((unsigned)v);
        else if (!strcmp(action, "confirm"))
            e = cyd_device_confirm();
        else if (!strcmp(action, "revert"))
            e = cyd_device_revert();
        else if (!strcmp(action, "reset_display"))
            e = cyd_device_reset_display();
        else if (!strcmp(action, "calibrate")) {
#if CYD_PAIR_HOST
            cyd_ui_lock();
            cyd_settings_open();
            cyd_ui_unlock();
#endif
            e = cyd_device_calibrate();
        } else if (!strcmp(action, "preferences")) {
            int64_t seconds, percent;
            if (cyd_pair_number(a, "seconds", 0, 300, &seconds) &&
                cyd_pair_number(a, "percent", 10, 50, &percent)) {
                cyd_device_status_t d;
                cyd_device_snapshot(&d);
                e = cyd_device_preferences((unsigned)seconds, (unsigned)percent, d.clock_12h);
            }
        }
#if CYD_PAIR_MANAGER
        else if (!strcmp(action, "clock") &&
                 cyd_pair_number(a, "epoch", 1704067200, 4102444799LL, &v) &&
                 cyd_pair_number(a, "offset", -720, 840, &offset))
            e = cyd_device_set_clock(v, (int)offset);
#else
        (void)offset;
#endif
    }
    cJSON_Delete(a);
    if (!e) {
        const char ok[] = "{\"ok\":true}";
        if (cap < sizeof(ok) - 1)
            return ESP_ERR_INVALID_SIZE;
        memcpy(out, ok, sizeof(ok) - 1);
        *size = sizeof(ok) - 1;
    }
    return e;
}
