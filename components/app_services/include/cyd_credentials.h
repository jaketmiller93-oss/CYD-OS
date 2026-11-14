#pragma once
#include "esp_err.h"
#include <stddef.h>
/* Full package identity is stored and compared; there is no identity hash
 * alias. */
esp_err_t cyd_credentials_get(const char *app, const char *key, void *value, size_t *size);
esp_err_t cyd_credentials_set(const char *app, const char *key, const void *value, size_t size);
esp_err_t cyd_credentials_delete(const char *app, const char *key);
