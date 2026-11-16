#include "cyd.h"
#include "cyd_ota.h"
#include "pair_internal.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
static void (*job)(void *);
static unsigned scenario, events[32], event_count, c5_written, local_written, polls;
static bool c5_ready, c5_restarted, host_restarted;
static uint64_t clock_ms;
static cyd_ota_status_t local_state;
static cyd_os_image_desc_t descriptor = {
    .layout_sha256 = "0000000000000000000000000000000000000000000000000000000000000000"};
SemaphoreHandle_t xSemaphoreCreateMutex(void) {
    return (void *)1;
}
int xSemaphoreTake(SemaphoreHandle_t h, unsigned wait) {
    (void)wait;
    assert(h);
    return pdTRUE;
}
int xSemaphoreGive(SemaphoreHandle_t h) {
    assert(h);
    return pdTRUE;
}
int xTaskCreate(void (*fn)(void *), const char *name, unsigned stack, void *arg, unsigned priority,
                void *handle) {
    (void)stack;
    (void)arg;
    (void)priority;
    (void)handle;
    if (scenario == 4 && !strcmp(name, "pair_update"))
        return 0;
    assert(!job);
    job = fn;
    return pdPASS;
}
void vTaskDelay(unsigned n) {
    clock_ms += n;
}
void vTaskDelete(void *p) {
    (void)p;
}
int64_t esp_timer_get_time(void) {
    return (int64_t)clock_ms * 1000;
}
void cyd_pair_elf_sha256(char out[65]) {
    memset(out, '1', 64);
    out[64] = 0;
}
const char *cyd_system_version(void) {
    return "0.5.0";
}
bool cyd_storage_ready(void) {
    return true;
}
uint64_t cyd_storage_free(void) {
    return 100000000;
}
bool cyd_maintenance_active(void) {
    return false;
}
esp_err_t cyd_runtime_stop(void) {
    events[event_count++] = 1;
    return 0;
}
void cyd_log(const char *a, const char *b) {
    (void)a;
    (void)b;
}
const cyd_os_image_desc_t *cyd_ota_image_descriptor(void) {
    return &descriptor;
}
void cyd_ota_snapshot(cyd_ota_status_t *s) {
    *s = local_state;
    s->boot_confirmed = true;
}
esp_err_t cyd_ota_begin(const uint8_t *p, size_t n, size_t total, char *error, size_t ec) {
    (void)error;
    (void)ec;
    assert(c5_ready && n == 512 && p[12] == 9 && total == 1024);
    events[event_count++] = 4;
    if (scenario == 2)
        return ESP_FAIL;
    local_state.stage = CYD_OS_UPLOADING;
    local_written = 0;
    return 0;
}
esp_err_t cyd_ota_write(const void *p, size_t n) {
    assert(p);
    local_written += (unsigned)n;
    return 0;
}
esp_err_t cyd_ota_finish(char *error, size_t ec) {
    (void)error;
    (void)ec;
    assert(local_written == 1024);
    events[event_count++] = 5;
    local_state.stage = CYD_OS_READY;
    return 0;
}
esp_err_t cyd_ota_cancel(void) {
    local_state.stage = CYD_OS_IDLE;
    return 0;
}
void cyd_ota_abort(const char *s) {
    (void)s;
    local_state.stage = CYD_OS_FAILED;
}
esp_err_t cyd_ota_reboot(void) {
    assert(c5_restarted && polls >= 3);
    host_restarted = true;
    events[event_count++] = 7;
    return 0;
}
esp_err_t cyd_pair_rpc(unsigned op, const void *in, size_t n, void *out, size_t cap, size_t *got) {
    (void)out;
    (void)cap;
    *got = 0;
    if (op == CYD_PAIR_UPDATE_BEGIN) {
        assert(n == 516 && ((const uint8_t *)in)[16] == 0x17);
        events[event_count++] = 2;
        c5_written = 0;
        return 0;
    }
    if (op == CYD_PAIR_UPDATE_WRITE) {
        uint32_t offset;
        assert(n > 4);
        memcpy(&offset, in, 4);
        assert(offset == c5_written);
        c5_written += (unsigned)n - 4;
        if (scenario == 1)
            return ESP_ERR_TIMEOUT;
        return 0;
    }
    if (op == CYD_PAIR_UPDATE_FINISH) {
        assert(c5_written == 1024);
        events[event_count++] = 3;
        c5_ready = true;
        return 0;
    }
    if (op == CYD_PAIR_UPDATE_REBOOT) {
        assert(c5_ready && local_state.stage == CYD_OS_READY);
        events[event_count++] = 6;
        c5_restarted = true;
        return 0;
    }
    if (op == CYD_PAIR_UPDATE_CANCEL) {
        c5_ready = false;
        events[event_count++] = 8;
        return 0;
    }
    assert(0);
    return ESP_FAIL;
}
cJSON *cyd_pair_json(unsigned op, const cJSON *request) {
    (void)request;
    assert(op == CYD_PAIR_STATUS);
    cJSON *o = cJSON_CreateObject();
    if (c5_restarted)
        polls++;
    cJSON_AddStringToObject(o, "firmware", "0.5.0");
    cJSON_AddBoolToObject(o, "boot_confirmed", true);
    cJSON_AddStringToObject(o, "boot_id",
                            c5_restarted && polls >= 3 ? "0000000000000002" : "0000000000000001");
    cJSON_AddStringToObject(
        o, "elf_sha256",
        c5_restarted && polls >= 3 && scenario != 3
            ? "4343434343434343434343434343434343434343434343434343434343434343"
            : "1111111111111111111111111111111111111111111111111111111111111111");
    return o;
}
#include "../components/paired_system/update.c"
static void run(void) {
    assert(job);
    void (*fn)(void *) = job;
    job = NULL;
    fn(NULL);
}
int main(int argc, char **argv) {
    assert(argc == 3);
    scenario = (unsigned)atoi(argv[1]);
    mkdir(CYD_ROOT, 0700);
    mkdir(CYD_ROOT "/system", 0700);
    FILE *f = fopen(argv[2], "rb");
    assert(f);
    assert(!fseek(f, 0, SEEK_END));
    long total = ftell(f);
    assert(total == 2560);
    rewind(f);
    uint8_t bytes[2560];
    assert(fread(bytes, 1, sizeof(bytes), f) == sizeof(bytes));
    fclose(f);
    if (scenario == 5)
        bytes[2500] ^= 1;
    uint32_t length = sizeof(bytes);
    size_t got = 0;
    uint8_t out[4096];
    assert(!cyd_pair_update(CYD_PAIR_BUNDLE_BEGIN, &length, 4, out, sizeof(out), &got));
    for (uint32_t offset = 0; offset < length;) {
        uint8_t block[2052];
        size_t take = length - offset;
        if (take > 2048)
            take = 2048;
        memcpy(block, &offset, 4);
        memcpy(block + 4, bytes + offset, take);
        assert(!cyd_pair_update(CYD_PAIR_BUNDLE_WRITE, block, take + 4, out, sizeof(out), &got));
        offset += (uint32_t)take;
    }
    assert(!cyd_pair_update(CYD_PAIR_BUNDLE_FINISH, NULL, 0, out, sizeof(out), &got));
    assert(!event_count);
    run();
    if (scenario == 5) {
        assert(!ready && !strcmp(phase, "failed") && !event_count);
        puts("PASS corrupt paired image rejected before either flash");
        return 0;
    }
    assert(ready && !strcmp(phase, "ready") && !event_count);
    assert(strlen(expected_host_elf) == 64 && !strncmp(expected_host_elf, "53535353", 8));
    esp_err_t e = cyd_pair_update(CYD_PAIR_BUNDLE_COMMIT, NULL, 0, out, sizeof(out), &got);
    if (scenario == 4) {
        assert(e == ESP_ERR_NO_MEM && ready && !working && !cyd_pair_installing());
        puts("PASS update task failure retains ready candidate and clears app gate");
        return 0;
    }
    assert(!e && cyd_pair_installing());
    run();
    if (!scenario) {
        assert(host_restarted && event_count == 7);
        const unsigned expected[] = {1, 2, 3, 4, 5, 6, 7};
        assert(!memcmp(events, expected, sizeof(expected)));
        puts("PASS verify both, C5 stage, S3 stage, fresh exact C5 confirmation, S3 reboot");
    } else {
        assert(!host_restarted && !working && !cyd_pair_installing() && !strcmp(phase, "failed"));
        if (scenario == 1)
            assert(c5_written == 1024 && event_count == 3 && events[2] == 8);
        if (scenario == 2)
            assert(!c5_ready);
        if (scenario == 3)
            assert(polls >= 120);
        puts("PASS failed/uncertain staging or wrong confirmed C5 build retains S3 boot selection");
    }
    return 0;
}
