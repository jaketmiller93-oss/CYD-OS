/* Actual HTTPD allocation/registration sequencer; task/socket SDK doubles. */
#include "../components/web_api/startup.c"
#include <assert.h>
#include <string.h>
static unsigned largest, starts, registrations, stops, reject_route;
static bool access_ready = true;
static char failure[192];
bool cyd_access_ready(void) { return access_ready; }
const char *esp_err_to_name(esp_err_t e) {
    return e == ESP_ERR_HTTPD_TASK ? "ESP_ERR_HTTPD_TASK" : "ESP_ERR_HTTPD_HANDLERS_FULL";
}
void cyd_log(const char *tag, const char *message) {
    assert(!strcmp(tag, "web")); snprintf(failure, sizeof(failure), "%s", message);
}
esp_err_t httpd_start(httpd_handle_t *handle, const httpd_config_t *config) {
    starts++;
    assert(config->stack_size == 16384);
    assert(config->task_caps == (MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    assert(config->max_uri_handlers >= 23 && config->lru_purge_enable);
    if (largest < config->stack_size)
        return ESP_ERR_HTTPD_TASK;
    *handle = (void *)1;
    largest -= config->stack_size;
    return ESP_OK;
}
esp_err_t httpd_register_uri_handler(httpd_handle_t handle, const httpd_uri_t *route) {
    assert(handle == (void *)1 && route->uri); registrations++;
    return registrations == reject_route ? ESP_ERR_HTTPD_HANDLERS_FULL : ESP_OK;
}
esp_err_t httpd_stop(httpd_handle_t handle) {
    assert(handle == (void *)1); stops++; return ESP_OK;
}
int main(void) {
    const httpd_uri_t routes[] = {{"/"}, {"/api/status"}, {"/api/recovery/os"}};
    largest = 9216; /* Exact observed largest internal block from user's photo. */
    assert(cyd_web_register(routes, 3) == ESP_ERR_HTTPD_TASK);
    assert(!server && !registered && !registrations);
    starts = 0; largest = 32768;
    assert(cyd_web_prepare() == ESP_OK && starts == 1);
    assert(!registered && !registrations); /* No routes exposed before UI ready. */
    largest = 9216; /* UI has now consumed/fragmented the remaining internal heap. */
    assert(cyd_web_prepare() == ESP_OK && starts == 1);
    reject_route = 2;
    assert(cyd_web_register(routes, 3) == ESP_ERR_HTTPD_HANDLERS_FULL);
    assert(!server && !registered && stops == 1);
    assert(strstr(failure, "/api/status") && strstr(failure, "0xb001"));
    largest = 32768; reject_route = 0; registrations = 0;
    assert(cyd_web_prepare() == ESP_OK && starts == 2);
    largest = 9216;
    assert(cyd_web_register(routes, 3) == ESP_OK);
    assert(starts == 2 && registrations == 3 && registered);
    assert(cyd_web_register(routes, 3) == ESP_ERR_INVALID_STATE && registrations == 3);
    server = NULL; registered = false; access_ready = false;
    assert(cyd_web_prepare() == ESP_ERR_INVALID_ARG && starts == 2);
    puts("PASS exact HTTPD task failure, early stack reservation, delayed routes and cleanup");
    return 0;
}
