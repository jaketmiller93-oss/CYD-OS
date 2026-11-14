#include "system_sdk/cyd.h"
#include "../components/ota_manager/ota.c"
static cyd_os_image_desc_t desc = {
    .magic = "CYD-OS-UPDATE",
    .format = 1,
    .board = "nm_cyd_c5",
    .features = 7,
    .wifi_schema = 1,
    .access_schema = 1,
    .min_ota_protocol = 1,
    .catalog_schema = 1,
    .layout_sha256 = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"};
const cyd_os_image_desc_t *cyd_ota_image_descriptor(void) {
    return &desc;
}
static esp_partition_t a = {0x20000, 0x700000, "ota_0"}, b = {0x720000, 0x700000, "ota_1"},
                       otadata = {0xf000, 0x2000, "otadata"}, nvs = {0x9000, 0x6000, "nvs"};
static int begin_count, end_count, abort_count, select_count, confirm_count, rollback_count,
    restart_count;
static bool pending_boot, can_rollback, fail_write, fail_end, fail_hash, fail_stop, fail_select;
static uint8_t prefix[512];
const esp_partition_t *esp_ota_get_running_partition(void) {
    return &a;
}
const esp_partition_t *esp_ota_get_next_update_partition(const esp_partition_t *p) {
    (void)p;
    return &b;
}
const esp_partition_t *esp_partition_find_first(int type, int subtype, const char *label) {
    (void)label;
    if (type == 0)
        return subtype == 0x10 ? &a : &b;
    return subtype == 0 ? &otadata : &nvs;
}
esp_err_t esp_flash_get_size(void *p, uint32_t *s) {
    (void)p;
    *s = 16 * 1024 * 1024;
    return 0;
}
esp_err_t esp_ota_get_state_partition(const esp_partition_t *p, esp_ota_img_states_t *s) {
    (void)p;
    *s = pending_boot ? ESP_OTA_IMG_PENDING_VERIFY : ESP_OTA_IMG_VALID;
    return 0;
}
bool esp_ota_check_rollback_is_possible(void) {
    return can_rollback;
}
esp_err_t esp_ota_mark_app_valid_cancel_rollback(void) {
    confirm_count++;
    return 0;
}
esp_err_t esp_ota_mark_app_invalid_rollback_and_reboot(void) {
    rollback_count++;
    return 0;
}
esp_err_t esp_ota_begin(const esp_partition_t *p, size_t size, esp_ota_handle_t *h) {
    assert(p == &b && size <= b.size);
    begin_count++;
    *h = 1;
    can_rollback = false;
    return 0;
}
esp_err_t esp_ota_write(esp_ota_handle_t h, const void *data, size_t n) {
    (void)h;
    (void)data;
    (void)n;
    return fail_write ? ESP_FAIL : 0;
}
esp_err_t esp_ota_end(esp_ota_handle_t h) {
    (void)h;
    end_count++;
    return fail_end ? ESP_FAIL : 0;
}
esp_err_t esp_ota_abort(esp_ota_handle_t h) {
    (void)h;
    abort_count++;
    return 0;
}
esp_err_t esp_partition_get_sha256(const esp_partition_t *p, uint8_t *hash) {
    assert(p == &b);
    memset(hash, 0xaa, 32);
    return fail_hash ? ESP_FAIL : 0;
}
esp_err_t esp_ota_set_boot_partition(const esp_partition_t *p) {
    assert(p == &b);
    select_count++;
    return fail_select ? ESP_FAIL : 0;
}
esp_err_t esp_partition_read(const esp_partition_t *p, size_t offset, void *out, size_t n) {
    assert(p == &b && !offset);
    memcpy(out, prefix, n);
    return 0;
}
void esp_restart(void) {
    restart_count++;
}
esp_err_t cyd_runtime_stop(void) {
    return fail_stop ? ESP_FAIL : 0;
}
static void reset_ota(bool pending) {
    host_reset();
    memset(&state, 0, sizeof(state));
    mutex = NULL;
    writer_open = health_finished = false;
    stable_started = 0;
    atomic_store(&health_bits, 0);
    atomic_store(&ui_heartbeat, 0);
    atomic_store(&network_heartbeat, 0);
    begin_count = end_count = abort_count = select_count = confirm_count = rollback_count =
        restart_count = 0;
    pending_boot = pending;
    can_rollback = true;
    fail_write = fail_end = fail_hash = fail_stop = fail_select = false;
    memset(prefix, 0, sizeof(prefix));
    prefix[0] = 0xe9;
    prefix[1] = 3;
    prefix[3] = 0x40;
    prefix[12] = 0x17;
    prefix[23] = 1;
    uint32_t len = 1024, magic = 0xabcd5432;
    memcpy(prefix + 28, &len, 4);
    memcpy(prefix + 32, &magic, 4);
    memcpy(prefix + 48, "0.3.1", 6);
    memcpy(prefix + 80, "cyd-os", 7);
    memcpy(prefix + 288, &desc, sizeof(desc));
    assert(cyd_ota_init() == 0);
}
static void healthy(unsigned ms) {
    cyd_ota_health_report(CYD_HEALTH_ALL);
    for (unsigned i = 0; i <= ms; i += 250) {
        cyd_ota_health_heartbeat(CYD_HEALTH_UI);
        cyd_ota_health_heartbeat(CYD_HEALTH_NETWORK);
        health_step();
        host_tick(250);
    }
}
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x))                                                                                  \
            return __LINE__;                                                                       \
    } while (0)
