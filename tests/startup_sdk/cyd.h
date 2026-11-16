#pragma once
#include <stdbool.h>
#include <stddef.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_ERR_NO_MEM 0x101
#define CYD_HEALTH_CORE 1u
#define CYD_HEALTH_BOARD 2u
#define CYD_HEALTH_API 4u
#define MALLOC_CAP_INTERNAL 1
#define MALLOC_CAP_8BIT 4
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
const char *cyd_system_version(void), *cyd_board_model(void), *esp_err_to_name(int);
esp_err_t cyd_ota_init(void), nvs_flash_init(void), cyd_access_init(void);
esp_err_t cyd_runtime_init(void), cyd_manager_init(void), cyd_board_init(void);
esp_err_t cyd_storage_init(void), cyd_network_init(void), cyd_ui_init(void);
esp_err_t cyd_web_start(void), cyd_web_prepare(void), cyd_peer_start(void);
unsigned cyd_board_width(void), cyd_board_height(void);
esp_err_t cyd_board_set_brightness(unsigned), cyd_ui_apply_rotation(unsigned);
esp_err_t cyd_device_init(unsigned, unsigned, esp_err_t (*)(unsigned), esp_err_t (*)(unsigned));
void cyd_device_clock_start(void), cyd_ota_health_report(unsigned);
void cyd_log(const char *, const char *);
size_t heap_caps_get_largest_free_block(unsigned);
