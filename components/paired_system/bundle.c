#include "pair_bundle.h"
#include <string.h>
static uint32_t u32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static uint32_t crc(const uint8_t *p, size_t n) {
    uint32_t c = ~0u;
    while (n--) {
        c ^= *p++;
        for (unsigned i = 0; i < 8; i++)
            c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1)));
    }
    return ~c;
}
static bool string(const uint8_t *p, size_t n) {
    bool ended = false;
    if (!p[0])
        return false;
    for (size_t i = 0; i < n; i++) {
        if (!p[i])
            ended = true;
        else if (ended || p[i] < 33 || p[i] > 126)
            return false;
    }
    return ended;
}
bool cyd_bundle_parse(const uint8_t *p, size_t n, size_t total, cyd_bundle_t *out) {
    if (!p || !out || n < CYD_BUNDLE_HEADER || total > CYD_BUNDLE_LIMIT || total < 1536)
        return false;
    static const char magic[16] = "CYD-PAIR-UPDATE";
    if (memcmp(p, magic, 16) || u32(p + 16) != 1 || u32(p + 20) != 2 || u32(p + 24) != total ||
        u32(p + 28) || !string(p + 32, 32) || u32(p + 508) != crc(p, 508))
        return false;
    for (unsigned i = 320; i < 508; i++)
        if (p[i])
            return false;
    cyd_bundle_t b = {0};
    memcpy(b.version, p + 32, 32);
    uint32_t offset = 512;
    for (unsigned i = 0; i < 2; i++) {
        const uint8_t *e = p + 64 + i * 128;
        if (!string(e, 24) || !string(e + 24, 32))
            return false;
        cyd_bundle_image_t *im = &b.images[i];
        memcpy(im->board, e, 24);
        memcpy(im->version, e + 24, 32);
        im->size = u32(e + 56);
        im->offset = u32(e + 60);
        memcpy(im->sha256, e + 64, 32);
        for (unsigned j = 96; j < 128; j++)
            if (e[j])
                return false;
        if (strcmp(im->board, i == 0 ? "crowpanel_s3" : "nm_cyd_c5") ||
            strcmp(im->version, b.version) || im->size < 512 || im->size > 0x700000 ||
            im->offset != offset)
            return false;
        if (im->size > total - offset)
            return false;
        offset += im->size;
    }
    if (offset != total)
        return false;
    *out = b;
    return true;
}
