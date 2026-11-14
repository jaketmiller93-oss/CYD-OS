#pragma once
#include "zip_host.h"
#include "cJSON.h"
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_ERR_INVALID_ARG 1
#define ESP_ERR_NO_MEM 2
#define ESP_ERR_NOT_SUPPORTED 3
typedef struct {
    char id[65], name[65], version[33], entry[CYD_MAX_PATH];
    bool storage;
} cyd_manifest_t;
bool cyd_safe_path(const char *);
bool cyd_safe_id(const char *);
