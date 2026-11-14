#include "display_geometry.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
void cyd_display_to_native(unsigned r, int w, int h, int x, int y, int *nx, int *ny) {
    switch (r & 3) {
    case 1:
        *nx = y;
        *ny = h - 1 - x;
        break;
    case 2:
        *nx = w - 1 - x;
        *ny = h - 1 - y;
        break;
    case 3:
        *nx = w - 1 - y;
        *ny = x;
        break;
    default:
        *nx = x;
        *ny = y;
    }
}
void cyd_display_to_logical(unsigned r, int w, int h, int x, int y, int *lx, int *ly) {
    switch (r & 3) {
    case 1:
        *lx = h - 1 - y;
        *ly = x;
        break;
    case 2:
        *lx = w - 1 - x;
        *ly = h - 1 - y;
        break;
    case 3:
        *lx = y;
        *ly = w - 1 - x;
        break;
    default:
        *lx = x;
        *ly = y;
    }
}
void cyd_rotate_rgb565(const uint16_t *src, uint16_t *dst, unsigned w, unsigned h, unsigned r) {
    if (!(r & 3)) {
        memcpy(dst, src, w * h * 2);
        return;
    }
    for (unsigned y = 0; y < h; y++)
        for (unsigned x = 0; x < w; x++) {
            unsigned index = (r & 3) == 1   ? (w - 1 - x) * h + y
                             : (r & 3) == 2 ? (h - 1 - y) * w + w - 1 - x
                                            : x * h + h - 1 - y;
            dst[index] = src[y * w + x];
        }
}
bool cyd_touch_fit(const float raw[5][2], const float target[5][2], float matrix[6]) {
    double mx = 0, my = 0, tx = 0, ty = 0;
    for (int i = 0; i < 5; i++) {
        mx += raw[i][0] / 5.;
        my += raw[i][1] / 5.;
        tx += target[i][0] / 5.;
        ty += target[i][1] / 5.;
    }
    double xx = 0, xy = 0, yy = 0, xtx = 0, ytx = 0, xty = 0, yty = 0;
    for (int i = 0; i < 5; i++) {
        double x = raw[i][0] - mx, y = raw[i][1] - my;
        xx += x * x;
        xy += x * y;
        yy += y * y;
        xtx += x * (target[i][0] - tx);
        ytx += y * (target[i][0] - tx);
        xty += x * (target[i][1] - ty);
        yty += y * (target[i][1] - ty);
    }
    double det = xx * yy - xy * xy;
    if (!isfinite(det) || xx < 100 || yy < 100 || fabs(det) < xx * yy * 1e-6)
        return false;
    matrix[0] = (xtx * yy - ytx * xy) / det;
    matrix[1] = (ytx * xx - xtx * xy) / det;
    matrix[2] = tx - matrix[0] * mx - matrix[1] * my;
    matrix[3] = (xty * yy - yty * xy) / det;
    matrix[4] = (yty * xx - xty * xy) / det;
    matrix[5] = ty - matrix[3] * mx - matrix[4] * my;
    for (int i = 0; i < 6; i++)
        if (!isfinite(matrix[i]) || fabsf(matrix[i]) > 10000)
            return false;
    for (int i = 0; i < 5; i++) {
        double x = matrix[0] * raw[i][0] + matrix[1] * raw[i][1] + matrix[2] - target[i][0];
        double y = matrix[3] * raw[i][0] + matrix[4] * raw[i][1] + matrix[5] - target[i][1];
        if (x * x + y * y > 196)
            return false;
    }
    return true;
}
static bool leap(int y) {
    return y % 4 == 0 && (y % 100 != 0 || y % 400 == 0);
}
bool cyd_clock_parse_local(const char *text, int offset, int64_t *epoch) {
    if (!text || strlen(text) != 16 || offset < -720 || offset > 840)
        return false;
    for (unsigned i = 0; i < 16; i++) {
        char c = text[i];
        if (i == 4 || i == 7) {
            if (c != '-')
                return false;
        } else if (i == 10) {
            if (c != ' ')
                return false;
        } else if (i == 13) {
            if (c != ':')
                return false;
        } else if (c < '0' || c > '9')
            return false;
    }
    int y, m, d, h, n;
    char tail;
    if (sscanf(text, "%4d-%2d-%2d %2d:%2d%c", &y, &m, &d, &h, &n, &tail) != 5 || y < 2024 ||
        y > 2099 || m < 1 || m > 12 || d < 1 || h < 0 || h > 23 || n < 0 || n > 59)
        return false;
    const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (d > days[m - 1] + (m == 2 && leap(y)))
        return false;
    /* Gregorian days since 1970, without changing the process timezone. */
    int64_t count = 0;
    for (int year = 1970; year < y; year++)
        count += 365 + leap(year);
    for (int month = 1; month < m; month++)
        count += days[month - 1] + (month == 2 && leap(y));
    count += d - 1;
    *epoch = count * 86400 + h * 3600 + n * 60 - (int64_t)offset * 60;
    return true;
}