int host_ota_case(int which) {
    reset_ota(which == 6 || which == 7);
    char error[160];
    if (which == 0) {
        CHECK(cyd_ota_begin(prefix, 512, 1024, error, sizeof(error)) != 0);
        CHECK(begin_count == 0);
        healthy(11000);
        CHECK(state.boot_confirmed);
        return 0;
    }
    if (which == 6) {
        healthy(11000);
        CHECK(confirm_count == 1);
        CHECK(rollback_count == 0);
        CHECK(!state.pending_verify);
        return 0;
    }
    if (which == 7) {
        cyd_ota_health_report(CYD_HEALTH_ALL);
        cyd_ota_health_heartbeat(CYD_HEALTH_UI);
        cyd_ota_health_heartbeat(CYD_HEALTH_NETWORK);
        host_tick(46000);
        health_step();
        CHECK(rollback_count == 1);
        CHECK(confirm_count == 0);
        return 0;
    }
    healthy(11000);
    if (which == 8) {
        fail_stop = true;
        CHECK(cyd_ota_begin(prefix, 512, 1024, error, sizeof(error)) != 0);
        CHECK(begin_count == 0 && !cyd_maintenance_active());
        return 0;
    }
    if (which == 9) {
        prefix[12] = 9;
        CHECK(cyd_ota_begin(prefix, 512, 1024, error, sizeof(error)) != 0);
        CHECK(begin_count == 0);
        return 0;
    }
    CHECK(cyd_ota_begin(prefix, 512, 1024, error, sizeof(error)) == 0);
    CHECK(begin_count == 1);
    CHECK(select_count == 0);
    CHECK(cyd_maintenance_active());
    if (which == 2) {
        CHECK(cyd_ota_write(prefix, 512) == 0);
        cyd_ota_abort("interrupted");
        CHECK(abort_count == 1 && select_count == 0 && !cyd_maintenance_active());
        return 0;
    }
    if (which == 3) {
        fail_write = true;
        CHECK(cyd_ota_write(prefix, 512) != 0);
        CHECK(abort_count == 1 && select_count == 0 && !cyd_maintenance_active());
        return 0;
    }
    CHECK(cyd_ota_write(prefix, 512) == 0);
    CHECK(cyd_ota_write(prefix, 512) == 0);
    if (which == 4)
        fail_end = true;
    if (which == 5)
        fail_hash = true;
    if (which == 4 || which == 5) {
        CHECK(cyd_ota_finish(error, sizeof(error)) != 0);
        CHECK(select_count == 0 && !cyd_maintenance_active());
        return 0;
    }
    CHECK(cyd_ota_finish(error, sizeof(error)) == 0);
    CHECK(state.stage == CYD_OS_READY && select_count == 0);
    CHECK(state.sha256[0] == 'a');
    if (which == 10) {
        CHECK(cyd_ota_cancel() == 0);
        CHECK(select_count == 0 && !cyd_maintenance_active());
        return 0;
    }
    if (which == 11) {
        host_fail_task(true);
        CHECK(cyd_ota_reboot() != 0);
        CHECK(state.stage == CYD_OS_READY);
        host_fail_task(false);
        CHECK(cyd_ota_cancel() == 0);
        return 0;
    }
    CHECK(cyd_ota_reboot() == 0);
    CHECK(select_count == 0);
    restart_task(NULL);
    CHECK(select_count == 1 && restart_count == 1);
    return 0;
}
