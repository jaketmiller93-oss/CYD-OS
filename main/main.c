#include "cyd.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "cyd_access.h"
#include "cyd_ota.h"
#include "cyd_device.h"
#include "cyd_peer.h"
void app_main(void) {
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
    e = cyd_board_init();
    if (e) {
        ESP_LOGE("boot", "Board initialization failed: %s", esp_err_to_name(e));
        return;
    }
    if (cyd_device_init(cyd_board_width(), cyd_board_height(), cyd_board_set_brightness,
                        cyd_ui_apply_rotation)) {
        ESP_LOGE("boot", "Device settings unavailable");
        return;
    }
    cyd_ota_health_report(CYD_HEALTH_BOARD);
    e = cyd_storage_init();
    if (e)
        ESP_LOGW("boot", "SD unavailable; shell remains usable");
    e = cyd_network_init();
    if (!e)
        cyd_device_clock_start();
    if (cyd_ui_init()) {
        ESP_LOGE("boot", "UI initialization failed");
        return;
    }
    if (e) {
        ESP_LOGE("boot", "Connection service unavailable");
        return;
    }
    if (cyd_peer_start())
        cyd_log("peer", "Diagnostic link not started; standalone OS remains available");
    e = cyd_web_start();
    if (e) {
        ESP_LOGE("boot", "Web API initialization failed");
        return;
    }
    cyd_ota_health_report(CYD_HEALTH_API);
}
