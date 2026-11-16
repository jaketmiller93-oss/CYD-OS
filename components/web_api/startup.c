#include "cyd.h"
#include "cyd_access.h"
#include "startup.h"
#include <stdio.h>

static httpd_handle_t server;
static bool registered;

esp_err_t cyd_web_prepare(void) {
    if (!cyd_access_ready())
        return ESP_ERR_INVALID_ARG;
    if (server)
        return ESP_OK;
    httpd_config_t c = HTTPD_DEFAULT_CONFIG();
    c.stack_size = 16384;
    c.max_uri_handlers = 28;
    c.recv_wait_timeout = 10;
    c.send_wait_timeout = 10;
    c.lru_purge_enable = true;
    httpd_handle_t candidate = NULL;
    esp_err_t e = httpd_start(&candidate, &c);
    if (!e)
        server = candidate;
    return e;
}

esp_err_t cyd_web_register(const httpd_uri_t *routes, size_t count) {
    if (!routes || !count)
        return ESP_ERR_INVALID_ARG;
    if (registered)
        return ESP_ERR_INVALID_STATE;
    esp_err_t e = cyd_web_prepare();
    if (e)
        return e;
    for (size_t i = 0; i < count; i++) {
        e = httpd_register_uri_handler(server, &routes[i]);
        if (e) {
            char detail[192];
            snprintf(detail, sizeof(detail), "Route %s failed: %s (0x%x)",
                     routes[i].uri, esp_err_to_name(e), (unsigned)e);
            cyd_log("web", detail);
            httpd_stop(server);
            server = NULL;
            return e;
        }
    }
    registered = true;
    return ESP_OK;
}
