#include "cyd_builtin.h"
#include "sdkconfig.h"
#include <string.h>
#include <stdio.h>
typedef struct {
    const char *key, *name, *code;
    size_t size;
    bool storage, documents;
} builtin_t;
#include "builtin_data.inc"
unsigned cyd_builtin_count(void) {
    return sizeof(builtins) / sizeof(builtins[0]);
}
const char *cyd_builtin_name(unsigned i) {
    return i < cyd_builtin_count() ? builtins[i].name : "";
}
const char *cyd_builtin_key(unsigned i) {
    return i < cyd_builtin_count() ? builtins[i].key : "";
}
const char *cyd_builtin_source(const char *path, size_t *n) {
    if (strncmp(path, "builtin:", 8))
        return NULL;
    for (unsigned i = 0; i < cyd_builtin_count(); i++)
        if (!strcmp(path + 8, builtins[i].key)) {
            *n = builtins[i].size;
            return builtins[i].code;
        }
    return NULL;
}
esp_err_t cyd_builtin_launch(unsigned i) {
#ifndef CONFIG_CYD_PAIRED_SYSTEM
    (void)i;
    return ESP_ERR_NOT_SUPPORTED;
#else
    if (i >= cyd_builtin_count())
        return ESP_ERR_INVALID_ARG;
    const builtin_t *b = &builtins[i];
    cyd_manifest_t m = {.storage = b->storage, .documents = b->documents};
    snprintf(m.id, sizeof(m.id), "com.cydos.%s", b->key);
    snprintf(m.name, sizeof(m.name), "%s", b->name);
    snprintf(m.version, sizeof(m.version), "1.0.0");
    strcpy(m.entry, "main.lua");
    char path[64];
    snprintf(path, sizeof(path), "builtin:%s", b->key);
    return cyd_runtime_start(path, "builtin", &m);
#endif
}
