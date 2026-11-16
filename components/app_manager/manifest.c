#include "cyd.h"
#include "sdkconfig.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static const char *str(cJSON *o, const char *k) {
    cJSON *v = cJSON_GetObjectItemCaseSensitive(o, k);
    return cJSON_IsString(v) ? v->valuestring : NULL;
}
esp_err_t cyd_manifest_parse(const void *data, size_t n, cyd_manifest_t *m) {
    if (!data || !m || !n || n > 4096 || memchr(data, 0, n))
        return ESP_ERR_INVALID_ARG;
    /* cJSON exposes C strings; reject decoded NULs rather than accepting aliases.
     */
    for (size_t i = 0; i + 6 <= n; i++)
        if (!memcmp((const char *)data + i, "\\u0000", 6))
            return ESP_ERR_INVALID_ARG;
    /* Bound cJSON recursion on the HTTP task stack before parsing any object. */
    unsigned depth = 0;
    bool in_string = false, escaped = false;
    for (size_t i = 0; i < n; i++) {
        char c = ((const char *)data)[i];
        if (in_string) {
            if (escaped)
                escaped = false;
            else if (c == '\\')
                escaped = true;
            else if (c == '"')
                in_string = false;
        } else if (c == '"')
            in_string = true;
        else if (c == '{' || c == '[') {
            if (++depth > 8)
                return ESP_ERR_INVALID_ARG;
        } else if (c == '}' || c == ']') {
            if (!depth)
                return ESP_ERR_INVALID_ARG;
            depth--;
        }
    }
    if (in_string || depth)
        return ESP_ERR_INVALID_ARG;
    char *copy = calloc(1, n + 1);
    if (!copy)
        return ESP_ERR_NO_MEM;
    memcpy(copy, data, n);
    cJSON *o = cJSON_ParseWithLengthOpts(copy, n + 1, NULL, true);
    free(copy);
    if (!o)
        return ESP_ERR_INVALID_ARG;
    memset(m, 0, sizeof(*m));
    esp_err_t err = ESP_ERR_INVALID_ARG;
    if (!cJSON_IsObject(o))
        goto done;
    for (cJSON *a = o->child; a; a = a->next)
        for (cJSON *b = a->next; b; b = b->next)
            if (!strcmp(a->string, b->string))
                goto done;
    const char *id = str(o, "package_id"), *name = str(o, "name"), *ver = str(o, "version"),
               *entry = str(o, "entry_point");
    cJSON *api = cJSON_GetObjectItemCaseSensitive(o, "api_version"),
          *targets = cJSON_GetObjectItemCaseSensitive(o, "target"),
          *perms = cJSON_GetObjectItemCaseSensitive(o, "permissions");
    if (!cyd_safe_id(id) || !name || !*name || strlen(name) > 64 || !ver || !*ver ||
        strlen(ver) > 32 || !cyd_safe_path(entry) || !cJSON_IsNumber(api) ||
        api->valuedouble != 1 || !cJSON_IsArray(targets) || !cJSON_IsArray(perms))
        goto done;
    for (const char *p = name; *p; p++)
        if ((unsigned char)*p < 32)
            goto done;
    for (const char *p = ver; *p; p++)
        if ((unsigned char)*p < 32)
            goto done;
    const char *icon = str(o, "icon");
    if (cJSON_GetObjectItemCaseSensitive(o, "icon") && (!icon || !cyd_safe_path(icon)))
        goto done;
    bool target = false;
    cJSON *v;
    cJSON_ArrayForEach(v, targets) {
        if (!cJSON_IsString(v))
            goto done;
        if (!strcmp(v->valuestring, CONFIG_IDF_TARGET))
            target = true;
    }
    if (!target) {
        err = ESP_ERR_NOT_SUPPORTED;
        goto done;
    }
    cJSON_ArrayForEach(v, perms) {
        if (!cJSON_IsString(v))
            goto done;
        if (!strcmp(v->valuestring, "storage"))
            m->storage = true;
        else if (!strcmp(v->valuestring, "network"))
            m->network = true;
        else if (!strcmp(v->valuestring, "mqtt"))
            m->mqtt = true;
        else if (!strcmp(v->valuestring, "credentials"))
            m->credentials = true;
        else if (!strcmp(v->valuestring, "documents"))
            m->documents = true;
        else {
            err = ESP_ERR_NOT_SUPPORTED;
            goto done;
        }
    }
    snprintf(m->id, sizeof(m->id), "%s", id);
    snprintf(m->name, sizeof(m->name), "%s", name);
    snprintf(m->version, sizeof(m->version), "%s", ver);
    snprintf(m->entry, sizeof(m->entry), "%s", entry);
    err = ESP_OK;
done:
    cJSON_Delete(o);
    return err;
}
