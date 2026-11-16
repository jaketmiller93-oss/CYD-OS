#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define CYD_BUNDLE_HEADER 512u
#define CYD_BUNDLE_LIMIT (512u + 2u * 0x700000u)
typedef struct {
    char board[24], version[32];
    uint32_t size, offset;
    uint8_t sha256[32];
} cyd_bundle_image_t;
typedef struct {
    char version[32];
    cyd_bundle_image_t images[2];
} cyd_bundle_t;
bool cyd_bundle_parse(const uint8_t *, size_t, size_t, cyd_bundle_t *);
