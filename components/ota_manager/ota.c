#include "cyd.h"
#include "cyd_ota.h"
#include "sdkconfig.h"
#include "esp_ota_ops.h"
#include "esp_app_desc.h"
#include "esp_app_format.h"
#include "esp_partition.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "esp_timer.h"
#include <stdio.h>
#include <string.h>
#include <stdatomic.h>
_Static_assert(sizeof(esp_image_header_t) == 24, "Image header ABI");
_Static_assert(sizeof(esp_app_desc_t) == 256, "App descriptor ABI");
_Static_assert(offsetof(esp_app_desc_t, version) == 16, "Version ABI");
_Static_assert(offsetof(esp_app_desc_t, project_name) == 48, "Project ABI");
static SemaphoreHandle_t mutex;
static cyd_ota_status_t state;
static const esp_partition_t *running, *target;
static esp_ota_handle_t writer;
static bool writer_open;
static atomic_uint health_bits, ui_heartbeat, network_heartbeat;
static uint32_t boot_started, stable_started;
static bool health_finished;
static uint32_t now_ms(void) {
    return (uint32_t)(esp_timer_get_time() / 1000);
}
static void set_stage(cyd_ota_stage_t stage, const char *msg) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    state.stage = stage;
    snprintf(state.message, sizeof(state.message), "%s", msg);
    xSemaphoreGive(mutex);
}
void cyd_ota_health_report(unsigned bits) {
    atomic_fetch_or(&health_bits, bits);
}
void cyd_ota_health_heartbeat(unsigned bit) {
    if (bit == CYD_HEALTH_UI)
        atomic_store(&ui_heartbeat, now_ms());
    if (bit == CYD_HEALTH_NETWORK)
        atomic_store(&network_heartbeat, now_ms());
    cyd_ota_health_report(bit);
}
void cyd_ota_snapshot(cyd_ota_status_t *out) {
    memset(out, 0, sizeof(*out));
    out->stage = CYD_OS_BOOTING;
    if (!mutex)
        return;
    xSemaphoreTake(mutex, portMAX_DELAY);
    *out = state;
    xSemaphoreGive(mutex);
}
static bool partition_layout_valid(void) {
    const esp_partition_t *a =
        esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
    const esp_partition_t *b =
        esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, NULL);
    const esp_partition_t *o =
        esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, NULL);
    const esp_partition_t *n =
        esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, NULL);
    uint32_t flash = 0;
    return a && b && o && n && running && target && running != target &&
           (running->address == a->address || running->address == b->address) &&
           a->address == 0x20000 && a->size == CYD_OTA_SLOT_SIZE && b->address == 0x720000 &&
           b->size == CYD_OTA_SLOT_SIZE && o->address == 0xf000 && o->size == 0x2000 &&
           n->address == 0x9000 && n->size == 0x6000 &&
           esp_flash_get_size(NULL, &flash) == ESP_OK && flash == 16 * 1024 * 1024;
}
static void health_step(void) {
    if (health_finished)
        return;
    uint32_t now = now_ms();
    bool ready = (atomic_load(&health_bits) & CYD_HEALTH_ALL) == CYD_HEALTH_ALL &&
                 (uint32_t)(now - atomic_load(&ui_heartbeat)) < 2500 &&
                 (uint32_t)(now - atomic_load(&network_heartbeat)) < 2500;
    if (ready) {
        if (!stable_started)
            stable_started = now;
        if ((uint32_t)(now - stable_started) >= 10000) {
            esp_err_t e = esp_ota_mark_app_valid_cancel_rollback();
            if (e == ESP_OK) {
                xSemaphoreTake(mutex, portMAX_DELAY);
                state.boot_confirmed = true;
                state.pending_verify = false;
                state.rollback_available = esp_ota_check_rollback_is_possible();
                xSemaphoreGive(mutex);
                set_stage(CYD_OS_IDLE, "OS startup confirmed; wireless updates available");
                cyd_log("os", "Startup checks passed; firmware confirmed");
                health_finished = true;
                return;
            }
        }
    } else
        stable_started = 0;
    if ((uint32_t)(now - boot_started) >= 45000) {
        bool pending = state.pending_verify;
        set_stage(CYD_OS_FAILED, pending ? "Startup checks failed; reverting to the previous OS"
                                         : "Startup checks failed; wireless updates disabled");
        health_finished = true;
        cyd_log("os", "OS startup checks failed");
        if (pending) {
            esp_err_t e = esp_ota_mark_app_invalid_rollback_and_reboot();
            if (e)
                set_stage(CYD_OS_FAILED, "Automatic rollback failed; use recovery instructions");
        }
    }
}
static void health_task(void *arg) {
    (void)arg;
    while (!health_finished) {
        health_step();
        vTaskDelay(pdMS_TO_TICKS(250));
    }
    vTaskDelete(NULL);
}
esp_err_t cyd_ota_init(void) {
    mutex = xSemaphoreCreateMutex();
    if (!mutex)
        return ESP_ERR_NO_MEM;
    running = esp_ota_get_running_partition();
    target = esp_ota_get_next_update_partition(NULL);
    snprintf(state.version, sizeof(state.version), "%s", cyd_system_version());
    if (running)
        snprintf(state.running_slot, sizeof(state.running_slot), "%s", running->label);
    if (target)
        snprintf(state.target_slot, sizeof(state.target_slot), "%s", target->label);
    esp_ota_img_states_t image_state = ESP_OTA_IMG_UNDEFINED;
    if (running && esp_ota_get_state_partition(running, &image_state) == ESP_OK)
        state.pending_verify = image_state == ESP_OTA_IMG_PENDING_VERIFY;
    state.rollback_available = esp_ota_check_rollback_is_possible();
    set_stage(CYD_OS_BOOTING, "Checking startup (about 10 seconds after services are ready)");
    if (!partition_layout_valid()) {
        set_stage(CYD_OS_FAILED, "Unsupported partition layout; USB recovery may be needed");
        if (state.pending_verify)
            esp_ota_mark_app_invalid_rollback_and_reboot();
        return ESP_ERR_INVALID_STATE;
    }
    boot_started = now_ms();
    return xTaskCreate(health_task, "os_health", 4096, NULL, 4, NULL) == pdPASS ? ESP_OK
                                                                                : ESP_ERR_NO_MEM;
}
static uint16_t chip_id(void) {
#ifdef CONFIG_IDF_TARGET_ESP32C5
    return ESP_CHIP_ID_ESP32C5;
#else
    return ESP_CHIP_ID_ESP32S3;
#endif
}
void cyd_ota_abort(const char *reason) {
    if (writer_open) {
        esp_ota_abort(writer);
        writer_open = false;
    }
    set_stage(CYD_OS_FAILED, reason);
    cyd_maintenance_end();
    cyd_log("os", reason);
}
esp_err_t cyd_ota_begin(const uint8_t *prefix, size_t n, size_t total, char *error, size_t ec) {
    cyd_ota_status_t s;
    cyd_ota_snapshot(&s);
    if (!s.boot_confirmed || (s.stage != CYD_OS_IDLE && s.stage != CYD_OS_FAILED)) {
        snprintf(error, ec, "Wait for startup confirmation, or finish/cancel the staged update");
        return ESP_ERR_INVALID_STATE;
    }
    const cyd_os_image_desc_t *d = cyd_ota_image_descriptor();
    if (!partition_layout_valid() || total > target->size) {
        snprintf(error, ec, "Image does not fit the inactive OTA slot");
        return ESP_ERR_INVALID_SIZE;
    }
    if (!cyd_ota_policy_validate(prefix, n, total, chip_id(), d->board, d->layout_sha256, error,
                                 ec))
        return ESP_ERR_INVALID_ARG;
    if (!cyd_maintenance_begin()) {
        snprintf(error, ec, "Another maintenance operation is active");
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t e = cyd_runtime_stop();
    if (e) {
        snprintf(error, ec, "The Lua app did not stop; OS update was not started");
        cyd_maintenance_end();
        return e;
    }
    xSemaphoreTake(mutex, portMAX_DELAY);
    state.received = 0;
    state.total = total;
    state.sha256[0] = 0;
    memcpy(state.candidate_version, prefix + 48, 32);
    xSemaphoreGive(mutex);
    set_stage(CYD_OS_UPLOADING, "Receiving firmware into the inactive slot; keep power connected");
    e = esp_ota_begin(target, total, &writer);
    if (e) {
        snprintf(error, ec, "Could not prepare the inactive OTA slot");
        cyd_ota_abort(error);
        return e;
    }
    writer_open = true;
    xSemaphoreTake(mutex, portMAX_DELAY);
    state.rollback_available = false;
    xSemaphoreGive(mutex);
    return ESP_OK;
}
esp_err_t cyd_ota_write(const void *data, size_t n) {
    if (!writer_open || !data || n > state.total - state.received)
        return ESP_ERR_INVALID_ARG;
    esp_err_t e = esp_ota_write(writer, data, n);
    if (e) {
        cyd_ota_abort("Flash write failed; current OS remains selected");
        return e;
    }
    xSemaphoreTake(mutex, portMAX_DELAY);
    state.received += n;
    xSemaphoreGive(mutex);
    return ESP_OK;
}
esp_err_t cyd_ota_finish(char *error, size_t ec) {
    if (!writer_open || state.received != state.total) {
        snprintf(error, ec, "Firmware upload was incomplete");
        cyd_ota_abort(error);
        return ESP_ERR_INVALID_SIZE;
    }
    set_stage(CYD_OS_VERIFYING, "Verifying application image and appended SHA-256");
    esp_err_t e = esp_ota_end(writer);
    writer_open = false;
    if (e) {
        snprintf(error, ec, "Image integrity/chip validation failed; current OS remains selected");
        cyd_ota_abort(error);
        return e;
    }
    uint8_t hash[32];
    e = esp_partition_get_sha256(target, hash);
    if (e) {
        snprintf(error, ec, "Image digest verification failed");
        cyd_ota_abort(error);
        return e;
    }
    char hex[65];
    for (unsigned i = 0; i < 32; i++)
        snprintf(hex + i * 2, 3, "%02x", hash[i]);
    xSemaphoreTake(mutex, portMAX_DELAY);
    snprintf(state.sha256, sizeof(state.sha256), "%s", hex);
    xSemaphoreGive(mutex);
    /* Do not change boot selection until the operator explicitly requests restart. */
    set_stage(CYD_OS_READY,
              "Firmware verified. Restart to install, or cancel to keep the current OS");
    return ESP_OK;
}
esp_err_t cyd_ota_cancel(void) {
    if (!mutex)
        return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(mutex, portMAX_DELAY);
    if (state.stage != CYD_OS_READY) {
        xSemaphoreGive(mutex);
        return ESP_ERR_INVALID_STATE;
    }
    state.stage = CYD_OS_IDLE;
    state.received = state.total = 0;
    state.candidate_version[0] = 0;
    snprintf(state.message, sizeof(state.message),
             "Staged update cancelled; current OS remains selected");
    xSemaphoreGive(mutex);
    cyd_maintenance_end();
    return ESP_OK;
}
static void restart_task(void *arg) {
    bool rollback = (bool)(uintptr_t)arg;
    vTaskDelay(pdMS_TO_TICKS(750));
    esp_err_t e = rollback ? esp_ota_mark_app_invalid_rollback_and_reboot()
                           : esp_ota_set_boot_partition(target);
    if (!e && !rollback)
        esp_restart();
    /* Real successful restart never returns. */
    if (e) {
        set_stage(CYD_OS_FAILED, rollback ? "Rollback failed; current OS retained"
                                          : "Could not select the new OS; current OS retained");
        cyd_maintenance_end();
    }
    vTaskDelete(NULL);
}
esp_err_t cyd_ota_reboot(void) {
    if (!mutex)
        return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(mutex, portMAX_DELAY);
    if (state.stage != CYD_OS_READY) {
        xSemaphoreGive(mutex);
        return ESP_ERR_INVALID_STATE;
    }
    state.stage = CYD_OS_REBOOTING;
    snprintf(state.message, sizeof(state.message),
             "Restarting into the verified OS; startup rollback is armed");
    xSemaphoreGive(mutex);
    if (xTaskCreate(restart_task, "os_restart", 4096, NULL, 4, NULL) != pdPASS) {
        set_stage(CYD_OS_READY, "Could not create restart task; retry or cancel");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
esp_err_t cyd_ota_rollback(void) {
    cyd_ota_status_t s;
    cyd_ota_snapshot(&s);
    if (!s.boot_confirmed || (s.stage != CYD_OS_IDLE && s.stage != CYD_OS_FAILED) ||
        !esp_ota_check_rollback_is_possible())
        return ESP_ERR_INVALID_STATE;
    /* Only restore another CYD image that preserves this updater and settings. */
    uint8_t prefix[CYD_OTA_PREFIX_BYTES];
    char error[160];
    const cyd_os_image_desc_t *d = cyd_ota_image_descriptor();
    if (esp_partition_read(target, 0, prefix, sizeof(prefix)) ||
        !cyd_ota_policy_validate(prefix, sizeof(prefix), target->size, chip_id(), d->board,
                                 d->layout_sha256, error, sizeof(error)))
        return ESP_ERR_NOT_SUPPORTED;
    if (!cyd_maintenance_begin())
        return ESP_ERR_INVALID_STATE;
    esp_err_t e = cyd_runtime_stop();
    if (e) {
        cyd_maintenance_end();
        return e;
    }
    set_stage(CYD_OS_REBOOTING, "Restoring the previous confirmed CYD OS");
    if (xTaskCreate(restart_task, "os_rollback", 4096, (void *)(uintptr_t)true, 4, NULL) !=
        pdPASS) {
        set_stage(CYD_OS_FAILED, "Could not create rollback task");
        cyd_maintenance_end();
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
