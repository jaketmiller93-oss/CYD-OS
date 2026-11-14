#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#define CYD_ACCESS_MAX_TOKEN 240
esp_err_t cyd_access_init(void);
bool cyd_access_ready(void);
bool cyd_access_authorized(const char *header, size_t length);
/* Physical UI only. Never expose tokens in HTTP responses, logs or Lua. */
void cyd_access_get_token(char *out, size_t size);
esp_err_t cyd_access_rotate(void);
