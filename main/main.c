#include "cyd.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "cyd_access.h"
#include "cyd_ota.h"
#include "cyd_device.h"
#include "cyd_peer.h"
#include "esp_heap_caps.h"
#ifdef ESP_PLATFORM
#include "sdkconfig.h"
#endif
#ifdef CONFIG_CYD_PAIRED_SYSTEM
#include "cyd_pair.h"
#endif
#include <stdio.h>
void app_main(void) {
#ifdef CONFIG_CYD_PAIRED_SYSTEM
    cyd_pair_memory_init(); /* JSON is not DMA; reserve internal RAM for OS/radio stacks. */
#endif
    ESP_LOGI("boot", "CYD OS %s / %s", cyd_system_version(), cyd_board_model());
    esp_err_t ota = cyd_ota_init();
    if (ota)
        ESP_LOGE("boot", "OTA initialization failed: %s", esp_err_to_name(ota));
    esp_err_t e = nvs_flash_init();
    /* Never erase persistent credentials as an automatic response to an update. */
    if (e) {
        ESP_LOGE("boot", "NVS unavailable; settings retained for recovery");
        return;
    }
    if (cyd_access_init()) {
        ESP_LOGE("boot", "Device access settings unavailable");
        return;
    }
    if (cyd_runtime_init() || cyd_manager_init()) {
        ESP_LOGE("boot", "Core initialization failed");
        return;
    }
    cyd_ota_health_report(CYD_HEALTH_CORE);
    ESP_LOGI("boot", "Initializing board/display/touch");
    e = cyd_board_init();
    if (e) {
        ESP_LOGE("boot", "Board initialization failed: %s", esp_err_to_name(e));
        return;
    }
    ESP_LOGI("boot", "Loading saved device settings");
    if (cyd_device_init(cyd_board_width(), cyd_board_height(), cyd_board_set_brightness,
                        cyd_ui_apply_rotation)) {
        ESP_LOGE("boot", "Device settings unavailable");
        return;
    }
    cyd_ota_health_report(CYD_HEALTH_BOARD);
    ESP_LOGI("boot", "Mounting storage");
    e = cyd_storage_init();
    if (e)
        ESP_LOGW("boot", "SD unavailable; shell remains usable");
    ESP_LOGI("boot", "Initializing network");
    e = cyd_network_init();
    if (!e)
#if !defined(CONFIG_CYD_PAIRED_SYSTEM) || !defined(CONFIG_IDF_TARGET_ESP32S3)
        cyd_device_clock_start();
#else
        cyd_log("boot", "C5 owns network and wall-clock settings");
#endif
#if defined(CONFIG_CYD_PAIRED_SYSTEM) && defined(CONFIG_IDF_TARGET_ESP32C5) && \
    !defined(CONFIG_IDF_TARGET_ESP32S3)
    /* Reserve HTTPD's contiguous internal stack before LVGL buffers/tasks can
     * fragment it. No routes exist until the UI has initialized below. */
    esp_err_t web_prepared = e ? ESP_OK : cyd_web_prepare();
#endif
    ESP_LOGI("boot", "Starting UI");
    if (cyd_ui_init()) {
        ESP_LOGE("boot", "UI initialization failed");
        return;
    }
    if (e) {
        ESP_LOGE("boot", "Connection service unavailable");
        return;
    }
    ESP_LOGI("boot", "Starting access service");
#if defined(CONFIG_CYD_PAIRED_SYSTEM) && defined(CONFIG_IDF_TARGET_ESP32S3)
    e = ESP_OK; /* Radio stays off; C5 gateways to the S3 service coordinator. */
#elif defined(CONFIG_CYD_PAIRED_SYSTEM) && defined(CONFIG_IDF_TARGET_ESP32C5)
    e = web_prepared ? web_prepared : cyd_web_start();
#else
    e = cyd_web_start();
#endif
    if (e) {
        char detail[160];
        snprintf(detail, sizeof(detail), "Web API initialization failed: %s (0x%x); internal largest=%u B",
                 esp_err_to_name(e), (unsigned)e,
                 (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
        ESP_LOGE("boot", "%s", detail);
        cyd_log("boot", detail);
        return;
    }
#ifndef CONFIG_CYD_PAIRED_SYSTEM
    cyd_ota_health_report(CYD_HEALTH_API);
#endif
    /* Required services reserve internal stacks before the optional UART task. */
    e = cyd_peer_start();
    if (e) cyd_log("peer", "Link unavailable; local OS remains available");
#ifdef CONFIG_CYD_PAIRED_SYSTEM
    const char *stage = "Paired UART";
    if (!e) {
        stage = "Paired service";
        e = cyd_pair_start();
    }
    if (e) {
        char detail[192];
        snprintf(detail, sizeof(detail),
                 "%s initialization failed: %s (0x%x); internal largest=%u B",
                 stage, esp_err_to_name(e), (unsigned)e,
                 (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
        ESP_LOGE("boot", "%s", detail);
        cyd_log("boot", detail);
        return;
    }
    cyd_ota_health_report(CYD_HEALTH_API);
#endif
    ESP_LOGI("boot", "Local OS startup complete; peer is optional");
}
