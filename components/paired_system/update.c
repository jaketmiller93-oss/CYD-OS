#include "pair_internal.h"
#include "pair_bundle.h"
#include "cyd_ota.h"
#include "cyd_peer.h"
#include "mbedtls/sha256.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "freertos/task.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdatomic.h>
static SemaphoreHandle_t gate;
static FILE *upload;
static size_t expected, received;
static cyd_bundle_t bundle;
static atomic_bool ready, working;
static char phase[24] = "idle", message[160] = "One paired update for both boards";
static size_t progress, progress_total;
static char expected_host_elf[65];
static uint64_t upload_at, flash_at;
static atomic_bool installing;
bool cyd_pair_installing(void) {
    return atomic_load(&installing);
}
static const char *temp = CYD_ROOT "/system/paired-upload.tmp";
static const char *candidate = CYD_ROOT "/system/paired-update.cydos";
static void state(const char *name, const char *why) {
    xSemaphoreTake(gate, portMAX_DELAY);
    snprintf(phase, sizeof(phase), "%s", name);
    snprintf(message, sizeof(message), "%s", why);
    xSemaphoreGive(gate);
    cyd_log("system-update", why);
}
static cJSON *snapshot(void) {
    cJSON *o = cJSON_CreateObject();
    xSemaphoreTake(gate, portMAX_DELAY);
    cJSON_AddStringToObject(o, "state", phase);
    cJSON_AddStringToObject(o, "message", message);
    cJSON_AddNumberToObject(o, "received", progress);
    cJSON_AddNumberToObject(o, "total", progress_total);
    cJSON_AddStringToObject(o, "board", "paired_tablet");
    cJSON_AddStringToObject(o, "running_slot", "S3 + C5");
    cJSON_AddStringToObject(o, "version", cyd_system_version());
    cJSON_AddStringToObject(o, "candidate_version", ready || working ? bundle.version : "");
    cJSON_AddStringToObject(o, "candidate_elf_sha256", ready || working ? expected_host_elf : "");
    cJSON_AddNumberToObject(o, "max_image_bytes", CYD_BUNDLE_LIMIT);
    cJSON_AddBoolToObject(o, "paired_update", true);
    xSemaphoreGive(gate);
    cyd_ota_status_t ota;
    cyd_ota_snapshot(&ota);
    cJSON_AddBoolToObject(o, "boot_confirmed", ota.boot_confirmed);
    char running_elf[65];
    cyd_pair_elf_sha256(running_elf);
    cJSON_AddStringToObject(o, "elf_sha256", running_elf);
    cJSON_AddBoolToObject(o, "pending_verify", ota.pending_verify);
    cJSON_AddBoolToObject(o, "rollback_available", false);
    return o;
}
static esp_err_t call(unsigned op, const void *in, size_t n, void *out, size_t cap, size_t *got) {
    return cyd_pair_rpc(op, in, n, out, cap, got);
}
static void fail_update(const char *why) {
    atomic_store(&installing, false);
    cyd_ota_status_t local;
    cyd_ota_snapshot(&local);
    if (local.stage == CYD_OS_UPLOADING || local.stage == CYD_OS_VERIFYING)
        cyd_ota_abort(why);
    else
        cyd_ota_cancel();
    uint8_t ignored[256];
    size_t got = 0;
    call(CYD_PAIR_UPDATE_CANCEL, NULL, 0, ignored, sizeof(ignored), &got);
    xSemaphoreTake(gate, portMAX_DELAY);
    ready = working = false;
    xSemaphoreGive(gate);
    state("failed", why);
}
static bool validate(FILE *f, size_t total, cyd_bundle_t *b, uint8_t *buffer) {
    uint8_t prefix[512];
    if (fseek(f, 0, SEEK_SET) || fread(prefix, 1, 512, f) != 512 ||
        !cyd_bundle_parse(prefix, 512, total, b))
        return false;
    const char *layout = cyd_ota_image_descriptor()->layout_sha256;
    for (unsigned i = 0; i < 2; i++) {
        cyd_bundle_image_t *im = &b->images[i];
        char error[160];
        if (fseek(f, (long)im->offset, SEEK_SET) || fread(prefix, 1, 512, f) != 512 ||
            !cyd_ota_policy_validate(prefix, 512, im->size, i == 0 ? 9 : 0x17, im->board, layout,
                                     error, sizeof(error)) ||
            memcmp(prefix + 48, im->version, strlen(im->version) + 1) ||
            !(prefix[332] & CYD_OTA_FEATURE_PAIRED) || prefix[417] != 1 || prefix[418] != i + 1)
            return false;
        if (fseek(f, (long)im->offset, SEEK_SET))
            return false;
        mbedtls_sha256_context hash;
        mbedtls_sha256_init(&hash);
        mbedtls_sha256_starts(&hash, 0);
        size_t left = im->size;
        while (left) {
            size_t take = left > 2048 ? 2048 : left;
            if (fread(buffer, 1, take, f) != take) {
                mbedtls_sha256_free(&hash);
                return false;
            }
            mbedtls_sha256_update(&hash, buffer, take);
            left -= take;
            vTaskDelay(1);
        }
        uint8_t digest[32];
        mbedtls_sha256_finish(&hash, digest);
        mbedtls_sha256_free(&hash);
        if (memcmp(digest, im->sha256, 32))
            return false;
    }
    return true;
}
static void verify_task(void *arg) {
    (void)arg;
    uint8_t *buf = heap_caps_malloc(2048, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    FILE *f = fopen(candidate, "rb");
    cyd_bundle_t checked;
    bool ok = buf && f && validate(f, expected, &checked, buf);
    uint8_t identity[32];
    char candidate_elf[65] = "";
    if (ok) {
        ok = !fseek(f, (long)checked.images[0].offset + 176, SEEK_SET) &&
             fread(identity, 1, 32, f) == 32;
        if (ok)
            for (unsigned i = 0; i < 32; i++)
                snprintf(candidate_elf + i * 2, 3, "%02x", identity[i]);
    }
    if (f)
        fclose(f);
    free(buf);
    xSemaphoreTake(gate, portMAX_DELAY);
    working = false;
    ready = ok;
    if (ok) {
        bundle = checked;
        strcpy(expected_host_elf, candidate_elf);
    }
    xSemaphoreGive(gate);
    state(ok ? "ready" : "failed",
          ok ? "Both board images verified. Install the paired update when ready."
             : "Bundle verification failed; neither board was flashed");
    vTaskDelete(NULL);
}
static esp_err_t stage_image(FILE *f, unsigned i, uint8_t *buf) {
    const cyd_bundle_image_t *im = &bundle.images[i];
    uint8_t prefix[516], reply[2048];
    size_t got = 0;
    char error[160];
    if (fseek(f, (long)im->offset, SEEK_SET) || fread(prefix + 4, 1, 512, f) != 512)
        return ESP_FAIL;
    memcpy(prefix, &im->size, 4);
    esp_err_t e =
        i ? call(CYD_PAIR_UPDATE_BEGIN, prefix, sizeof(prefix), reply, sizeof(reply), &got)
          : cyd_ota_begin(prefix + 4, 512, im->size, error, sizeof(error));
    if (e)
        return e;
    if (fseek(f, (long)im->offset, SEEK_SET))
        return ESP_FAIL;
    size_t off = 0;
    while (off < im->size) {
        size_t take = im->size - off;
        if (take > 2048)
            take = 2048;
        if (fread(buf + 4, 1, take, f) != take)
            return ESP_FAIL;
        uint32_t at = (uint32_t)off;
        memcpy(buf, &at, 4);
        e = i ? call(CYD_PAIR_UPDATE_WRITE, buf, take + 4, reply, sizeof(reply), &got)
              : cyd_ota_write(buf + 4, take);
        if (e)
            return e;
        off += take;
        xSemaphoreTake(gate, portMAX_DELAY);
        progress += take;
        xSemaphoreGive(gate);
        vTaskDelay(1);
    }
    return i ? call(CYD_PAIR_UPDATE_FINISH, NULL, 0, reply, sizeof(reply), &got)
             : cyd_ota_finish(error, sizeof(error));
}
static void install_task(void *arg) {
    if (cyd_runtime_stop()) {
        fail_update("App did not stop; update was not started");
        vTaskDelete(NULL);
        return;
    }
    (void)arg;
    uint8_t *buf = heap_caps_malloc(2052, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    FILE *f = fopen(candidate, "rb");
    if (!buf || !f) {
        if (f)
            fclose(f);
        free(buf);
        fail_update("Could not open the verified bundle; current firmware retained");
        vTaskDelete(NULL);
        return;
    }
    cyd_bundle_t checked;
    if (!validate(f, expected, &checked, buf)) {
        fclose(f);
        free(buf);
        fail_update("Bundle changed after verification; update cancelled");
        vTaskDelete(NULL);
        return;
    }
    state("uploading", "Staging the C5 image over the cable; keep both boards powered");
    xSemaphoreTake(gate, portMAX_DELAY);
    progress = 0;
    progress_total = bundle.images[0].size + bundle.images[1].size;
    xSemaphoreGive(gate);
    uint8_t digest[32];
    char wanted_elf[65];
    if (fseek(f, (long)bundle.images[1].offset + 176, SEEK_SET) || fread(digest, 1, 32, f) != 32) {
        fclose(f);
        free(buf);
        fail_update("Could not read C5 build identity");
        vTaskDelete(NULL);
        return;
    }
    for (unsigned i = 0; i < 32; i++)
        snprintf(wanted_elf + i * 2, 3, "%02x", digest[i]);
    esp_err_t e = stage_image(f, 1, buf);
    if (!e) {
        state("uploading", "C5 staged. Staging the S3 image");
        e = stage_image(f, 0, buf);
    }
    fclose(f);
    free(buf);
    if (e) {
        fail_update("Image staging failed; current boot selections retained. Retry only after "
                    "checking both boards.");
        vTaskDelete(NULL);
        return;
    }
    uint8_t reply[256];
    size_t got = 0;
    state("rebooting", "Restarting the C5 first; waiting for its startup confirmation");
    cJSON *previous = cyd_pair_json(CYD_PAIR_STATUS, NULL);
    char previous_boot[17];
    snprintf(previous_boot, sizeof(previous_boot), "%.16s", cyd_pair_string(previous, "boot_id"));
    cJSON_Delete(previous);
    if (strlen(previous_boot) != 16) {
        fail_update("Could not identify the C5 boot session; staged images cancelled");
        vTaskDelete(NULL);
        return;
    }
    e = call(CYD_PAIR_UPDATE_REBOOT, NULL, 0, reply, sizeof(reply), &got);
    if (e) {
        fail_update("C5 restart acknowledgement missing; S3 retained its current firmware");
        vTaskDelete(NULL);
        return;
    }
    uint64_t deadline = esp_timer_get_time() / 1000 + 120000;
    bool confirmed = false;
    while ((uint64_t)esp_timer_get_time() / 1000 < deadline) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        cJSON *s = cyd_pair_json(CYD_PAIR_STATUS, NULL);
        confirmed = s && strcmp(cyd_pair_string(s, "boot_id"), previous_boot) &&
                    !strcmp(cyd_pair_string(s, "elf_sha256"), wanted_elf) &&
                    !strcmp(cyd_pair_string(s, "firmware"), bundle.version) &&
                    cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(s, "boot_confirmed"));
        cJSON_Delete(s);
        if (confirmed)
            break;
    }
    if (!confirmed) {
        atomic_store(&installing, false);
        cyd_ota_cancel();
        xSemaphoreTake(gate, portMAX_DELAY);
        ready = working = false;
        xSemaphoreGive(gate);
        state("failed", "C5 did not confirm the expected release. S3 stayed on its working "
                        "firmware; inspect Diagnostics.");
        vTaskDelete(NULL);
        return;
    }
    state("rebooting", "C5 confirmed. Restarting the S3 into the matching image");
    e = cyd_ota_reboot();
    if (e)
        fail_update("C5 updated; S3 restart failed. Its current firmware is retained.");
    vTaskDelete(NULL);
}
esp_err_t cyd_pair_update(unsigned op, const void *in, size_t n, void *out, size_t cap,
                          size_t *size) {
    *size = 0;
    if (!gate) {
        gate = xSemaphoreCreateMutex();
        if (!gate)
            return ESP_ERR_NO_MEM;
    }
    if (op == CYD_PAIR_UPDATE_STATUS) {
        cyd_ota_status_t s;
        cyd_ota_snapshot(&s);
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "received", s.received);
        cJSON_AddBoolToObject(o, "ready", s.stage == CYD_OS_READY);
        return cyd_pair_encode(o, out, cap, size);
    }
    if (op == CYD_PAIR_UPDATE_BEGIN) {
        flash_at = esp_timer_get_time() / 1000;
        if (n != 516)
            return ESP_ERR_INVALID_ARG;
        uint32_t total;
        memcpy(&total, in, 4);
        char error[160];
        return cyd_ota_begin((const uint8_t *)in + 4, 512, total, error, sizeof(error));
    }
    if (op == CYD_PAIR_UPDATE_WRITE) {
        flash_at = esp_timer_get_time() / 1000;
        if (n < 5)
            return ESP_ERR_INVALID_ARG;
        uint32_t offset;
        memcpy(&offset, in, 4);
        cyd_ota_status_t s;
        cyd_ota_snapshot(&s);
        if (offset != s.received)
            return ESP_ERR_INVALID_STATE;
        return cyd_ota_write((const uint8_t *)in + 4, n - 4);
    }
    if (op == CYD_PAIR_UPDATE_FINISH) {
        flash_at = esp_timer_get_time() / 1000;
        char error[160];
        return cyd_ota_finish(error, sizeof(error));
    }
    if (op == CYD_PAIR_UPDATE_CANCEL) {
        cyd_ota_status_t s;
        cyd_ota_snapshot(&s);
        if (s.stage == CYD_OS_UPLOADING || s.stage == CYD_OS_VERIFYING) {
            cyd_ota_abort("Paired transfer cancelled; current OS retained");
            return ESP_OK;
        }
        return cyd_ota_cancel();
    }
    if (op == CYD_PAIR_UPDATE_REBOOT)
        return cyd_ota_reboot();
