#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define CYD_OTA_PROTOCOL 1
#define CYD_OTA_DESC_OFFSET 288
#define CYD_OTA_PREFIX_BYTES 512
#define CYD_OTA_SLOT_SIZE 0x700000u
#define CYD_OTA_FEATURE_ROLLBACK 1u
#define CYD_OTA_FEATURE_PERSISTENT_ACCESS 2u
#define CYD_OTA_FEATURE_BOARD_CONFIRMED 4u
#define CYD_OTA_REQUIRED_FEATURES 7u
#define CYD_OTA_FEATURE_PAIRED 8u
typedef struct {
    char magic[16];
    uint32_t format;
    char board[24];
    uint32_t features;
    uint32_t wifi_schema, access_schema, min_ota_protocol, catalog_schema;
    char layout_sha256[65];
    uint8_t reserved[63];
} cyd_os_image_desc_t;
_Static_assert(sizeof(cyd_os_image_desc_t) == 192, "OTA descriptor ABI");
bool cyd_ota_policy_validate(const uint8_t *prefix, size_t prefix_size, size_t image_size,
                             uint16_t chip_id, const char *board, const char *layout_sha256,
                             char *error, size_t error_size);
