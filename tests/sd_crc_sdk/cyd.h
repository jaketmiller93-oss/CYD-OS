#pragma once
#include <stdbool.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_ERR_NOT_SUPPORTED 0x106
#define ESP_ERR_TIMEOUT 0x107
#define ESP_ERR_INVALID_CRC 0x109
void cyd_log(const char *, const char *);
