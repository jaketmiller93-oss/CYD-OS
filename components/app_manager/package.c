#include "cyd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
esp_err_t cyd_package_validate(const uint8_t *bytes, size_t n, cyd_zip_t *zip, cyd_manifest_t *m,
                               char *error, size_t cap) {
    const char *why = "Invalid ZIP_STORED package";
    esp_err_t e = ESP_ERR_INVALID_ARG;
    if (cyd_zip_parse(bytes, n, zip))
        goto fail;
    cyd_zip_file_t *mf = NULL;
    for (unsigned i = 0; i < zip->count; i++) {
        if (!strcmp(zip->files[i].name, "package.cydapp")) {
            why = "Reserved package filename";
            goto fail;
        }
        if (!strcmp(zip->files[i].name, "manifest.json"))
            mf = &zip->files[i];
    }
    if (!mf || mf->size > 4096) {
        why = "Missing or oversized manifest";
        goto fail;
    }
    e = cyd_manifest_parse(mf->data, mf->size, m);
    if (e) {
        why = "Invalid manifest, API, target or permission";
        goto fail;
    }
    for (unsigned i = 0; i < zip->count; i++)
        if (!strcmp(zip->files[i].name, m->entry) && zip->files[i].size) {
            if (cap)
                error[0] = 0;
            return ESP_OK;
        }
    e = ESP_ERR_INVALID_ARG;
    why = "Missing or empty entry point";
fail:
    snprintf(error, cap, "%s", why);
    return e;
}
cJSON *cyd_package_inspect(const uint8_t *bytes, size_t n, char *error, size_t cap) {
    cyd_zip_t *zip = calloc(1, sizeof(*zip));
    if (!zip) {
        snprintf(error, cap, "Allocation failed");
        return NULL;
    }
    cyd_manifest_t m;
    cJSON *out = NULL;
    if (!cyd_package_validate(bytes, n, zip, &m, error, cap))
        for (unsigned i = 0; i < zip->count; i++)
            if (!strcmp(zip->files[i].name, "manifest.json")) {
                out = cyd_json_parse(zip->files[i].data, zip->files[i].size, 4096);
                break;
            }
    free(zip);
    return out;
}
