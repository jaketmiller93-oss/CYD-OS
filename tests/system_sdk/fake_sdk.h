#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <sys/time.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NO_MEM 0x101
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_INVALID_SIZE 0x104
#define ESP_ERR_NOT_FOUND 0x105
#define ESP_ERR_NOT_SUPPORTED 0x106
#define ESP_ERR_TIMEOUT 0x107
#define ESP_ERR_NVS_NOT_FOUND 0x1102
#define NVS_READWRITE 1
#define portMAX_DELAY 0xffffffffu
#define pdTRUE 1
#define pdPASS 1
#define pdMS_TO_TICKS(n) (n)
#define CONFIG_IDF_TARGET_ESP32C5 1
#define CONFIG_CYD_API_TOKEN "host-initial-token-1234567890"
#define CONFIG_CYD_C5_TOUCH_X_MIN 225
#define CONFIG_CYD_C5_TOUCH_X_SPAN 3413
#define CONFIG_CYD_C5_TOUCH_Y_MIN 403
#define CONFIG_CYD_C5_TOUCH_Y_SPAN 3334
#define ESP_CHIP_ID_ESP32C5 0x17
#define ESP_CHIP_ID_ESP32S3 9
typedef void *SemaphoreHandle_t;
typedef struct host_queue *QueueHandle_t;
typedef unsigned nvs_handle_t;
SemaphoreHandle_t xSemaphoreCreateMutex(void);
int xSemaphoreTake(SemaphoreHandle_t, unsigned);
int xSemaphoreGive(SemaphoreHandle_t);
QueueHandle_t xQueueCreate(unsigned, size_t);
int xQueueSend(QueueHandle_t, const void *, unsigned);
int xQueueReceive(QueueHandle_t, void *, unsigned);
int xTaskCreate(void (*)(void *), const char *, unsigned, void *, unsigned, void *);
void vTaskDelay(unsigned);
void vTaskDelete(void *);
int64_t esp_timer_get_time(void);
uint32_t esp_random(void);
esp_err_t nvs_open(const char *, int, nvs_handle_t *);
esp_err_t nvs_get_str(nvs_handle_t, const char *, char *, size_t *);
esp_err_t nvs_set_str(nvs_handle_t, const char *, const char *);
esp_err_t nvs_commit(nvs_handle_t);
esp_err_t nvs_get_blob(nvs_handle_t, const char *, void *, size_t *);
esp_err_t nvs_set_blob(nvs_handle_t, const char *, const void *, size_t);
typedef struct {
    uint32_t address, size;
    char label[17];
} esp_partition_t;
#define ESP_PARTITION_TYPE_APP 0
#define ESP_PARTITION_TYPE_DATA 1
#define ESP_PARTITION_SUBTYPE_APP_OTA_0 0x10
#define ESP_PARTITION_SUBTYPE_APP_OTA_1 0x11
#define ESP_PARTITION_SUBTYPE_DATA_OTA 0
#define ESP_PARTITION_SUBTYPE_DATA_NVS 2
typedef unsigned esp_ota_handle_t;
typedef enum {
    ESP_OTA_IMG_NEW,
    ESP_OTA_IMG_PENDING_VERIFY,
    ESP_OTA_IMG_VALID,
    ESP_OTA_IMG_INVALID,
    ESP_OTA_IMG_ABORTED,
    ESP_OTA_IMG_UNDEFINED
} esp_ota_img_states_t;
typedef struct {
    uint8_t bytes[24];
} esp_image_header_t;
typedef struct {
    uint8_t header[16];
    char version[32], project_name[32];
    uint8_t rest[176];
} esp_app_desc_t;
const esp_partition_t *esp_ota_get_running_partition(void),
    *esp_ota_get_next_update_partition(const esp_partition_t *),
    *esp_partition_find_first(int, int, const char *);
esp_err_t esp_ota_get_state_partition(const esp_partition_t *, esp_ota_img_states_t *);
bool esp_ota_check_rollback_is_possible(void);
esp_err_t esp_flash_get_size(void *, uint32_t *), esp_ota_mark_app_valid_cancel_rollback(void),
    esp_ota_mark_app_invalid_rollback_and_reboot(void),
    esp_ota_begin(const esp_partition_t *, size_t, esp_ota_handle_t *),
    esp_ota_write(esp_ota_handle_t, const void *, size_t), esp_ota_end(esp_ota_handle_t),
    esp_ota_abort(esp_ota_handle_t), esp_partition_get_sha256(const esp_partition_t *, uint8_t *),
    esp_ota_set_boot_partition(const esp_partition_t *),
    esp_partition_read(const esp_partition_t *, size_t, void *, size_t);
void esp_restart(void);
typedef struct {
    const char *server;
    void (*sync_cb)(struct timeval *);
} esp_sntp_config_t;
#define ESP_NETIF_SNTP_DEFAULT_CONFIG(s) {.server = (s)}
esp_err_t esp_netif_sntp_init(const esp_sntp_config_t *);
int host_settimeofday(const struct timeval *, const void *);
#define settimeofday host_settimeofday
void host_reset(void);
void host_tick(unsigned);
void host_fail_save(bool);
void host_fail_task(bool);
SemaphoreHandle_t xSemaphoreCreateRecursiveMutex(void);
int xSemaphoreTakeRecursive(SemaphoreHandle_t,unsigned),xSemaphoreGiveRecursive(SemaphoreHandle_t);
const char *esp_err_to_name(esp_err_t);
