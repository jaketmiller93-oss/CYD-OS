#include "cyd_access.h"
#include "cyd.h"
#include "sdkconfig.h"
#include "nvs.h"
#include "esp_random.h"
#include "bootloader_random.h"
#include <stdio.h>
#include <string.h>
static SemaphoreHandle_t mutex;
static nvs_handle_t store;
static char token[CYD_ACCESS_MAX_TOKEN + 1];
static bool ready;
static bool valid(const char *s) {
    size_t n = strnlen(s, CYD_ACCESS_MAX_TOKEN + 1);
    if (n < 24 || n > CYD_ACCESS_MAX_TOKEN)
        return false;
    for (size_t i = 0; i < n; i++)
        if ((unsigned char)s[i] < 33 || (unsigned char)s[i] > 126)
            return false;
    return true;
}
static void wipe(void *p, size_t n) {
    volatile unsigned char *b = p;
    while (n--)
        *b++ = 0;
}
static void generate(char out[49]) {
    for (unsigned i = 0; i < 6; i++)
        snprintf(out + i * 8, 9, "%08lx", (unsigned long)esp_random());
}
static esp_err_t save(const char *next) {
    esp_err_t e = nvs_set_str(store, "token", next);
    if (!e)
        e = nvs_commit(store);
    if (!e) {
        xSemaphoreTake(mutex, portMAX_DELAY);
        snprintf(token, sizeof(token), "%s", next);
        ready = true;
        xSemaphoreGive(mutex);
    }
    return e;
}
esp_err_t cyd_access_init(void) {
    mutex = xSemaphoreCreateMutex();
    if (!mutex)
        return ESP_ERR_NO_MEM;
    esp_err_t e = nvs_open("cyd_access", NVS_READWRITE, &store);
    if (e)
        return e;
    size_t n = sizeof(token);
    e = nvs_get_str(store, "token", token, &n);
    if (!e) {
        if (!valid(token)) {
            wipe(token, sizeof(token));
            return ESP_ERR_INVALID_STATE;
        }
        ready = true;
        return ESP_OK;
    }
    /* A malformed existing record must never silently replace a working credential. */
    if (e != ESP_ERR_NVS_NOT_FOUND)
        return e;
    char initial[CYD_ACCESS_MAX_TOKEN + 1] = {0};
    if (valid(CONFIG_CYD_API_TOKEN))
        snprintf(initial, sizeof(initial), "%s", CONFIG_CYD_API_TOKEN);
    else {
        /* Initialization precedes all ADC/radio use. Enable hardware entropy while
           creating the first credential, then restore normal peripheral operation. */
        bootloader_random_enable();
        generate(initial);
        bootloader_random_disable();
    }
    e = save(initial);
    wipe(initial, sizeof(initial));
    return e;
}
bool cyd_access_ready(void) {
    if (!mutex)
        return false;
    xSemaphoreTake(mutex, portMAX_DELAY);
    bool r = ready;
    xSemaphoreGive(mutex);
    return r;
}
bool cyd_access_authorized(const char *h, size_t length) {
    if (!h || !mutex || length < 7 || length > CYD_ACCESS_MAX_TOKEN + 7)
        return false;
    xSemaphoreTake(mutex, portMAX_DELAY);
    size_t n = strlen(token), given = length - 7;
    unsigned diff = (unsigned)(given ^ n);
    for (unsigned i = 0; i < 7; i++)
        diff |= (unsigned char)h[i] ^ (unsigned char)"Bearer "[i];
    for (size_t i = 0; i < n; i++)
        diff |= (unsigned char)token[i] ^ (unsigned char)(i < given ? h[i + 7] : 0);
    bool ok = ready && n >= 24 && diff == 0;
    xSemaphoreGive(mutex);
    return ok;
}
void cyd_access_get_token(char *out, size_t n) {
    if (!n)
        return;
    out[0] = 0;
    if (!mutex)
        return;
    xSemaphoreTake(mutex, portMAX_DELAY);
    if (ready)
        snprintf(out, n, "%s", token);
    xSemaphoreGive(mutex);
}
esp_err_t cyd_access_rotate(void) {
    if (!ready || cyd_maintenance_active())
        return ESP_ERR_INVALID_STATE;
    char next[49];
    generate(next);
    esp_err_t e = save(next);
    wipe(next, sizeof(next));
    return e;
}
