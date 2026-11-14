#pragma once
#include <stdbool.h>
#include <stdint.h>
void cyd_display_to_native(unsigned rotation, int native_width, int native_height, int x, int y,
                           int *nx, int *ny);
void cyd_display_to_logical(unsigned rotation, int native_width, int native_height, int x, int y,
                            int *lx, int *ly);
void cyd_rotate_rgb565(const uint16_t *src, uint16_t *dst, unsigned width, unsigned height,
                       unsigned rotation);
bool cyd_touch_fit(const float raw[5][2], const float target[5][2], float matrix[6]);
bool cyd_clock_parse_local(const char *text, int offset_minutes, int64_t *epoch);
