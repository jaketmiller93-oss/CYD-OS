#pragma once
#include <stdbool.h>
#include <stddef.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_HTTPD_TASK 0xb008
#define ESP_ERR_HTTPD_HANDLERS_FULL 0xb001
#define MALLOC_CAP_INTERNAL 1u
#define MALLOC_CAP_8BIT 4u
const char *esp_err_to_name(esp_err_t);
void cyd_log(const char *, const char *);
