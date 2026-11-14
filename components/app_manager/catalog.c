#include "cyd.h"
#include "cyd_projects.h"
#include "esp_random.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>
typedef struct {
    char id[9], name[33];
} folder_t;
typedef struct {
    char package_id[65], folder[9];
} placement_t;
typedef struct {
    uint32_t generation;
    unsigned folders, placements;
    folder_t folder[CYD_PROJECT_MAX_FOLDERS];
    placement_t placement[CYD_PROJECT_MAX_PLACEMENTS];
} catalog_t;
static catalog_t catalog;
static SemaphoreHandle_t guard;
static bool loaded;
static int active_slot;
static const char *str(cJSON *o, const char *key) {
    cJSON *v = cJSON_GetObjectItemCaseSensitive(o, key);
    return cJSON_IsString(v) ? v->valuestring : NULL;
}
static bool uint32_value(cJSON *v, uint32_t *out) {
    if (!cJSON_IsNumber(v) || v->valuedouble < 0 || v->valuedouble > UINT32_MAX ||
        floor(v->valuedouble) != v->valuedouble)
        return false;
    *out = (uint32_t)v->valuedouble;
    return true;
}
static bool valid_id(const char *s) {
    if (!s || strlen(s) != 8)
        return false;
    for (unsigned i = 0; i < 8; i++)
        if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f')))
            return false;
    return true;
}
static bool valid_name(const char *s) {
    if (!s)
        return false;
    size_t n = strnlen(s, 33);
    if (!n || n > 32 || s[0] == ' ' || s[n - 1] == ' ')
        return false;
    for (size_t i = 0; i < n; i++)
        if ((unsigned char)s[i] < 32 || (unsigned char)s[i] == 127)
            return false;
    return true;
}
static int folder_index(const catalog_t *c, const char *id) {
    for (unsigned i = 0; i < c->folders; i++)
        if (!strcmp(c->folder[i].id, id))
            return i;
    return -1;
}
static const char *placement(const char *id) {
    for (unsigned i = 0; i < catalog.placements; i++)
        if (!strcmp(catalog.placement[i].package_id, id))
            return catalog.placement[i].folder;
    return "";
}
static void slot_path(int slot, char *out, size_t n) {
    snprintf(out, n, "%s/system/project-folders-%c.json", CYD_ROOT, slot ? 'b' : 'a');
}
static esp_err_t load_slot(int slot, catalog_t *out) {
    char path[256];
    slot_path(slot, path, sizeof(path));
    uint8_t *data = NULL;
    size_t n;
    esp_err_t e = cyd_read_file(path, &data, &n, 16384);
    if (e)
        return e;
    cJSON *root = cyd_json_parse(data, n, 16384);
    free(data);
    if (!root)
        return ESP_ERR_INVALID_ARG;
    uint32_t generation, checksum, format;
    char *canonical = NULL;
    e = ESP_ERR_INVALID_ARG;
    if (!uint32_value(cJSON_GetObjectItem(root, "format"), &format) || format != 1 ||
        !uint32_value(cJSON_GetObjectItem(root, "generation"), &generation) || !generation ||
        !uint32_value(cJSON_GetObjectItem(root, "checksum"), &checksum))
        goto done;
    cJSON_DeleteItemFromObjectCaseSensitive(root, "checksum");
    canonical = cJSON_PrintUnformatted(root);
    if (!canonical || cyd_crc32((uint8_t *)canonical, strlen(canonical)) != checksum)
        goto done;
    cJSON *folders = cJSON_GetObjectItem(root, "folders"),
          *placements = cJSON_GetObjectItem(root, "placements");
    if (!cJSON_IsArray(folders) || !cJSON_IsArray(placements) ||
        cJSON_GetArraySize(folders) > CYD_PROJECT_MAX_FOLDERS ||
        cJSON_GetArraySize(placements) > CYD_PROJECT_MAX_PLACEMENTS)
        goto done;
    memset(out, 0, sizeof(*out));
    out->generation = generation;
    cJSON *v;
    cJSON_ArrayForEach(v, folders) {
        const char *id = str(v, "id"), *name = str(v, "name");
        if (!valid_id(id) || !valid_name(name) || folder_index(out, id) >= 0)
            goto done;
        for (unsigned j = 0; j < out->folders; j++)
            if (!strcmp(out->folder[j].name, name))
                goto done;
        folder_t *f = &out->folder[out->folders++];
        snprintf(f->id, 9, "%s", id);
        snprintf(f->name, 33, "%s", name);
    }
    cJSON_ArrayForEach(v, placements) {
        const char *id = str(v, "package_id"), *folder = str(v, "folder");
        if (!cyd_safe_id(id) || !valid_id(folder) || folder_index(out, folder) < 0)
            goto done;
        for (unsigned j = 0; j < out->placements; j++)
            if (!strcmp(out->placement[j].package_id, id))
                goto done;
        placement_t *p = &out->placement[out->placements++];
        snprintf(p->package_id, 65, "%s", id);
        snprintf(p->folder, 9, "%s", folder);
    }
    e = ESP_OK;
done:
    free(canonical);
    cJSON_Delete(root);
    return e;
}
static esp_err_t ensure_loaded(void) {
    if (loaded)
        return ESP_OK;
    if (!cyd_storage_ready())
        return ESP_ERR_INVALID_STATE;
    catalog_t *candidate = calloc(1, sizeof(*candidate));
    if (!candidate)
        return ESP_ERR_NO_MEM;
    bool found = false, broken = false;
    for (int i = 0; i < 2; i++) {
        char path[256];
        slot_path(i, path, sizeof(path));
        struct stat s;
        if (stat(path, &s))
            continue;
        if (load_slot(i, candidate) == ESP_OK) {
            if (!found || candidate->generation > catalog.generation) {
                catalog = *candidate;
                active_slot = i;
            }
            found = true;
        } else
            broken = true;
    }
    free(candidate);
    if (!found && broken)
        return ESP_FAIL;
    if (!found) {
        memset(&catalog, 0, sizeof(catalog));
        active_slot = 1;
    }
    loaded = true;
    return ESP_OK;
}
static esp_err_t commit(catalog_t *next) {
    if (catalog.generation == UINT32_MAX)
        return ESP_ERR_INVALID_STATE;
    next->generation = catalog.generation + 1;
    cJSON *root = cJSON_CreateObject(), *folders = cJSON_CreateArray(),
          *placements = cJSON_CreateArray();
    if (!root || !folders || !placements) {
        cJSON_Delete(root);
        cJSON_Delete(folders);
        cJSON_Delete(placements);
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddNumberToObject(root, "format", 1);
    cJSON_AddNumberToObject(root, "generation", next->generation);
    cJSON_AddItemToObject(root, "folders", folders);
    cJSON_AddItemToObject(root, "placements", placements);
    for (unsigned i = 0; i < next->folders; i++) {
        cJSON *f = cJSON_CreateObject();
        cJSON_AddStringToObject(f, "id", next->folder[i].id);
        cJSON_AddStringToObject(f, "name", next->folder[i].name);
        cJSON_AddItemToArray(folders, f);
    }
    for (unsigned i = 0; i < next->placements; i++) {
        cJSON *p = cJSON_CreateObject();
        cJSON_AddStringToObject(p, "package_id", next->placement[i].package_id);
        cJSON_AddStringToObject(p, "folder", next->placement[i].folder);
        cJSON_AddItemToArray(placements, p);
    }
    char *data = cJSON_PrintUnformatted(root);
    if (!data) {
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }
    uint32_t checksum = cyd_crc32((uint8_t *)data, strlen(data));
    free(data);
    cJSON_AddNumberToObject(root, "checksum", checksum);
    data = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!data)
        return ESP_ERR_NO_MEM;
    char path[256];
    slot_path(1 - active_slot, path, sizeof(path));
    esp_err_t e =
        strlen(data) > 16384 ? ESP_ERR_INVALID_SIZE : cyd_write_file(path, data, strlen(data));
    free(data);
    if (!e) {
        catalog = *next;
        active_slot = 1 - active_slot;
    }
    return e;
}
esp_err_t cyd_projects_init(void) {
    guard = xSemaphoreCreateMutex();
    loaded = false;
    return guard ? ESP_OK : ESP_ERR_NO_MEM;
}
static esp_err_t lock_catalog(void) {
    if (!guard || cyd_maintenance_active())
        return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(guard, portMAX_DELAY);
    esp_err_t e = ensure_loaded();
    if (e)
        xSemaphoreGive(guard);
    return e;
}
static esp_err_t finish(catalog_t *next) {
    esp_err_t e = next ? commit(next) : ESP_ERR_NO_MEM;
    free(next);
    xSemaphoreGive(guard);
    return e;
}
esp_err_t cyd_project_folder_create(const char *name, char id[9]) {
    if (!valid_name(name) || !id)
        return ESP_ERR_INVALID_ARG;
    esp_err_t e = lock_catalog();
    if (e)
        return e;
    if (catalog.folders >= CYD_PROJECT_MAX_FOLDERS) {
        xSemaphoreGive(guard);
        return ESP_ERR_NO_MEM;
    }
    for (unsigned i = 0; i < catalog.folders; i++)
        if (!strcmp(catalog.folder[i].name, name)) {
            xSemaphoreGive(guard);
            return ESP_ERR_INVALID_ARG;
        }
    catalog_t *next = malloc(sizeof(*next));
    if (!next)
        return finish(NULL);
    *next = catalog;
    for (unsigned tries = 0;; tries++) {
        snprintf(id, 9, "%08lx", (unsigned long)esp_random());
        if (folder_index(next, id) < 0)
            break;
        if (tries >= 16) {
            free(next);
            xSemaphoreGive(guard);
            return ESP_FAIL;
        }
    }
    folder_t *f = &next->folder[next->folders++];
    snprintf(f->id, 9, "%s", id);
    snprintf(f->name, 33, "%s", name);
    return finish(next);
}
esp_err_t cyd_project_folder_rename(const char *id, const char *name) {
    if (!valid_id(id) || !valid_name(name))
        return ESP_ERR_INVALID_ARG;
    esp_err_t e = lock_catalog();
    if (e)
        return e;
    int index = folder_index(&catalog, id);
    if (index < 0) {
        xSemaphoreGive(guard);
        return ESP_ERR_NOT_FOUND;
    }
    for (unsigned i = 0; i < catalog.folders; i++)
        if (i != (unsigned)index && !strcmp(catalog.folder[i].name, name)) {
            xSemaphoreGive(guard);
            return ESP_ERR_INVALID_ARG;
        }
    catalog_t *next = malloc(sizeof(*next));
    if (!next)
        return finish(NULL);
    *next = catalog;
    snprintf(next->folder[index].name, 33, "%s", name);
    return finish(next);
}
esp_err_t cyd_project_folder_delete(const char *id) {
    if (!valid_id(id))
        return ESP_ERR_INVALID_ARG;
    esp_err_t e = lock_catalog();
    if (e)
        return e;
    int index = folder_index(&catalog, id);
    if (index < 0) {
        xSemaphoreGive(guard);
        return ESP_ERR_NOT_FOUND;
    }
    catalog_t *next = malloc(sizeof(*next));
    if (!next)
        return finish(NULL);
    *next = catalog;
    for (unsigned i = index; i + 1 < next->folders; i++)
        next->folder[i] = next->folder[i + 1];
    next->folders--;
    unsigned count = 0;
    for (unsigned i = 0; i < next->placements; i++)
        if (strcmp(next->placement[i].folder, id))
            next->placement[count++] = next->placement[i];
    next->placements = count;
    return finish(next);
}
static bool installed(cJSON *versions, const char *id) {
    cJSON *v;
    cJSON_ArrayForEach(v, versions) {
        const char *p = str(v, "package_id");
        if (p && !strcmp(p, id))
            return true;
    }
    return false;
}
esp_err_t cyd_project_move(const char *id, const char *folder) {
    if (!cyd_safe_id(id) || !folder || (*folder && !valid_id(folder)))
        return ESP_ERR_INVALID_ARG;
    cJSON *versions = cyd_versions();
    if (!versions)
        return ESP_ERR_NO_MEM;
    if (!installed(versions, id)) {
        cJSON_Delete(versions);
        return ESP_ERR_NOT_FOUND;
    }
    esp_err_t e = lock_catalog();
    if (e) {
        cJSON_Delete(versions);
        return e;
    }
    if (*folder && folder_index(&catalog, folder) < 0) {
        xSemaphoreGive(guard);
        cJSON_Delete(versions);
        return ESP_ERR_NOT_FOUND;
    }
    catalog_t *next = malloc(sizeof(*next));
    if (!next) {
        cJSON_Delete(versions);
        return finish(NULL);
    }
    *next = catalog;
    unsigned count = 0;
    for (unsigned i = 0; i < next->placements; i++)
        if (strcmp(next->placement[i].package_id, id) &&
            installed(versions, next->placement[i].package_id))
            next->placement[count++] = next->placement[i];
    next->placements = count;
    cJSON_Delete(versions);
    if (*folder) {
        if (count >= CYD_PROJECT_MAX_PLACEMENTS) {
            free(next);
            xSemaphoreGive(guard);
            return ESP_ERR_NO_MEM;
        }
        placement_t *p = &next->placement[next->placements++];
        snprintf(p->package_id, 65, "%s", id);
        snprintf(p->folder, 9, "%s", folder);
    }
    return finish(next);
}
cJSON *cyd_projects_catalog(void) {
    cJSON *versions = cyd_versions(), *root = cJSON_CreateObject(), *folders = cJSON_CreateArray(),
          *projects = cJSON_CreateArray();
    if (!versions || !root || !folders || !projects) {
        cJSON_Delete(versions);
        cJSON_Delete(root);
        cJSON_Delete(folders);
        cJSON_Delete(projects);
        return NULL;
    }
    cJSON_AddItemToObject(root, "folders", folders);
    cJSON_AddItemToObject(root, "projects", projects);
    xSemaphoreTake(guard, portMAX_DELAY);
    esp_err_t e = ensure_loaded();
    if (e)
        cJSON_AddStringToObject(root, "catalog_error",
                                cyd_storage_ready()
                                    ? "Folder metadata could not be read; apps are retained"
                                    : "Insert an SD card to use Apps");
    if (!e)
        for (unsigned i = 0; i < catalog.folders; i++) {
            cJSON *f = cJSON_CreateObject();
            cJSON_AddStringToObject(f, "id", catalog.folder[i].id);
            cJSON_AddStringToObject(f, "name", catalog.folder[i].name);
            cJSON_AddItemToArray(folders, f);
        }
    while (cJSON_GetArraySize(versions)) {
        cJSON *v = cJSON_DetachItemFromArray(versions, 0);
        const char *id = str(v, "package_id"), *name = str(v, "name");
        if (!id || !name) {
            cJSON_Delete(v);
            continue;
        }
        cJSON *project = NULL, *p;
        cJSON_ArrayForEach(p, projects) {
            const char *existing = str(p, "package_id");
            if (existing && !strcmp(existing, id)) {
                project = p;
                break;
            }
        }
        if (!project) {
            project = cJSON_CreateObject();
            cJSON_AddStringToObject(project, "package_id", id);
            cJSON_AddStringToObject(project, "name", name);
            cJSON_AddStringToObject(project, "folder", e ? "" : placement(id));
            cJSON_AddItemToObject(project, "versions", cJSON_CreateArray());
            cJSON_AddItemToArray(projects, project);
        }
        cJSON_AddItemToArray(cJSON_GetObjectItem(project, "versions"), v);
    }
    xSemaphoreGive(guard);
    cJSON_Delete(versions);
    return root;
}
