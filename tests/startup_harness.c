/* Actual app_main, with explicit service/allocation-order doubles. */
#include "../main/main.c"
#include <assert.h>
#include <string.h>
static int scenario, order, web_order, peer_order, peer_calls;
static int pair_order, pair_calls;
static int prepare_order, prepare_calls, ui_order;
static unsigned health;
static char failure_log[200];
const char *cyd_system_version(void) { return "fixture"; }
const char *cyd_board_model(void) { return "fixture"; }
const char *esp_err_to_name(int e) { (void)e; return "ESP_ERR_NO_MEM"; }
esp_err_t cyd_ota_init(void) { return 0; }
esp_err_t nvs_flash_init(void) { return 0; }
esp_err_t cyd_access_init(void) { return 0; }
esp_err_t cyd_runtime_init(void) { return 0; }
esp_err_t cyd_manager_init(void) { return 0; }
esp_err_t cyd_board_init(void) { return 0; }
esp_err_t cyd_storage_init(void) { return 0; }
esp_err_t cyd_network_init(void) { return 0; }
esp_err_t cyd_ui_init(void) { ui_order = ++order; return 0; }
unsigned cyd_board_width(void) { return 320; }
unsigned cyd_board_height(void) { return 240; }
esp_err_t cyd_board_set_brightness(unsigned x) { (void)x; return 0; }
esp_err_t cyd_ui_apply_rotation(unsigned x) { (void)x; return 0; }
esp_err_t cyd_device_init(unsigned w, unsigned h, esp_err_t (*b)(unsigned), esp_err_t (*r)(unsigned)) {
    (void)w; (void)h; (void)b; (void)r; return 0;
}
void cyd_device_clock_start(void) {}
void cyd_ota_health_report(unsigned bits) { health |= bits; }
size_t heap_caps_get_largest_free_block(unsigned caps) {
    assert(caps == (MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)); return 12000;
}
esp_err_t cyd_web_start(void) {
    web_order = ++order;
    /* Reproduces pressure when an optional task reserves RAM first. */
    return scenario == 1 || scenario == 4 || peer_calls ||
        (scenario == 5 && ui_order && !prepare_calls) ? ESP_ERR_NO_MEM : 0;
}
esp_err_t cyd_web_prepare(void) {
    prepare_calls++;
    prepare_order = ++order;
    assert(!(health & CYD_HEALTH_API));
    return scenario == 1 ? ESP_ERR_NO_MEM : 0;
}
esp_err_t cyd_peer_start(void) {
    peer_calls++;
    peer_order = ++order;
    return scenario == 2 ? ESP_ERR_NO_MEM : 0;
}
esp_err_t cyd_pair_memory_init(void) { return 0; }
esp_err_t cyd_pair_start(void) {
    pair_calls++;
    pair_order = ++order;
    return scenario == 3 ? ESP_ERR_NO_MEM : 0;
}
void cyd_log(const char *tag, const char *message) {
    if (!strcmp(tag, "boot")) snprintf(failure_log, sizeof(failure_log), "%s", message);
}
int startup_case(int which) {
    scenario = which;
    order = web_order = peer_order = peer_calls = 0;
    pair_order = pair_calls = 0;
    prepare_order = prepare_calls = ui_order = 0;
    health = 0; failure_log[0] = 0;
    app_main();
    if (which == 0) return !(web_order && peer_order > web_order && (health & CYD_HEALTH_API))
#ifdef CONFIG_CYD_PAIRED_SYSTEM
        || !(pair_calls == 1 && pair_order > peer_order && prepare_calls == 1 &&
             prepare_order < ui_order && ui_order < web_order)
#endif
        ;
    if (which == 1) return peer_calls || pair_calls || (health & CYD_HEALTH_API) ||
        !strstr(failure_log, "ESP_ERR_NO_MEM (0x101)") || !strstr(failure_log, "12000 B");
#ifdef CONFIG_CYD_PAIRED_SYSTEM
    if (which == 2 || which == 3) return (health & CYD_HEALTH_API) ||
        !strstr(failure_log, which == 2 ? "Paired UART" : "Paired service") ||
        !strstr(failure_log, "ESP_ERR_NO_MEM (0x101)") ||
        (which == 2 ? pair_calls != 0 : pair_calls != 1);
    if (which == 4) return (health & CYD_HEALTH_API) || peer_calls || pair_calls ||
        prepare_calls != 1 || !web_order || !strstr(failure_log, "Web API initialization failed");
    if (which == 5) return !(health & CYD_HEALTH_API) || prepare_calls != 1 ||
        !(prepare_order < ui_order && ui_order < web_order && web_order < peer_order);
#else
    if (which == 2) return !(peer_calls == 1 && (health & CYD_HEALTH_API));
#endif
    return 99;
}
