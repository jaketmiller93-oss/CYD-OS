#include "cyd.h"
#include <errno.h>
#include <sys/stat.h>
#include <string.h>
static bool ready = true, maintenance;
void files_flags(int mounted, int locked) {
    ready = mounted;
    maintenance = locked;
}
bool cyd_storage_ready(void) {
    return ready;
}
bool cyd_maintenance_active(void) {
    return maintenance;
}
esp_err_t cyd_mkdirs(const char *path) {
    char p[512];
    if (strlen(path) >= sizeof(p))
        return ESP_ERR_INVALID_SIZE;
    strcpy(p, path);
    for (char *c = p + 1; *c; c++)
        if (*c == '/') {
            *c = 0;
            if (mkdir(p, 0700) && errno != EEXIST)
                return ESP_FAIL;
            *c = '/';
        }
    return !mkdir(p, 0700) || errno == EEXIST ? ESP_OK : ESP_FAIL;
}
