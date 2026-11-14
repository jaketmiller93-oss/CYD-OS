#include "cyd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include "esp_vfs_fat.h"
static bool ready;
bool cyd_storage_ready(void) {
    return ready;
}
esp_err_t cyd_mkdirs(const char *p) {
    char b[320];
    if (strlen(p) >= sizeof(b))
        return ESP_ERR_INVALID_SIZE;
    strcpy(b, p);
    for (char *c = b + 1; *c; c++)
        if (*c == '/') {
            *c = 0;
            if (mkdir(b, 0775) && errno != EEXIST)
                return ESP_FAIL;
            *c = '/';
        }
    return (!mkdir(b, 0775) || errno == EEXIST) ? ESP_OK : ESP_FAIL;
}
esp_err_t cyd_storage_init(void) {
    esp_err_t e = cyd_board_sd_mount();
    if (e != ESP_OK)
        return e;
    const char *dirs[] = {"packages", "apps",  "projects", "backups",
                          "logs",     "icons", "system",   "data"};
    for (unsigned i = 0; i < sizeof(dirs) / sizeof(*dirs); i++) {
        char p[128];
        snprintf(p, sizeof(p), CYD_ROOT "/%s", dirs[i]);
        if (cyd_mkdirs(p))
            return ESP_FAIL;
    }
    /* A staging directory is never executable. Remove interrupted installations. */
    cyd_remove_tree(CYD_ROOT "/staging");
    ready = true;
    return ESP_OK;
}
esp_err_t cyd_write_file(const char *path, const void *data, size_t n) {
    FILE *f = fopen(path, "wb");
    if (!f)
        return ESP_FAIL;
    bool ok = fwrite(data, 1, n, f) == n;
    if (fflush(f))
        ok = false;
    if (fsync(fileno(f)))
        ok = false;
    if (fclose(f))
        ok = false;
    return ok ? ESP_OK : ESP_FAIL;
}
esp_err_t cyd_read_file(const char *p, uint8_t **data, size_t *n, size_t max) {
    struct stat s;
    if (stat(p, &s) || s.st_size < 0 || (size_t)s.st_size > max)
        return ESP_ERR_INVALID_SIZE;
    FILE *f = fopen(p, "rb");
    if (!f)
        return ESP_ERR_NOT_FOUND;
    *data = malloc(s.st_size + 1);
    if (!*data) {
        fclose(f);
        return ESP_ERR_NO_MEM;
    }
    *n = fread(*data, 1, s.st_size, f);
    fclose(f);
    (*data)[*n] = 0;
    if (*n != (size_t)s.st_size) {
        free(*data);
        *data = NULL;
        return ESP_FAIL;
    }
    return ESP_OK;
}
void cyd_remove_tree(const char *p) {
    DIR *d = opendir(p);
    if (!d) {
        unlink(p);
        return;
    }
    struct dirent *e;
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
            continue;
        char child[512];
        if (snprintf(child, sizeof(child), "%s/%s", p, e->d_name) >= (int)sizeof(child))
            continue;
        struct stat s;
        if (!stat(child, &s) && S_ISDIR(s.st_mode))
            cyd_remove_tree(child);
        else
            unlink(child);
    }
    closedir(d);
    rmdir(p);
}
uint64_t cyd_storage_free(void) {
    uint64_t total = 0, free_bytes = 0;
    if (!ready || esp_vfs_fat_info("/sdcard", &total, &free_bytes) != ESP_OK)
        return 0;
    return free_bytes;
}
