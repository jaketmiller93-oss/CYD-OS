#include "cyd.h"
#include "cyd_projects.h"
#include "esp_random.h"
#include "sdkconfig.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
static SemaphoreHandle_t mutex;
static const char *str(cJSON *o, const char *k) {
    cJSON *v = cJSON_GetObjectItemCaseSensitive(o, k);
    return cJSON_IsString(v) ? v->valuestring : NULL;
}
esp_err_t cyd_manager_init(void) {
    mutex = xSemaphoreCreateMutex();
    return mutex ? cyd_projects_init() : ESP_ERR_NO_MEM;
}
static bool revision_ok(const char *r) {
    if (!r || strlen(r) != 16)
        return false;
    for (int i = 0; i < 16; i++)
        if (!((r[i] >= '0' && r[i] <= '9') || (r[i] >= 'a' && r[i] <= 'f')))
            return false;
    return true;
}
esp_err_t cyd_install(const uint8_t *bytes, size_t n, char *revision, size_t cap, char *error,
                      size_t ec) {
    if (!cyd_storage_ready() || cyd_maintenance_active())
        return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(mutex, portMAX_DELAY);
    esp_err_t e = ESP_FAIL;
    cyd_zip_t *zip = calloc(1, sizeof(*zip));
    char final[180] = {0};
    const char *why = "Invalid stored-ZIP package";
    if (!zip) {
        e = ESP_ERR_NO_MEM;
        goto done;
    }
    cyd_manifest_t m;
    e = cyd_package_validate(bytes, n, zip, &m, error, ec);
    if (e) {
        why = error;
        goto done;
    }
    if (cyd_storage_free() < 2 * n + 65536) {
        e = ESP_ERR_NO_MEM;
        why = "Insufficient SD storage";
        goto done;
    }
    snprintf(revision, cap, "%08lx%08lx", (unsigned long)esp_random(), (unsigned long)esp_random());
    snprintf(final, sizeof(final), CYD_ROOT "/projects/%s", revision);
    struct stat st;
    if (!stat(final, &st)) {
        e = ESP_FAIL;
        why = "Revision collision; retry";
        goto done;
    }
    const char *stage = CYD_ROOT "/staging";
    cyd_remove_tree(stage);
    e = cyd_mkdirs(stage);
    if (e)
        goto io;
    for (unsigned i = 0; i < zip->count; i++) {
        char path[320], parent[320];
        snprintf(path, sizeof(path), "%s/%s", stage, zip->files[i].name);
        strcpy(parent, path);
        *strrchr(parent, '/') = 0;
        if ((e = cyd_mkdirs(parent)) ||
            (e = cyd_write_file(path, zip->files[i].data, zip->files[i].size)))
            goto io;
    }
    /* Preserve the exact upload for downloads; reserved filename can't shadow app
     * data. */
    {
        char path[200];
        snprintf(path, sizeof(path), "%s/package.cydapp", stage);
        for (unsigned i = 0; i < zip->count; i++)
            if (!strcmp(zip->files[i].name, "package.cydapp")) {
                e = ESP_ERR_INVALID_ARG;
                why = "Reserved package filename";
                goto done;
            }
        e = cyd_write_file(path, bytes, n);
        if (e)
            goto io;
    }
    if (rename(stage, final)) {
        e = ESP_FAIL;
        goto io;
    }
    cyd_log(m.id, "Installed revision");
    e = ESP_OK;
    goto done;
io:
    why = "SD write/commit failed; previous revisions retained";
done:
    if (e) {
        cyd_remove_tree(CYD_ROOT "/staging");
        if (why != error)
            snprintf(error, ec, "%s", why);
    }
    free(zip);
    xSemaphoreGive(mutex);
    return e;
}
cJSON *cyd_versions(void) {
    cJSON *a = cJSON_CreateArray();
    if (!cyd_storage_ready())
        return a;
    xSemaphoreTake(mutex, portMAX_DELAY);
    DIR *d = opendir(CYD_ROOT "/projects");
    struct dirent *ent;
    if (d)
        while ((ent = readdir(d)))
            if (revision_ok(ent->d_name)) {
                char p[200];
                snprintf(p, sizeof(p), CYD_ROOT "/projects/%.16s/manifest.json", ent->d_name);
                uint8_t *b;
                size_t n;
                cyd_manifest_t m;
                if (!cyd_read_file(p, &b, &n, 4096)) {
                    if (!cyd_manifest_parse(b, n, &m)) {
                        cJSON *o = cJSON_CreateObject();
                        cJSON_AddStringToObject(o, "revision", ent->d_name);
                        cJSON_AddStringToObject(o, "package_id", m.id);
                        cJSON_AddStringToObject(o, "name", m.name);
                        cJSON_AddStringToObject(o, "version", m.version);
                        cJSON_AddItemToArray(a, o);
                    }
                    free(b);
                }
            }
    if (d)
        closedir(d);
    xSemaphoreGive(mutex);
    return a;
}
esp_err_t cyd_run_revision(const char *r) {
    if (cyd_maintenance_active())
        return ESP_ERR_INVALID_STATE;
    if (!revision_ok(r))
        return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(mutex, portMAX_DELAY);
    char path[256];
    snprintf(path, sizeof(path), CYD_ROOT "/projects/%s/manifest.json", r);
    uint8_t *b = NULL;
    size_t n;
    cyd_manifest_t m;
    esp_err_t e = cyd_read_file(path, &b, &n, 4096);
    if (!e)
        e = cyd_manifest_parse(b, n, &m);
    free(b);
    if (!e) {
        snprintf(path, sizeof(path), CYD_ROOT "/projects/%s/%s", r, m.entry);
        e = cyd_runtime_start(path, r, &m);
    }
    xSemaphoreGive(mutex);
    return e;
}
esp_err_t cyd_delete_revision(const char *r) {
    if (cyd_maintenance_active())
        return ESP_ERR_INVALID_STATE;
    if (!revision_ok(r))
        return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(mutex, portMAX_DELAY);
    cJSON *status = cyd_runtime_status();
    const char *active = str(status, "revision"), *state = str(status, "state");
    bool busy = active && !strcmp(active, r) && state && strcmp(state, "stopped") &&
                strcmp(state, "crashed");
    cJSON_Delete(status);
    if (busy) {
        xSemaphoreGive(mutex);
        return ESP_ERR_INVALID_STATE;
    }
    char p[180];
    snprintf(p, sizeof(p), CYD_ROOT "/projects/%s", r);
    struct stat s;
    esp_err_t e = stat(p, &s) ? ESP_ERR_NOT_FOUND : ESP_OK;
    if (!e) {
        cyd_remove_tree(p);
        if (!stat(p, &s))
            e = ESP_FAIL;
    }
    xSemaphoreGive(mutex);
    return e;
}
