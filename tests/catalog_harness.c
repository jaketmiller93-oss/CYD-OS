#include "system_sdk/cyd.h"
#include "../components/app_manager/catalog.c"
static const char *versions_json =
    "[{\"package_id\":\"org.example.hello\",\"name\":\"Hello\",\"version\":\"1\",\"revision\":"
    "\"1111111111111111\"},{\"package_id\":\"org.example.hello\",\"name\":\"Hello\",\"version\":"
    "\"2\",\"revision\":\"2222222222222222\"},{\"package_id\":\"org.example.clock\",\"name\":"
    "\"Clock\",\"version\":\"1\",\"revision\":\"3333333333333333\"}]";
static bool fail_write, sd_ready;
cJSON *cyd_versions(void) {
    return cJSON_Parse(versions_json);
}
bool cyd_storage_ready(void) {
    return sd_ready;
}
esp_err_t cyd_read_file(const char *path, uint8_t **out, size_t *n, size_t max) {
    FILE *f = fopen(path, "rb");
    if (!f)
        return ESP_ERR_NOT_FOUND;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    if (size < 0 || (size_t)size > max) {
        fclose(f);
        return ESP_FAIL;
    }
    *out = calloc(1, size + 1);
    *n = fread(*out, 1, size, f);
    fclose(f);
    return 0;
}
esp_err_t cyd_write_file(const char *path, const void *data, size_t n) {
    if (fail_write)
        return ESP_FAIL;
    FILE *f = fopen(path, "wb");
    if (!f)
        return ESP_FAIL;
    bool ok = fwrite(data, 1, n, f) == n;
    fclose(f);
    return ok ? 0 : ESP_FAIL;
}
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x))                                                                                  \
            return __LINE__;                                                                       \
    } while (0)
int host_catalog_case(int which) {
    host_reset();
    mkdir(CYD_ROOT, 0700);
    mkdir(CYD_ROOT "/system", 0700);
    remove(CYD_ROOT "/system/project-folders-a.json");
    remove(CYD_ROOT "/system/project-folders-b.json");
    sd_ready = true;
    fail_write = false;
    cyd_projects_init();
    char id[9], id2[9];
    if (which == 0) {
        cJSON *o = cyd_projects_catalog();
        cJSON *p = cJSON_GetObjectItem(o, "projects");
        CHECK(cJSON_GetArraySize(p) == 2);
        CHECK(cJSON_GetArraySize(cJSON_GetObjectItem(cJSON_GetArrayItem(p, 0), "versions")) == 2);
        cJSON_Delete(o);
        return 0;
    }
    CHECK(cyd_project_folder_create("Tools", id) == 0);
    if (which == 1) {
        CHECK(cyd_project_move("org.example.hello", id) == 0);
        loaded = false;
        CHECK(ensure_loaded() == 0);
        CHECK(!strcmp(placement("org.example.hello"), id));
        CHECK(cyd_project_folder_delete(id) == 0);
        CHECK(!*placement("org.example.hello"));
        cJSON *o = cyd_projects_catalog();
        CHECK(cJSON_GetArraySize(cJSON_GetObjectItem(o, "projects")) == 2);
        cJSON_Delete(o);
        return 0;
    }
    if (which == 2) {
        CHECK(cyd_project_folder_rename(id, "Renamed") == 0);
        loaded = false;
        CHECK(ensure_loaded() == 0);
        CHECK(!strcmp(catalog.folder[0].name, "Renamed"));
        return 0;
    }
    if (which == 3) {
        CHECK(cyd_project_folder_rename(id, "Renamed") == 0);
        char path[256];
        slot_path(active_slot, path, sizeof(path));
        FILE *f = fopen(path, "wb");
        fputs("{broken", f);
        fclose(f);
        loaded = false;
        CHECK(ensure_loaded() == 0);
        CHECK(!strcmp(catalog.folder[0].name, "Tools"));
        return 0;
    }
    if (which == 4) {
        fail_write = true;
        CHECK(cyd_project_folder_rename(id, "Bad") != 0);
        CHECK(!strcmp(catalog.folder[0].name, "Tools"));
        return 0;
    }
    if (which == 5) {
        CHECK(cyd_project_folder_create("Tools", id2) != 0);
        CHECK(cyd_project_folder_create(" ", id2) != 0);
        CHECK(cyd_project_folder_create("bad\nname", id2) != 0);
        CHECK(cyd_project_move("../escape", id) != 0);
        CHECK(cyd_project_move("missing", id) != 0);
        CHECK(cyd_project_folder_delete("../x") != 0);
        return 0;
    }
    if (which == 6) {
        CHECK(cyd_maintenance_begin());
        CHECK(cyd_project_folder_create("Blocked", id2) != 0);
        CHECK(catalog.folders == 1);
        return 0;
    }
    if (which == 7) {
        for (unsigned i = 1; i < 16; i++) {
            char name[32];
            snprintf(name, sizeof(name), "Folder%u", i);
            CHECK(cyd_project_folder_create(name, id2) == 0);
        }
        CHECK(cyd_project_folder_create("Overflow", id2) != 0);
        return 0;
    }
    return 0;
}
