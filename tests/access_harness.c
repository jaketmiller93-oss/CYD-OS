#include "system_sdk/cyd.h"
#include "../components/device_access/access.c"
int host_access_case(int which) {
    host_reset();
    ready = false;
    token[0] = 0;
    if (cyd_access_init())
        return 1;
    char original[241];
    cyd_access_get_token(original, sizeof(original));
    if (strcmp(original, CONFIG_CYD_API_TOKEN))
        return 2;
    char header[256];
    snprintf(header, sizeof(header), "Bearer %s", original);
    if (!cyd_access_authorized(header, strlen(header)))
        return 3;
    if (which == 0) {
        ready = false;
        token[0] = 0;
        if (cyd_access_init() || strcmp(token, original))
            return 4;
    }
    if (which == 1) {
        if (cyd_access_rotate() || !strcmp(token, original))
            return 5;
        if (cyd_access_authorized(header, strlen(header)))
            return 6;
    }
    if (which == 2) {
        host_fail_save(true);
        if (!cyd_access_rotate() || strcmp(token, original))
            return 7;
    }
    if (which == 3) {
        if (cyd_access_authorized("Bearer short", 12) ||
            cyd_access_authorized(header, strlen(header) - 1) ||
            cyd_access_authorized("bearer host-initial-token-1234567890", 34))
            return 8;
    }
    if (which == 4) {
        nvs_set_str(store, "token", "bad");
        nvs_commit(store);
        ready = false;
        if (cyd_access_init() == 0)
            return 9;
    }
    return 0;
}
