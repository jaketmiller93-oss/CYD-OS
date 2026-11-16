#include "pair_internal.h"
#include "cyd_pair_manager.h"
#include "cyd_peer.h"
#include "cyd_device.h"
#include "cyd_ota.h"
#include "nvs.h"
#include "freertos/task.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#if CYD_PAIR_MANAGER
static nvs_handle_t store;
static SemaphoreHandle_t guard;
static char host_profile[512];
esp_err_t cyd_pair_manager_remember(const cJSON *settings) {
    if (!guard || cyd_maintenance_active())
        return ESP_ERR_INVALID_STATE;
    const cJSON *mode = cJSON_GetObjectItemCaseSensitive(settings, "display_mode");
    if (cJSON_IsNumber(mode) && mode->valueint != 0)
        return ESP_ERR_INVALID_STATE;
    int64_t brightness, rotation, seconds, percent;
    if (!settings || !cyd_pair_number(settings, "brightness", 10, 100, &brightness) ||
        !cyd_pair_number(settings, "rotation", 0, 3, &rotation) ||
        !cyd_pair_number(settings, "dim_seconds", 0, 300, &seconds) ||
        !cyd_pair_number(settings, "dim_percent", 10, 50, &percent) ||
        (seconds != 0 && seconds != 30 && seconds != 60 && seconds != 120 && seconds != 300) ||
        (percent != 10 && percent != 25 && percent != 50))
        return ESP_ERR_INVALID_ARG;
    char *json = cJSON_PrintUnformatted(settings);
    if (!json)
        return ESP_ERR_NO_MEM;
    if (strlen(json) >= sizeof(host_profile)) {
        free(json);
        return ESP_ERR_INVALID_SIZE;
    }
    xSemaphoreTake(guard, portMAX_DELAY);
    esp_err_t e = ESP_OK;
    if (strcmp(json, host_profile)) {
        e = nvs_set_str(store, "host_display", json);
        if (!e)
            e = nvs_commit(store);
        if (!e)
            strcpy(host_profile, json);
    }
    xSemaphoreGive(guard);
    free(json);
    return e;
}
static void report(const char *name, cJSON *o) {
    if (!o || !cyd_storage_ready()) {
        cJSON_Delete(o);
        return;
    }
    char *text = cJSON_PrintUnformatted(o);
    cJSON_Delete(o);
    if (!text)
        return;
    cyd_mkdirs(CYD_ROOT "/reports");
    char path[256], stage[256];
    snprintf(path, sizeof(path), CYD_ROOT "/reports/%s", name);
    snprintf(stage, sizeof(stage), CYD_ROOT "/reports/.%s.new", name);
    if (!cyd_write_file(stage, text, strlen(text))) {
        unlink(path);
        rename(stage, path);
    }
    free(text);
}
static void collect(void *arg) {
    (void)arg;
    bool profile_applied = false;
    uint32_t resets = 0;
    for (;;) {
        /* Boot remains local. Defer remote profile/NVS/report work until the
         * same acceptance point used by a board connected after startup. */
        if (!cyd_ota_boot_confirmed()) {
            vTaskDelay(pdMS_TO_TICKS(250));
            continue;
        }
        cyd_peer_info_t peer;
        cyd_peer_snapshot(&peer);
        if (!peer.connected || peer.resets != resets) {
            profile_applied = false;
            resets = peer.resets;
        }
        if (peer.connected) {
            cJSON *settings = cyd_pair_json(CYD_PAIR_SETTINGS_GET, NULL);
            if (settings && !profile_applied) {
                char profile[512];
                xSemaphoreTake(guard, portMAX_DELAY);
                strcpy(profile, host_profile);
                xSemaphoreGive(guard);
                if (!profile[0]) {
                    profile_applied = cyd_pair_manager_remember(settings) == ESP_OK;
                } else {
                    cJSON *desired = cyd_json_parse(profile, strlen(profile), sizeof(profile));
                    cJSON *result = cyd_pair_json(CYD_PAIR_SETTINGS_APPLY, desired);
                    profile_applied = result != NULL;
                    cJSON_Delete(result);
                    cJSON_Delete(desired);
                }
            }
            cJSON_Delete(settings);
            report("s3-logs.json", cyd_pair_json(CYD_PAIR_LOGS, NULL));
            cJSON *host = cyd_pair_json(CYD_PAIR_STATUS, NULL);
            cyd_device_status_t local;
            cyd_device_snapshot(&local);
            if (!local.clock_valid &&
                cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(host, "clock_valid"))) {
                int64_t epoch;
                if (cyd_pair_number(host, "epoch", 1704067200, 4102444799LL, &epoch))
                    cyd_device_set_clock(epoch, local.utc_offset_minutes);
            }
            report("s3-diagnostics.json", host);
        }
        report("c5-logs.json", cyd_logs());
        report("c5-diagnostics.json", cyd_pair_status());
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}
esp_err_t cyd_pair_manager_start(void) {
    guard = xSemaphoreCreateMutex();
    if (!guard) {
        cyd_log("pair", "Startup failed at manager settings mutex");
        return ESP_ERR_NO_MEM;
    }
    esp_err_t e = nvs_open("cyd_pairprefs", NVS_READWRITE, &store);
    if (e) {
        char detail[128];
        snprintf(detail, sizeof(detail), "Startup failed opening manager settings NVS (0x%x)",
                 (unsigned)e);
        cyd_log("pair", detail);
        vSemaphoreDelete(guard);
        guard = NULL;
        return e;
    }
    size_t n = sizeof(host_profile);
    if (nvs_get_str(store, "host_display", host_profile, &n) != ESP_OK)
        host_profile[0] = 0;
    if (xTaskCreate(collect, "device_reports", 4096, NULL, 1, NULL) != pdPASS) {
        cyd_log("pair", "Startup failed at diagnostic reports task (4096-byte internal stack)");
        nvs_close(store);
        vSemaphoreDelete(guard);
        guard = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
#else
esp_err_t cyd_pair_manager_remember(const cJSON *s) {
    (void)s;
    return ESP_ERR_NOT_SUPPORTED;
}
esp_err_t cyd_pair_manager_start(void) {
    return ESP_OK;
}
#endif
