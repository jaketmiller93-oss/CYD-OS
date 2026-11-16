#pragma once
#include "esp_http_server.h"
/* Start the HTTP task without exposing routes to an unfinished UI. */
esp_err_t cyd_web_prepare(void);
esp_err_t cyd_web_register(const httpd_uri_t *routes, size_t count);
