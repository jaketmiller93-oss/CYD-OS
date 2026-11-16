#include "cyd_ota.h"
#include "sdkconfig.h"
#include "cyd_layout.h"
#ifdef CONFIG_IDF_TARGET_ESP32C5
#define BOARD_PROFILE "nm_cyd_c5"
#else
#define BOARD_PROFILE "crowpanel_s3"
#endif
#ifdef CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
#define ROLLBACK_FEATURE CYD_OTA_FEATURE_ROLLBACK
#else
#define ROLLBACK_FEATURE 0
#endif
#ifdef CONFIG_CYD_BOARD_CONFIRMED
#define BOARD_FEATURE CYD_OTA_FEATURE_BOARD_CONFIRMED
#else
#define BOARD_FEATURE 0
#endif
#ifdef CONFIG_CYD_PAIRED_SYSTEM
#define PAIRED_FEATURE CYD_OTA_FEATURE_PAIRED
#ifdef CONFIG_IDF_TARGET_ESP32S3
#define PAIRED_ROLE 1
#else
#define PAIRED_ROLE 2
#endif
#else
#define PAIRED_FEATURE 0
#endif
const cyd_os_image_desc_t cyd_os_image_descriptor
    __attribute__((used, section(".rodata_custom_desc"), aligned(4))) = {
        .magic = "CYD-OS-UPDATE",
        .format = CYD_OTA_PROTOCOL,
        .board = BOARD_PROFILE,
        .features = ROLLBACK_FEATURE | BOARD_FEATURE | CYD_OTA_FEATURE_PERSISTENT_ACCESS | PAIRED_FEATURE,
        .wifi_schema = 1,
        .access_schema = 1,
        .min_ota_protocol = 1,
        .catalog_schema = 1,
        .layout_sha256 = CYD_LAYOUT_SHA256,
#ifdef CONFIG_CYD_PAIRED_SYSTEM
        .reserved = {1, PAIRED_ROLE},
#endif
    };
const cyd_os_image_desc_t *cyd_ota_image_descriptor(void) {
    return &cyd_os_image_descriptor;
}
