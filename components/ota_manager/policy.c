#include "ota_policy.h"
#include <stdio.h>
#include <string.h>
static uint16_t u16(const uint8_t *p) {
    return p[0] | ((uint16_t)p[1] << 8);
}
static uint32_t u32(const uint8_t *p) {
    return u16(p) | ((uint32_t)u16(p + 2) << 16);
}
static bool fail(char *out, size_t n, const char *why) {
    snprintf(out, n, "%s", why);
    return false;
}
bool cyd_ota_policy_validate(const uint8_t *p, size_t n, size_t total, uint16_t chip,
                             const char *board, const char *layout, char *error, size_t ec) {
    if (!p || n < CYD_OTA_PREFIX_BYTES || total < CYD_OTA_PREFIX_BYTES || total > CYD_OTA_SLOT_SIZE)
        return fail(error, ec, "Image size is outside the OTA slot limits");
    if (p[0] != 0xe9 || !p[1] || p[1] > 16 || p[23] != 1 || ((p[3] >> 4) != 4))
        return fail(error, ec, "Expected a 16 MB ESP application image with an appended SHA-256");
    if (u16(p + 12) != chip)
        return fail(error, ec, "Firmware is for a different ESP32 chip");
    if (u32(p + 28) < 256 + sizeof(cyd_os_image_desc_t) || u32(p + 32) != 0xabcd5432 ||
        strncmp((const char *)p + 80, "cyd-os", 32))
        return fail(error, ec,
                    "Expected a CYD OS application image, not a merged image or bootloader");
    cyd_os_image_desc_t d;
    memcpy(&d, p + CYD_OTA_DESC_OFFSET, sizeof(d));
    if (memcmp(d.magic, "CYD-OS-UPDATE", 14) || d.format != CYD_OTA_PROTOCOL ||
        d.min_ota_protocol > CYD_OTA_PROTOCOL)
        return fail(error, ec, "Firmware has no supported CYD OS update descriptor");
    if (!memchr(d.board, 0, sizeof(d.board)) || strcmp(d.board, board))
        return fail(error, ec, "Firmware board profile does not match this device");
    if ((d.features & CYD_OTA_REQUIRED_FEATURES) != CYD_OTA_REQUIRED_FEATURES ||
        d.wifi_schema != 1 || d.access_schema != 1 || d.catalog_schema != 1)
        return fail(error, ec,
                    "Firmware must preserve rollback, device credentials and supported settings");
    if (!memchr(d.layout_sha256, 0, sizeof(d.layout_sha256)) || strlen(d.layout_sha256) != 64 ||
        strcmp(d.layout_sha256, layout))
        return fail(error, ec, "Firmware partition layout does not match the installed OS");
    if (!memchr(p + 48, 0, 32))
        return fail(error, ec, "Invalid application version descriptor");
    if (ec)
        error[0] = 0;
    return true;
}
