#pragma once
#include "cyd.h"
typedef void *httpd_handle_t;
typedef struct { unsigned stack_size, task_caps, max_uri_handlers;
    unsigned recv_wait_timeout, send_wait_timeout; bool lru_purge_enable; } httpd_config_t;
#define HTTPD_DEFAULT_CONFIG() { .stack_size = 4096, .task_caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT }
typedef struct { const char *uri; } httpd_uri_t;
esp_err_t httpd_start(httpd_handle_t *, const httpd_config_t *);
esp_err_t httpd_stop(httpd_handle_t);
esp_err_t httpd_register_uri_handler(httpd_handle_t, const httpd_uri_t *);