#if CYD_PAIR_HOST
    if (op == CYD_PAIR_BUNDLE_CANCEL) {
        if (working)
            return ESP_ERR_INVALID_STATE;
        if (upload) {
            fclose(upload);
            upload = NULL;
        }
        unlink(temp);
        ready = false;
        state("idle", "Paired upload cancelled; running firmware retained");
        return ESP_OK;
    }
    if (op == CYD_PAIR_BUNDLE_STATUS)
        return cyd_pair_encode(snapshot(), out, cap, size);
    if (op == CYD_PAIR_BUNDLE_BEGIN) {
        if (n != 4 || working)
            return ESP_ERR_INVALID_STATE;
        uint32_t total;
        memcpy(&total, in, 4);
        if (total < 1536 || total > CYD_BUNDLE_LIMIT || !cyd_storage_ready() ||
            cyd_storage_free() < total + 65536)
            return ESP_ERR_INVALID_SIZE;
        if (upload)
            fclose(upload);
        upload = fopen(temp, "wb");
        if (!upload)
            return ESP_FAIL;
        expected = total;
        received = 0;
        ready = false;
        expected_host_elf[0] = 0;
        upload_at = esp_timer_get_time() / 1000;
        xSemaphoreTake(gate, portMAX_DELAY);
        progress = 0;
        progress_total = total;
        xSemaphoreGive(gate);
        state("uploading", "Receiving paired firmware on System drive");
        return ESP_OK;
    }
    if (op == CYD_PAIR_BUNDLE_WRITE) {
        if (!upload || n < 5 || esp_timer_get_time() / 1000 - upload_at > 30000)
            return ESP_ERR_INVALID_STATE;
        uint32_t off;
        memcpy(&off, in, 4);
        if (off != received || n - 4 > expected - received)
            return ESP_ERR_INVALID_ARG;
        if (fwrite((const uint8_t *)in + 4, 1, n - 4, upload) != n - 4)
            return ESP_FAIL;
        received += n - 4;
        upload_at = esp_timer_get_time() / 1000;
        xSemaphoreTake(gate, portMAX_DELAY);
        progress = received;
        xSemaphoreGive(gate);
        return ESP_OK;
    }
    if (op == CYD_PAIR_BUNDLE_FINISH) {
        if (!upload || received != expected)
            return ESP_ERR_INVALID_STATE;
        bool ok = !fflush(upload) && !fsync(fileno(upload));
        if (fclose(upload))
            ok = false;
        upload = NULL;
        if (!ok)
            return ESP_FAIL;
        unlink(candidate);
        if (rename(temp, candidate))
            return ESP_FAIL;
        working = true;
        state("verifying", "Verifying both board images before writing either flash");
        if (xTaskCreate(verify_task, "pair_verify", 8192, NULL, 2, NULL) != pdPASS) {
            working = false;
            state("failed", "Verification task memory unavailable");
            return ESP_ERR_NO_MEM;
        }
        return cyd_pair_encode(snapshot(), out, cap, size);
    }
    if (op == CYD_PAIR_BUNDLE_COMMIT) {
        if (!ready || working)
            return ESP_ERR_INVALID_STATE;
        ready = false;
        working = true;
        atomic_store(&installing, true);
        if (xTaskCreate(install_task, "pair_update", 8192, NULL, 2, NULL) != pdPASS) {
            atomic_store(&installing, false);
            working = false;
            ready = true;
            return ESP_ERR_NO_MEM;
        }
        return ESP_OK;
    }
#else
    (void)expected;
    (void)received;
    (void)upload_at;
    (void)candidate;
    (void)temp;
#endif
    return ESP_ERR_NOT_SUPPORTED;
}

void cyd_pair_update_tick(void) {
    if (!gate)
        return;
    uint64_t now = esp_timer_get_time() / 1000;
    if (upload && now - upload_at > 60000) {
        fclose(upload);
        upload = NULL;
        unlink(temp);
        state("failed", "Upload expired; neither board was selected for restart");
    }
#if CYD_PAIR_MANAGER
    cyd_ota_status_t s;
    cyd_ota_snapshot(&s);
    if (flash_at && now - flash_at > 120000 &&
        (s.stage == CYD_OS_UPLOADING || s.stage == CYD_OS_READY)) {
        if (s.stage == CYD_OS_READY)
            cyd_ota_cancel();
        else
            cyd_ota_abort("Paired staging expired; current firmware retained");
        flash_at = 0;
    }
#endif
}
