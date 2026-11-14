#include "cyd.h"
#include "cyd_credentials.h"
#include "nvs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define RECORDS 8
#define KEYS 4
typedef struct {
    char key[33];
    uint16_t size;
    unsigned char value[256];
} item_t;
typedef struct {
    uint32_t format;
    char owner[65];
    item_t item[KEYS];
} record_t;
static bool key_ok(const char *k) {
    if (!k || !*k || strlen(k) > 32)
        return false;
    for (const char *p = k; *p; p++)
        if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '_' || *p == '-'))
            return false;
    return true;
}
static void wipe(void *data, size_t n) {
    volatile unsigned char *p = data;
    while (n--)
        *p++ = 0;
}
/* One Lua foreground task owns this namespace; no OS service shares it. */
static esp_err_t operation(const char *app, const char *key, void *data, size_t *size, int action) {
    if (!cyd_safe_id(app) || !key_ok(key) || !size || *size > 256)
        return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    esp_err_t e = nvs_open("cyd_app_creds", NVS_READWRITE, &h);
    if (e)
        return e;
    record_t *r = calloc(1, sizeof(*r));
    if (!r) {
        nvs_close(h);
        return ESP_ERR_NO_MEM;
    }
    int slot = -1, free_slot = -1;
    char name[8];
    for (unsigned i = 0; i < RECORDS; i++) {
        snprintf(name, sizeof(name), "a%02u", i);
        size_t n = sizeof(*r);
        e = nvs_get_blob(h, name, r, &n);
        if (e == ESP_ERR_NVS_NOT_FOUND) {
            if (free_slot < 0)
                free_slot = i;
            continue;
        }
        if (e || n != sizeof(*r) || r->format != 1 || !memchr(r->owner, 0, sizeof(r->owner)) ||
            !cyd_safe_id(r->owner)) {
            e = ESP_ERR_INVALID_STATE;
            goto done;
        }
        for (unsigned j = 0; j < KEYS; j++)
            if (!memchr(r->item[j].key, 0, 33) || r->item[j].size > 256) {
                e = ESP_ERR_INVALID_STATE;
                goto done;
            }
        if (!strcmp(r->owner, app)) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        if (action != 1) {
            e = ESP_ERR_NOT_FOUND;
            goto done;
        }
        if (free_slot < 0) {
            e = ESP_ERR_NO_MEM;
            goto done;
        }
        slot = free_slot;
        memset(r, 0, sizeof(*r));
        r->format = 1;
        snprintf(r->owner, sizeof(r->owner), "%s", app);
    }
    int item = -1, empty = -1;
    for (unsigned j = 0; j < KEYS; j++) {
        if (!*r->item[j].key && empty < 0)
            empty = j;
        if (!strcmp(r->item[j].key, key))
            item = j;
    }
    if (action == 0) {
        if (item < 0) {
            e = ESP_ERR_NOT_FOUND;
            goto done;
        }
        if (*size < r->item[item].size) {
            e = ESP_ERR_INVALID_SIZE;
            goto done;
        }
        *size = r->item[item].size;
        memcpy(data, r->item[item].value, *size);
        e = ESP_OK;
        goto done;
    }
    if (action == 1) {
        if (item < 0)
            item = empty;
        if (item < 0) {
            e = ESP_ERR_NO_MEM;
            goto done;
        }
        wipe(&r->item[item], sizeof(item_t));
        snprintf(r->item[item].key, 33, "%s", key);
        r->item[item].size = *size;
        memcpy(r->item[item].value, data, *size);
    } else {
        if (item < 0) {
            e = ESP_ERR_NOT_FOUND;
            goto done;
        }
        wipe(&r->item[item], sizeof(item_t));
    }
    name[0] = 'a';
    name[1] = '0';
    name[2] = (char)('0' + slot);
    name[3] = 0;
    bool any = false;
    for (unsigned j = 0; j < KEYS; j++)
        any |= *r->item[j].key != 0;
    e = any ? nvs_set_blob(h, name, r, sizeof(*r)) : nvs_erase_key(h, name);
    if (!e)
        e = nvs_commit(h);
done:
    wipe(r, sizeof(*r));
    free(r);
    nvs_close(h);
    return e;
}
esp_err_t cyd_credentials_get(const char *a, const char *k, void *v, size_t *n) {
    return operation(a, k, v, n, 0);
}
esp_err_t cyd_credentials_set(const char *a, const char *k, const void *v, size_t n) {
    return operation(a, k, (void *)v, &n, 1);
}
esp_err_t cyd_credentials_delete(const char *a, const char *k) {
    size_t n = 0;
    return operation(a, k, NULL, &n, 2);
}
