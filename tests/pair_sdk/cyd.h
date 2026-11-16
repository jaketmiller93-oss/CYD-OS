#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "cJSON.h"
typedef int esp_err_t;
typedef void *SemaphoreHandle_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NO_MEM 0x101
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_INVALID_SIZE 0x104
#define ESP_ERR_NOT_FOUND 0x105
#define ESP_ERR_NOT_SUPPORTED 0x106
#define ESP_ERR_TIMEOUT 0x107
#define ESP_ERR_INVALID_RESPONSE 0x108
#define pdTRUE 1
#define pdPASS 1
#define portMAX_DELAY 0xffffffffu
#define pdMS_TO_TICKS(n) (n)
SemaphoreHandle_t xSemaphoreCreateMutex(void);
int xSemaphoreTake(SemaphoreHandle_t,unsigned);
int xSemaphoreGive(SemaphoreHandle_t);
void vSemaphoreDelete(SemaphoreHandle_t);
cJSON *cyd_json_parse(const void *,size_t,size_t);

#ifndef CYD_ROOT
#define CYD_ROOT "/tmp/cyd-pair-host"
#endif
const char *cyd_system_version(void);
bool cyd_storage_ready(void);
uint64_t cyd_storage_free(void);
bool cyd_maintenance_active(void);
esp_err_t cyd_runtime_stop(void);
void cyd_log(const char *,const char *);
bool cyd_safe_path(const char *);
esp_err_t cyd_mkdirs(const char *);

#define CYD_MAX_PATH 96
#define CYD_MAX_FILES 48
#define CYD_MAX_PACKAGE (512*1024)
typedef struct {char name[CYD_MAX_PATH];const uint8_t *data;size_t size;} cyd_zip_file_t;
typedef struct {cyd_zip_file_t files[CYD_MAX_FILES];unsigned count;} cyd_zip_t;
bool cyd_safe_id(const char *);
uint32_t cyd_crc32(const uint8_t *,size_t);

#define CYD_MAX_SCRIPT (64*1024)
