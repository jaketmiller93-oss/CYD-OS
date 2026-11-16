#include "pair_internal.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
const char *cyd_pair_string(const cJSON *o, const char *key) {
    cJSON *v = cJSON_GetObjectItemCaseSensitive(o, key);
    return cJSON_IsString(v) ? v->valuestring : "";
}
bool cyd_pair_number(const cJSON *o, const char *key, int64_t lo, int64_t hi, int64_t *value) {
    cJSON *v = cJSON_GetObjectItemCaseSensitive(o, key);
    if (!cJSON_IsNumber(v) || !isfinite(v->valuedouble) || v->valuedouble < lo ||
        v->valuedouble > hi || floor(v->valuedouble) != v->valuedouble)
        return false;
    *value = (int64_t)v->valuedouble;
    return true;
}
esp_err_t cyd_pair_encode(cJSON *o, void *out, size_t cap, size_t *size) {
    if (!o)
        return ESP_ERR_NO_MEM;
    char *s = cJSON_PrintUnformatted(o);
    cJSON_Delete(o);
    if (!s)
        return ESP_ERR_NO_MEM;
    size_t n = strlen(s);
    esp_err_t e = n <= cap ? ESP_OK : ESP_ERR_INVALID_SIZE;
    if (!e) {
        memcpy(out, s, n);
        *size = n;
    }
    free(s);
    return e;
}

#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
static void *json_memory(size_t n) {
    return heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}
#endif
esp_err_t cyd_pair_memory_init(void) {
#ifdef ESP_PLATFORM
    cJSON_Hooks hooks = {.malloc_fn = json_memory, .free_fn = free};
    cJSON_InitHooks(&hooks);
#endif
    return ESP_OK;
}

static bool replace(cJSON *o, const char *key, cJSON *v) {
    if (!v)
        return false;
    bool ok = cJSON_HasObjectItem(o, key) ? cJSON_ReplaceItemInObjectCaseSensitive(o, key, v)
                                          : cJSON_AddItemToObject(o, key, v);
    if (!ok)
        cJSON_Delete(v);
    return ok;
}
cJSON *cyd_pair_dashboard_merge(cJSON *local, cJSON *host, cJSON *manager) {
    if (!cJSON_IsObject(local) || !cJSON_IsObject(manager)) {
        cJSON_Delete(local);
        cJSON_Delete(host);
        cJSON_Delete(manager);
        return NULL;
    }
    if (!cJSON_IsObject(host)) {
        cJSON_Delete(host);
        host = NULL;
    }
    const char *version = cyd_pair_string(local, "firmware");
    bool healthy = host && !strcmp(version, cyd_pair_string(host, "firmware")) &&
                   cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(host, "boot_confirmed")) &&
                   cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(manager, "boot_confirmed"));
    cJSON *app = host ? cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(host, "app"), true)
                      : cJSON_CreateObject();
    if (!host && app) {
        cJSON_AddStringToObject(app, "state", "unavailable");
        cJSON_AddStringToObject(app, "error", "S3 application host unavailable");
    }
    bool ok = replace(local, "app", app) &&
              replace(local, "model", cJSON_CreateString("CYD-OS paired tablet")) &&
              replace(local, "target", cJSON_CreateString("esp32s3")) &&
              replace(local, "sd",
                      cJSON_CreateBool(host &&
                                       cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(host, "sd"))));
    const char *metrics[] = {"heap_free",   "largest_block",      "psram_total",
                             "psram_free",  "internal_heap_free", "internal_largest_block",
                             "storage_free"};
    for (unsigned i = 0; i < sizeof(metrics) / sizeof(metrics[0]) && ok; i++) {
        cJSON *value = cJSON_GetObjectItemCaseSensitive(host, metrics[i]);
        ok = replace(local, metrics[i],
                     cJSON_CreateNumber(cJSON_IsNumber(value) ? value->valuedouble : 0));
    }
    if (ok && host)
        ok = replace(local, "firmware", cJSON_CreateString(cyd_pair_string(host, "firmware")));
    if (ok)
        ok = replace(local, "paired_system", cJSON_CreateBool(true)) &&
             replace(local, "system_ready", cJSON_CreateBool(healthy)) &&
             replace(local, "resource_scope", cJSON_CreateString("application_host"));
    if (!ok) {
        cJSON_Delete(local);
        cJSON_Delete(host);
        cJSON_Delete(manager);
        return NULL;
    }
    if (!host)
        host = cJSON_CreateNull();
    if (!host || !cJSON_AddItemToObject(local, "host", host)) {
        cJSON_Delete(host);
        cJSON_Delete(manager);
        cJSON_Delete(local);
        return NULL;
    }
    if (!cJSON_AddItemToObject(local, "manager", manager)) {
        cJSON_Delete(manager);
        cJSON_Delete(local);
        return NULL;
    }
    return local;
}

cJSON *cyd_pair_logs_merge(cJSON *manager, cJSON *host, unsigned per_board) {
    if (!per_board || per_board > 64) {
        cJSON_Delete(manager);
        cJSON_Delete(host);
        return NULL;
    }
    cJSON *out = cJSON_CreateArray();
    if (!out) {
        cJSON_Delete(manager);
        cJSON_Delete(host);
        return NULL;
    }
    cJSON *sources[] = {manager, host};
    const char *names[] = {"C5", "S3"};
    for (unsigned side = 0; side < 2; side++) {
        int total = cJSON_GetArraySize(sources[side]), index = 0;
        cJSON *v;
        cJSON_ArrayForEach(v, sources[side]) {
            if (index++ < total - (int)per_board || !cJSON_IsString(v))
                continue;
            char line[352];
            snprintf(line, sizeof(line), "[%s] %.330s", names[side], v->valuestring);
            cJSON *entry = cJSON_CreateString(line);
            if (!entry || !cJSON_AddItemToArray(out, entry)) {
                cJSON_Delete(entry);
                cJSON_Delete(out);
                out = NULL;
                break;
            }
        }
        if (!out)
            break;
    }
    if (out && !host)
        cJSON_AddItemToArray(
            out, cJSON_CreateString("[S3] Logs unavailable; check the application host and link"));
    cJSON_Delete(manager);
    cJSON_Delete(host);
    return out;
}
