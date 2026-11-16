#include "cyd.h"
#include "cyd_peer.h"
#include "esp_heap_caps.h"
#include <assert.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static jmp_buf step;
static cyd_peer_request_t pending;
static bool have;
static uint8_t answer[248];
static size_t answer_size;
static unsigned dispatches, exchanges, drop_reply;
static uint64_t boot = 123;
static uint32_t random_value;
static bool fail_mutex, fail_service_task;
static unsigned fail_allocation, allocations;
static esp_err_t manager_result;
static bool startup_confirmed = true;
bool cyd_ota_boot_confirmed(void) { return startup_confirmed; }
static char startup_log[160];
static void *rpc_allocate(size_t n, unsigned caps) {
    assert(caps == (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    allocations++;
    return allocations == fail_allocation ? NULL : malloc(n);
}
void cyd_log(const char *tag, const char *message) {
    assert(!strcmp(tag, "pair"));
    snprintf(startup_log, sizeof(startup_log), "%s", message);
}
SemaphoreHandle_t xSemaphoreCreateMutex(void) {
    return fail_mutex ? NULL : (void *)1;
}
int xSemaphoreTake(SemaphoreHandle_t h, unsigned n) {
    (void)n;
    assert(h);
    return pdTRUE;
}
int xSemaphoreGive(SemaphoreHandle_t h) {
    assert(h);
    return pdTRUE;
}
void vSemaphoreDelete(SemaphoreHandle_t h) {
    (void)h;
}
int xTaskCreate(void (*f)(void *), const char *name, unsigned stack, void *arg, unsigned priority,
                void *handle) {
    (void)f;
    (void)name;
    (void)stack;
    (void)arg;
    (void)priority;
    (void)handle;
    return fail_service_task && !strcmp(name, "pair_services") ? 0 : pdPASS;
}
void vTaskDelay(unsigned n) {
    (void)n;
}
void vTaskDelete(void *p) {
    (void)p;
}
uint32_t esp_random(void) {
    return ++random_value;
}
int64_t esp_timer_get_time(void) {
    return 500000;
}
void cyd_pair_network_tick(void) {}
void cyd_pair_update_tick(void) {}
esp_err_t cyd_pair_manager_start(void) {
    return manager_result;
}
void cyd_pair_network_cache(const cJSON *o) {
    (void)o;
}
esp_err_t cyd_pair_dispatch(unsigned op, const void *in, size_t n, void *out, size_t cap,
                            size_t *size) {
    dispatches++;
    *size = 0;
    if (op == 2)
        return ESP_ERR_NOT_FOUND;
    if (op == 3)
        return ESP_OK;
    if (op == 4) {
        *size = cap + 1;
        return ESP_OK;
    }
    size_t wanted = n * 4;
    if (wanted > cap)
        return ESP_ERR_INVALID_SIZE;
    for (unsigned i = 0; i < 4; i++)
        memcpy((uint8_t *)out + n * i, in, n);
    *size = wanted;
    return ESP_OK;
}
bool cyd_peer_receive(cyd_peer_request_t *r, unsigned wait) {
    (void)wait;
    if (!have)
        longjmp(step, 1);
    *r = pending;
    have = false;
    return true;
}
bool cyd_peer_reply(const cyd_peer_request_t *r, const void *out, size_t n) {
    (void)r;
    assert(n <= sizeof(answer));
    memcpy(answer, out, n);
    answer_size = n;
    return true;
}
#undef heap_caps_malloc
#define heap_caps_malloc(n,c) rpc_allocate(n,c)
#include "../components/paired_system/rpc.c"
static void packet(const void *p, size_t n, uint64_t session) {
    assert(n <= 248);
    pending = (cyd_peer_request_t){.boot = session, .event = {.id = 1, .size = n}};
    memcpy(pending.event.data, p, n);
    have = true;
    answer_size = 0;
    if (!setjmp(step))
        serve(NULL);
    assert(answer_size >= 4);
}
esp_err_t cyd_peer_exchange(const void *p, size_t n, void *out, size_t cap, size_t *size,
                            unsigned wait) {
    (void)wait;
    exchanges++;
    packet(p, n, boot);
    if (exchanges == drop_reply)
        return ESP_ERR_TIMEOUT;
    assert(answer_size <= cap);
    memcpy(out, answer, answer_size);
    *size = answer_size;
    return ESP_OK;
}
static void reset(void) {
    free(request);
    free(response);
    request = response = NULL;
    serial = NULL;
    token = 0;
    dispatches = exchanges = drop_reply = 0;
    boot = 123;
    startup_confirmed = true;
    assert(cyd_pair_start() == ESP_OK);
}
int main(void) {
    const char *stages[] = {"RPC mutex", "request PSRAM", "response PSRAM", "RPC service task"};
    for (unsigned i = 0; i < 4; i++) {
        fail_mutex = i == 0;
        fail_allocation = i == 1 ? 1 : i == 2 ? 2 : 0;
        fail_service_task = i == 3;
        allocations = 0;
        assert(cyd_pair_start() == ESP_ERR_NO_MEM);
        assert(!serial && !request && !response && strstr(startup_log, stages[i]));
    }
    fail_mutex = fail_service_task = false;
    fail_allocation = 0;
    manager_result = ESP_ERR_INVALID_STATE;
    assert(cyd_pair_start() == ESP_ERR_INVALID_STATE); /* Preserve non-heap failure. */
    manager_result = ESP_OK;
    uint8_t *input = malloc(8192), *output = malloc(32769);
    assert(input && output);
    for (unsigned i = 0; i < 8192; i++)
        input[i] = (uint8_t)i;
    reset();
    size_t got = 0;
    startup_confirmed = false;
    assert(cyd_pair_rpc(1, input, 4, output, 32768, &got) == ESP_ERR_INVALID_STATE);
    assert(dispatches == 0 && got == 0 && exchanges == 0);
    uint8_t cold_begin[11] = {1}, cold_exec[5] = {3};
    w32(cold_begin + 1, 444); cold_begin[9] = 3;
    w32(cold_exec + 1, 444);
    packet(cold_begin, sizeof(cold_begin), boot);
    packet(cold_exec, sizeof(cold_exec), boot);
    assert(u32(answer) == ESP_OK && u32(answer + 4) == ESP_ERR_INVALID_STATE && !dispatches);
    startup_confirmed = true;
    packet(cold_exec, sizeof(cold_exec), boot);
    assert(u32(answer) == ESP_ERR_INVALID_ARG && !dispatches); /* No deferred mutation replay. */
    assert(cyd_pair_rpc(1, input, 8192, output, 32768, &got) == ESP_OK && got == 32768 &&
           dispatches == 1);
    for (unsigned i = 0; i < 4; i++)
        assert(!memcmp(output + 8192 * i, input, 8192));
    assert(!token);
    for (unsigned i = 0; i < 8192; i++)
        assert(request[i] == 0);
    for (unsigned i = 0; i < 32768; i++)
        assert(response[i] == 0);
    reset();
    assert(cyd_pair_rpc(1, input, 8193, output, 32768, &got) == ESP_ERR_INVALID_ARG &&
           exchanges == 0);
    reset();
    assert(cyd_pair_rpc(1, input, 8192, output, 100, &got) == ESP_ERR_INVALID_SIZE &&
           dispatches == 1);
    reset();
    assert(cyd_pair_rpc(2, input, 8, output, 32768, &got) == ESP_ERR_NOT_FOUND && dispatches == 1);
    reset();
    assert(cyd_pair_rpc(4, input, 8, output, 32768, &got) == ESP_ERR_INVALID_SIZE &&
           dispatches == 1);
    reset();
    drop_reply = 3;
    assert(cyd_pair_rpc(3, input, 8, output, 32768, &got) == ESP_ERR_TIMEOUT && dispatches == 1 &&
           exchanges == 3);
    /* A duplicate EXEC must not execute its mutation a second time. */
    uint8_t exec[5] = {3};
    w32(exec + 1, token);
    packet(exec, 5, boot);
    assert(u32(answer) == ESP_ERR_INVALID_ARG && dispatches == 1);
    reset();
    uint8_t begin[11] = {1};
    w32(begin + 1, 88);
    w32(begin + 5, 8);
    begin[9] = 3;
    packet(begin, 11, 123);
    assert(u32(answer) == ESP_OK);
    uint8_t chunk[17] = {2};
    w32(chunk + 1, 88);
    w32(chunk + 5, 0);
    packet(chunk, 17, 456);
    assert(u32(answer) != ESP_OK && received == 0);
    w32(chunk + 5, 1);
    packet(chunk, 17, 123);
    assert(u32(answer) != ESP_OK && received == 0);
    w32(chunk + 5, 0);
    packet(chunk, 17, 123);
    assert(u32(answer) == ESP_OK && received == 8);
    w32(exec + 1, 88);
    packet(exec, 5, 123);
    assert(u32(answer) == ESP_OK && dispatches == 1);
    packet(exec, 5, 123);
    assert(u32(answer) != ESP_OK && dispatches == 1);
    free(input);
    free(output);
    free(request);
    free(response);
    puts("PASS bounded RPC assembly, 32 KiB responses, sessions, offsets, uncertain EXEC and no "
         "replay");
    return 0;
}
