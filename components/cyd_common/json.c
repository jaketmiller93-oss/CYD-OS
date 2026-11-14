#include "cyd.h"
#include <stdlib.h>
#include <string.h>
cJSON *cyd_json_parse(const void *data, size_t n, size_t max) {
    if (!data || !n || n > max || memchr(data, 0, n))
        return NULL;
    unsigned depth = 0;
    bool string = false, escape = false;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = ((const unsigned char *)data)[i];
        if (string) {
            if (escape)
                escape = false;
            else if (c == '\\')
                escape = true;
            else if (c == '"')
                string = false;
        } else if (c == '"')
            string = true;
        else if (c == '{' || c == '[') {
            if (++depth > 8)
                return NULL;
        } else if (c == '}' || c == ']') {
            if (!depth)
                return NULL;
            depth--;
        }
        if (i + 6 <= n && !memcmp((const char *)data + i, "\\u0000", 6))
            return NULL;
    }
    if (string || depth)
        return NULL;
    char *copy = calloc(1, n + 1);
    if (!copy)
        return NULL;
    memcpy(copy, data, n);
    cJSON *o = cJSON_ParseWithLengthOpts(copy, n + 1, NULL, true);
    free(copy);
    return o;
}
