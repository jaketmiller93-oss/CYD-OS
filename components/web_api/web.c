#include "cyd.h"
#include "cyd_peer.h"
#include "cyd_access.h"
#include "cyd_device.h"
#include "cyd_network.h"
#include "cyd_ota.h"
#include "cyd_projects.h"
#include "esp_heap_caps.h"
#include "esp_http_server.h"
#include "startup.h"
#include "esp_psram.h"
#include "sdkconfig.h"
#ifdef CONFIG_CYD_PAIRED_SYSTEM
#include "cyd_pair.h"
#else
#define CYD_PAIR_MANAGER 0
#endif
#if CYD_PAIR_MANAGER
#include "pair_ops.h"
#include "pair_bundle.h"
#endif
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern const char dashboard_start[] asm("_binary_dashboard_html_start");
static esp_err_t json(httpd_req_t *r, cJSON *o) {
    if (!o)
        return httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "Allocation failed");
    char *s = cJSON_PrintUnformatted(o);
    cJSON_Delete(o);
    if (!s)
        return ESP_ERR_NO_MEM;
    httpd_resp_set_type(r, "application/json");
    httpd_resp_set_hdr(r, "Cache-Control", "no-store");
    esp_err_t e = httpd_resp_sendstr(r, s);
    free(s);
    return e;
}
static bool auth(httpd_req_t *r) {
    char got[256];
    size_t len = httpd_req_get_hdr_value_len(r, "Authorization");
    if (len < sizeof(got) &&
        httpd_req_get_hdr_value_str(r, "Authorization", got, sizeof(got)) == ESP_OK &&
        cyd_access_authorized(got, len))
        return true;
    httpd_resp_set_status(r, "401 Unauthorized");
    httpd_resp_set_hdr(r, "WWW-Authenticate", "Bearer");
    httpd_resp_sendstr(r, "{\"error\":\"Authentication required\"}");
    return false;
}
static esp_err_t failure(httpd_req_t *r, esp_err_t e, const char *msg) {
    httpd_resp_set_status(r, e == ESP_ERR_NOT_FOUND       ? "404 Not Found"
                             : e == ESP_ERR_TIMEOUT       ? "503 Service Unavailable"
                             : e == ESP_ERR_INVALID_STATE ? "409 Conflict"
                                                          : "400 Bad Request");
    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "error", msg);
    return json(r, o);
}
static bool revision(httpd_req_t *r, char *out) {
    char q[128];
    if (httpd_req_get_url_query_str(r, q, sizeof(q)) ||
        httpd_query_key_value(q, "revision", out, 17))
        return false;
    if (strlen(out) != 16)
        return false;
    for (int i = 0; i < 16; i++)
        if (!((out[i] >= '0' && out[i] <= '9') || (out[i] >= 'a' && out[i] <= 'f')))
            return false;
    return true;
}
static esp_err_t index_page(httpd_req_t *r) {
    httpd_resp_set_type(r, "text/html");
    httpd_resp_set_hdr(r, "X-Content-Type-Options", "nosniff");
    httpd_resp_set_hdr(r, "Content-Security-Policy",
                       "default-src 'none'; script-src 'unsafe-inline'; style-src "
                       "'unsafe-inline'; connect-src "
                       "'self'; frame-ancestors 'none'; base-uri 'none'; form-action 'none'");
    return httpd_resp_sendstr(r, dashboard_start);
}
static esp_err_t status(httpd_req_t *r) {
    if (!auth(r))
        return ESP_OK;
    char ip[32];
    cyd_network_address(ip, sizeof(ip));
    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "model", cyd_board_model());
    cJSON_AddStringToObject(o, "firmware", cyd_system_version());
    cJSON_AddStringToObject(o, "target", CONFIG_IDF_TARGET);
    cJSON_AddStringToObject(o, "ip", ip);
    cJSON_AddBoolToObject(o, "wifi", cyd_network_connected());
    cJSON_AddBoolToObject(o, "sd", cyd_storage_ready());
    cJSON_AddNumberToObject(o, "heap_free", esp_get_free_heap_size());
    cJSON_AddNumberToObject(o, "largest_block", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    cJSON_AddNumberToObject(o, "psram_total", esp_psram_get_size());
    cJSON_AddNumberToObject(o, "psram_free", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    cJSON_AddNumberToObject(o, "internal_heap_free", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    cJSON_AddNumberToObject(o, "internal_largest_block",
                          heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    cJSON_AddNumberToObject(o, "storage_free", cyd_storage_free());
    cJSON_AddItemToObject(o, "app", cyd_runtime_status());
    cJSON_AddItemToObject(o, "peer_link", cyd_peer_status());
#if CYD_PAIR_MANAGER
    o=cyd_pair_dashboard_merge(o,cyd_pair_json(CYD_PAIR_STATUS,NULL),cyd_pair_status());
    if(!o)return json(r,NULL);
#endif
    cyd_net_status_t ns;
    cyd_network_snapshot(&ns);
    cJSON *net = cJSON_CreateObject();
    cJSON_AddBoolToObject(net, "ready", ns.ready);
    cJSON_AddBoolToObject(net, "scanning", ns.scanning);
    cJSON_AddStringToObject(net, "state",
                            ns.state == CYD_NET_CONNECTED    ? "connected"
                            : ns.state == CYD_NET_CONNECTING ? "connecting"
                            : ns.state == CYD_NET_FAILED     ? "failed"
                                                             : "offline");
    cJSON_AddStringToObject(net, "station_ssid", ns.ssid);
    cJSON_AddStringToObject(net, "station_ip", ns.station_ip);
    cJSON_AddBoolToObject(net, "ap_enabled", ns.ap_enabled);
    cJSON_AddStringToObject(net, "ap_ssid", ns.ap_ssid);
    cJSON_AddStringToObject(net, "ap_ip", ns.ap_ip);
    cJSON_AddStringToObject(net, "message", ns.message);
    cJSON_AddNumberToObject(net, "saved_count", ns.saved_count);
    cJSON_AddItemToObject(o, "network", net);
    return json(r, o);
}
static esp_err_t versions(httpd_req_t *r) {
    if (!auth(r))
        return ESP_OK;
    return json(r, cyd_versions());
}
static esp_err_t logs_handler(httpd_req_t *r) {
    if (!auth(r))
        return ESP_OK;
#if CYD_PAIR_MANAGER
    return json(r,cyd_pair_logs_merge(cyd_logs(),cyd_pair_json(CYD_PAIR_LOGS,NULL),32));
#else
    return json(r, cyd_logs());
#endif
}
static esp_err_t package_request(httpd_req_t *r, bool inspect) {
    if (!auth(r))
        return ESP_OK;
    if (!inspect && !cyd_storage_ready())
        return failure(r, ESP_ERR_INVALID_STATE, "SD card unavailable");
    if (r->content_len < 22 || r->content_len > CYD_MAX_PACKAGE)
        return failure(r, ESP_ERR_INVALID_SIZE, "Package limit 512 KiB");
    uint8_t *b = malloc(r->content_len);
    if (!b)
        return failure(r, ESP_ERR_NO_MEM, "Allocation failed");
    size_t offset = 0;
    while (offset < r->content_len) {
        int n = httpd_req_recv(r, (char *)b + offset, r->content_len - offset);
        if (n <= 0) {
            free(b);
            return failure(r, ESP_ERR_TIMEOUT, "Upload interrupted");
        }
        offset += n;
    }
    char rev[17], error[128];
    if (inspect) {
        cJSON *meta = cyd_package_inspect(b, offset, error, sizeof(error));
        free(b);
        if (!meta)
            return failure(r, ESP_ERR_INVALID_ARG, error);
        return json(r, meta);
    }
    esp_err_t e = cyd_install(b, offset, rev, sizeof(rev), error, sizeof(error));
    free(b);
    if (e)
        return failure(r, e, error);
    httpd_resp_set_status(r, "201 Created");
    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "revision", rev);
    cJSON_AddStringToObject(o, "status", "installed");
    return json(r, o);
}
static esp_err_t upload(httpd_req_t *r) { return package_request(r, false); }
static esp_err_t inspect_package(httpd_req_t *r) { return package_request(r, true); }
static esp_err_t run(httpd_req_t *r) {
    if (!auth(r))
        return ESP_OK;
    char rev[17];
    if (!revision(r, rev))
        return failure(r, ESP_ERR_INVALID_ARG, "Expected revision");
    esp_err_t e = cyd_run_revision(rev);
    if (e)
        return failure(r, e, esp_err_to_name(e));
    httpd_resp_set_status(r, "202 Accepted");
    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "status", "starting");
    return json(r, o);
}
static esp_err_t stop(httpd_req_t *r) {
    if (!auth(r))
        return ESP_OK;
    esp_err_t e = cyd_runtime_stop();
    if (e)
        return failure(r, e, "App has not stopped");
    return json(r, cyd_runtime_status());
}
static esp_err_t remove_version(httpd_req_t *r) {
    if (!auth(r))
        return ESP_OK;
    char rev[17];
    if (!revision(r, rev))
        return failure(r, ESP_ERR_INVALID_ARG, "Expected revision");
    esp_err_t e = cyd_delete_revision(rev);
    if (e)
        return failure(r, e, esp_err_to_name(e));
    cJSON *o = cJSON_CreateObject();
    cJSON_AddBoolToObject(o, "deleted", true);
    return json(r, o);
}
static esp_err_t download(httpd_req_t *r) {
    if (!auth(r))
        return ESP_OK;
    char rev[17];
    if (!revision(r, rev))
        return failure(r, ESP_ERR_INVALID_ARG, "Expected revision");
    char path[180];
    snprintf(path, sizeof(path), CYD_ROOT "/projects/%s/package.cydapp", rev);
    FILE *f = fopen(path, "rb");
    if (!f)
        return failure(r, ESP_ERR_NOT_FOUND, "Revision not found");
    httpd_resp_set_type(r, "application/zip");
    httpd_resp_set_hdr(r, "Content-Disposition", "attachment; filename=application.cydapp");
    char b[2048];
    size_t n;
    esp_err_t e = ESP_OK;
    while ((n = fread(b, 1, sizeof(b), f)) && e == ESP_OK)
        e = httpd_resp_send_chunk(r, b, n);
    bool failed = ferror(f);
    fclose(f);
    if (e || failed)
        return ESP_FAIL;
    return httpd_resp_send_chunk(r, NULL, 0);
}
static cJSON *os_json(void) {
    cyd_ota_status_t s;
    cyd_ota_snapshot(&s);
    const char *names[] = {"booting", "idle",      "uploading", "verifying",
                           "ready",   "rebooting", "failed"};
    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "state", names[s.stage]);
    cJSON_AddBoolToObject(o, "boot_confirmed", s.boot_confirmed);
    cJSON_AddBoolToObject(o, "pending_verify", s.pending_verify);
    cJSON_AddBoolToObject(o, "rollback_available", s.rollback_available);
    cJSON_AddStringToObject(o, "running_slot", s.running_slot);
    cJSON_AddStringToObject(o, "target_slot", s.target_slot);
    cJSON_AddStringToObject(o, "version", s.version);
    cJSON_AddStringToObject(o, "candidate_version", s.candidate_version);
    cJSON_AddStringToObject(o, "image_sha256", s.sha256);
    cJSON_AddStringToObject(o, "message", s.message);
    cJSON_AddNumberToObject(o, "received", s.received);
    cJSON_AddNumberToObject(o, "total", s.total);
    cJSON_AddStringToObject(o, "board", cyd_ota_image_descriptor()->board);
    cJSON_AddNumberToObject(o, "max_image_bytes", CYD_OTA_SLOT_SIZE);
    return o;
}
static esp_err_t os_status(httpd_req_t *r) {
    if (!auth(r))
        return ESP_OK;
    return json(r, os_json());
}
static esp_err_t os_upload_error(httpd_req_t *r, esp_err_t e, const char *why) {
    httpd_resp_set_hdr(r, "Connection", "close");
    failure(r, e, why);
    return ESP_FAIL;
}
static esp_err_t os_upload(httpd_req_t *r) {
    if (!auth(r))
        return ESP_FAIL;
    if (r->content_len < CYD_OTA_PREFIX_BYTES || r->content_len > CYD_OTA_SLOT_SIZE)
        return os_upload_error(r, ESP_ERR_INVALID_SIZE,
                               "Choose a CYD OS application .bin, at most 7 MiB");
    uint8_t prefix[CYD_OTA_PREFIX_BYTES];
    size_t got = 0;
    while (got < sizeof(prefix)) {
        int n = httpd_req_recv(r, (char *)prefix + got, sizeof(prefix) - got);
        if (n <= 0)
            return os_upload_error(r, ESP_ERR_TIMEOUT,
                                   "Firmware upload interrupted before validation");
        got += n;
    }
    char error[160];
    esp_err_t e = cyd_ota_begin(prefix, sizeof(prefix), r->content_len, error, sizeof(error));
    if (e)
        return os_upload_error(r, e, error);
    e = cyd_ota_write(prefix, sizeof(prefix));
    if (e)
        return os_upload_error(r, e, "Could not write the image header");
    uint8_t buffer[4096];
    while (got < r->content_len) {
        size_t chunk = r->content_len - got;
        if (chunk > sizeof(buffer))
            chunk = sizeof(buffer);
        int n = httpd_req_recv(r, (char *)buffer, chunk);
        if (n <= 0) {
            cyd_ota_abort("Firmware transfer interrupted; current OS retained");
            return os_upload_error(r, ESP_ERR_TIMEOUT,
                                   "Firmware transfer interrupted; current OS retained");
        }
        e = cyd_ota_write(buffer, n);
        if (e)
            return os_upload_error(r, e, "Firmware write failed; current OS retained");
        got += n;
    }
    e = cyd_ota_finish(error, sizeof(error));
    if (e)
        return os_upload_error(r, e, error);
    httpd_resp_set_status(r, "201 Created");
    return json(r, os_json());
}
static esp_err_t os_reboot(httpd_req_t *r) {
    if (!auth(r))
        return ESP_OK;
    esp_err_t e = cyd_ota_reboot();
    if (e)
        return failure(r, e, "No verified staged update is ready to restart");
    httpd_resp_set_status(r, "202 Accepted");
    return json(r, os_json());
}
static esp_err_t os_cancel(httpd_req_t *r) {
    if (!auth(r))
        return ESP_OK;
    esp_err_t e = cyd_ota_cancel();
    if (e)
        return failure(r, e, "No staged update can be cancelled");
    return json(r, os_json());
}
static esp_err_t os_rollback(httpd_req_t *r) {
    if (!auth(r))
        return ESP_OK;
    esp_err_t e = cyd_ota_rollback();
    if (e)
        return failure(r, e, "No compatible confirmed previous OS is available");
    httpd_resp_set_status(r, "202 Accepted");
    return json(r, os_json());
}
static cJSON *request_json(httpd_req_t *r) {
    if (r->content_len < 2 || r->content_len > 768)
        return NULL;
    char b[768];
    size_t n = 0;
    while (n < r->content_len) {
        int got = httpd_req_recv(r, b + n, r->content_len - n);
        if (got <= 0)
            return NULL;
        n += got;
    }
    cJSON *o = cyd_json_parse(b, n, sizeof(b));
    if (!cJSON_IsObject(o)) {
        cJSON_Delete(o);
        return NULL;
    }
    return o;
}
static const char *string_value(cJSON *o, const char *key) {
    cJSON *v = cJSON_GetObjectItemCaseSensitive(o, key);
    return cJSON_IsString(v) ? v->valuestring : "";
}
static bool integer_value(cJSON *o, const char *key, int64_t min, int64_t max, int64_t *out) {
    cJSON *v = cJSON_GetObjectItemCaseSensitive(o, key);
    if (!cJSON_IsNumber(v) || !isfinite(v->valuedouble) || v->valuedouble < min ||
        v->valuedouble > max || floor(v->valuedouble) != v->valuedouble)
        return false;
    *out = (int64_t)v->valuedouble;
    return true;
}
static esp_err_t projects_get(httpd_req_t *r) {
    if (!auth(r))
        return ESP_OK;
    return json(r, cyd_projects_catalog());
}
static esp_err_t projects_post(httpd_req_t *r) {
    if (!auth(r))
        return ESP_FAIL;
    cJSON *o = request_json(r);
    if (!o)
        return failure(r, ESP_ERR_INVALID_ARG, "Expected a small JSON object");
    const char *action = string_value(o, "action"), *folder = string_value(o, "folder"),
               *name = string_value(o, "name");
    esp_err_t e = ESP_ERR_INVALID_ARG;
    char id[9];
    if (!strcmp(action, "create"))
        e = cyd_project_folder_create(name, id);
    else if (!strcmp(action, "rename"))
        e = cyd_project_folder_rename(folder, name);
    else if (!strcmp(action, "delete"))
        e = cyd_project_folder_delete(folder);
    else if (!strcmp(action, "move"))
        e = cyd_project_move(string_value(o, "package_id"), folder);
    cJSON_Delete(o);
    if (e)
        return failure(r, e, "Folder change failed: check name, SD card, limits and update state");
    return json(r, cyd_projects_catalog());
}
static cJSON *device_json(void) {
    cyd_device_status_t s;
    cyd_device_snapshot(&s);
    cJSON *o = cJSON_CreateObject();
    cJSON_AddNumberToObject(o, "brightness", s.brightness);
    cJSON_AddNumberToObject(o, "rotation", s.rotation);
    cJSON_AddNumberToObject(o, "utc_offset_minutes", s.utc_offset_minutes);
    cJSON_AddBoolToObject(o, "clock_valid", s.clock_valid);
    cJSON_AddStringToObject(o, "clock", s.clock);
    cJSON_AddNumberToObject(o, "display_mode", s.mode);
    cJSON_AddNumberToObject(o, "seconds_left", s.seconds_left);
    cJSON_AddStringToObject(o, "message", s.message);
    return o;
}
static esp_err_t device_get(httpd_req_t *r) {
    if (!auth(r))
        return ESP_OK;
    return json(r, device_json());
}
static esp_err_t device_post(httpd_req_t *r) {
    if (!auth(r))
        return ESP_FAIL;
    cJSON *o = request_json(r);
    if (!o)
        return failure(r, ESP_ERR_INVALID_ARG, "Expected a small JSON object");
    const char *a = string_value(o, "action");
    esp_err_t e = ESP_ERR_INVALID_ARG;
    int64_t v, offset;
    if (!strcmp(a, "brightness") && integer_value(o, "value", 10, 100, &v))
        e = cyd_device_brightness(v, true);
    else if (!strcmp(a, "rotation") && integer_value(o, "value", 0, 3, &v))
        e = cyd_device_rotation(v);
    else if (!strcmp(a, "confirm"))
        e = cyd_device_confirm();
    else if (!strcmp(a, "revert"))
        e = cyd_device_revert();
    else if (!strcmp(a, "reset_display"))
        e = cyd_device_reset_display();
    else if (!strcmp(a, "clock") && integer_value(o, "epoch", 1704067200, 4102444799LL, &v) &&
             integer_value(o, "offset", -720, 840, &offset))
        e = cyd_device_set_clock(v, offset);
    cJSON_Delete(o);
    if (e)
        return failure(r, e, "Setting unavailable or invalid; finish the current operation first");
    httpd_resp_set_status(r, "202 Accepted");
    return json(r, device_json());
}
#if CYD_PAIR_MANAGER
#include "pair_gateway.inc"
#endif
esp_err_t cyd_web_start(void) {
    httpd_uri_t routes[] = {
        {.uri = "/", .method = HTTP_GET, .handler = index_page},
        {.uri = "/api/status", .method = HTTP_GET, .handler = status},
        {.uri = "/api/versions", .method = HTTP_GET, .handler = versions},
        {.uri = "/api/logs", .method = HTTP_GET, .handler = logs_handler},
        {.uri = "/api/packages", .method = HTTP_POST, .handler = upload},
        {.uri = "/api/packages/inspect", .method = HTTP_POST, .handler = inspect_package},
        {.uri = "/api/run", .method = HTTP_POST, .handler = run},
        {.uri = "/api/stop", .method = HTTP_POST, .handler = stop},
        {.uri = "/api/versions", .method = HTTP_DELETE, .handler = remove_version},
        {.uri = "/api/download", .method = HTTP_GET, .handler = download},
        {.uri = "/api/projects", .method = HTTP_GET, .handler = projects_get},
        {.uri = "/api/projects", .method = HTTP_POST, .handler = projects_post},
        {.uri = "/api/device", .method = HTTP_GET, .handler = device_get},
        {.uri = "/api/device", .method = HTTP_POST, .handler = device_post},
        {.uri = "/api/os", .method = HTTP_GET, .handler = os_status},
        {.uri = "/api/os/update", .method = HTTP_POST, .handler = os_upload},
        {.uri = "/api/os/reboot", .method = HTTP_POST, .handler = os_reboot},
        {.uri = "/api/os/cancel", .method = HTTP_POST, .handler = os_cancel},
        {.uri = "/api/os/rollback", .method = HTTP_POST, .handler = os_rollback},
#if CYD_PAIR_MANAGER
        {.uri = "/api/recovery/os", .method = HTTP_GET, .handler = os_status},
        {.uri = "/api/recovery/os/update", .method = HTTP_POST, .handler = os_upload},
        {.uri = "/api/recovery/os/reboot", .method = HTTP_POST, .handler = os_reboot},
        {.uri = "/api/recovery/os/cancel", .method = HTTP_POST, .handler = os_cancel},
#endif
    };
#if CYD_PAIR_MANAGER
    for (unsigned i = 0; i < sizeof(routes) / sizeof(*routes); i++) {
        httpd_uri_t *route = &routes[i];
        if(!strcmp(route->uri,"/api/versions")||!strcmp(route->uri,"/api/packages")||!strcmp(route->uri,"/api/packages/inspect")||
           !strcmp(route->uri,"/api/run")||!strcmp(route->uri,"/api/stop")||!strcmp(route->uri,"/api/download")||!strcmp(route->uri,"/api/projects")||
           !strcmp(route->uri,"/api/os")||!strcmp(route->uri,"/api/os/update")||!strcmp(route->uri,"/api/os/reboot")||!strcmp(route->uri,"/api/os/cancel")||!strcmp(route->uri,"/api/os/rollback"))route->handler=pair_gateway;
    }
#endif
    return cyd_web_register(routes, sizeof(routes) / sizeof(*routes));
}
