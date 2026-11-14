/* Strict stored-ZIP subset: no compression, encryption, descriptors, ZIP64 or links. */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#ifdef CYD_HOST_TEST
#include "zip_host.h"
#else
#include "cyd.h"
#endif
static uint16_t u16(const uint8_t *p) {
    return p[0] | ((uint16_t)p[1] << 8);
}
static uint32_t u32(const uint8_t *p) {
    return u16(p) | ((uint32_t)u16(p + 2) << 16);
}
static bool range(size_t off, size_t n, size_t end) {
    return off <= end && n <= end - off;
}
bool cyd_safe_path(const char *s) {
    if (!s || !*s || strlen(s) >= CYD_MAX_PATH || s[0] == '/')
        return false;
    size_t seg = 0;
    unsigned levels = 0;
    const char *start = s;
    for (const char *p = s;; p++) {
        char c = *p;
        if (c == '/' || !c) {
            if (!seg || seg > 64 || start[0] == '.' || p[-1] == '.')
                return false;
            if (!c)
                break;
            if (++levels > 4)
                return false;
            seg = 0;
            start = p + 1;
        } else {
            if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-' ||
                  c == '.'))
                return false;
            seg++;
        }
    }
    return true;
}
bool cyd_safe_id(const char *s) {
    if (!s || strlen(s) > 64 || strlen(s) < 3 || !strchr(s, '.') || !cyd_safe_path(s))
        return false;
    for (const char *p = s; *p; p++)
        if (*p == '/' || *p == '_')
            return false;
    return true;
}
uint32_t cyd_crc32(const uint8_t *p, size_t n) {
    uint32_t c = ~0u;
    while (n--) {
        c ^= *p++;
        for (int i = 0; i < 8; i++)
            c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1)));
    }
    return ~c;
}
int cyd_zip_parse(const uint8_t *b, size_t n, cyd_zip_t *z) {
    if (!b || !z || n < 22 || n > CYD_MAX_PACKAGE)
        return -1;
    memset(z, 0, sizeof(*z));
    const uint8_t *e = b + n - 22;
    if (u32(e) != 0x06054b50 || u16(e + 4) || u16(e + 6) || u16(e + 20))
        return -1;
    unsigned count = u16(e + 10);
    size_t cd = u32(e + 16), cds = u32(e + 12), local = 0;
    if (!count || count > CYD_MAX_FILES || u16(e + 8) != count || !range(cd, cds, n - 22) ||
        cd + cds != n - 22)
        return -1;
    size_t pos = cd;
    for (unsigned i = 0; i < count; i++) {
        if (!range(pos, 46, n - 22))
            return -1;
        const uint8_t *c = b + pos;
        size_t ns = u16(c + 28), ex = u16(c + 30), comment = u16(c + 32), size = u32(c + 24);
        unsigned mode = u32(c + 38) >> 16;
        if (u32(c) != 0x02014b50 || u16(c + 8) || u16(c + 10) || u16(c + 34) ||
            u32(c + 20) != size || size > CYD_MAX_SCRIPT || !ns || ns >= CYD_MAX_PATH ||
            !range(pos + 46, ns + ex + comment, n - 22) || u32(c + 42) != local ||
            (mode && ((mode & 0170000) != 0100000)) || (u32(c + 38) & 0x10))
            return -1;
        cyd_zip_file_t *f = &z->files[i];
        memcpy(f->name, c + 46, ns);
        f->name[ns] = 0;
        if (memchr(c + 46, 0, ns) || !cyd_safe_path(f->name))
            return -1;
        for (unsigned j = 0; j < i; j++) {
            const char *a = z->files[j].name;
            size_t al = strlen(a);
            if (!strcmp(a, f->name) || (!strncmp(a, f->name, al) && f->name[al] == '/') ||
                (!strncmp(a, f->name, ns) && a[ns] == '/'))
                return -1;
        }
        if (!range(local, 30, cd))
            return -1;
        const uint8_t *l = b + local;
        size_t lex = u16(l + 28);
        if (u32(l) != 0x04034b50 || u16(l + 6) || u16(l + 8) || u32(l + 14) != u32(c + 16) ||
            u32(l + 18) != size || u32(l + 22) != size || u16(l + 26) != ns ||
            !range(local + 30, ns + lex, cd) || memcmp(l + 30, f->name, ns))
            return -1;
        size_t data = local + 30 + ns + lex;
        if (!range(data, size, cd) || cyd_crc32(b + data, size) != u32(c + 16))
            return -1;
        f->data = b + data;
        f->size = size;
        local = data + size;
        pos += 46 + ns + ex + comment;
    }
    if (pos != n - 22 || local != cd)
        return -1;
    z->count = count;
    return 0;
}
