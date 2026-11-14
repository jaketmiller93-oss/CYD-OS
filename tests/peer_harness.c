#include "../components/device_link/peer.c"
#include "cyd.h"
#include "cyd_link.h"
#include <assert.h>
#include <setjmp.h>
int fake_tx = 4, fake_rx = 5;
static int failure, driver_calls, driver_deletes, mutex_deletes, loops;
static int64_t clock_ms;
static void (*task_entry)(void *);
static jmp_buf stop;
static cyd_link_t remote;
static unsigned char incoming[4096];
static size_t incoming_size;
SemaphoreHandle_t xSemaphoreCreateMutex(void) { return failure == 4 ? NULL : (void *)1; }
int xSemaphoreTake(SemaphoreHandle_t h, unsigned t) {
    (void)h;
    (void)t;
    return 1;
}
int xSemaphoreGive(SemaphoreHandle_t h) {
    (void)h;
    return 1;
}
void vSemaphoreDelete(SemaphoreHandle_t h) {
    (void)h;
    mutex_deletes++;
}
int xTaskCreate(void (*f)(void *), const char *n, unsigned z, void *p, unsigned priority, void *h) {
    (void)n;
    (void)z;
    (void)p;
    (void)priority;
    (void)h;
    if (failure == 5)
        return 0;
    task_entry = f;
    return pdPASS;
}
uint32_t esp_random(void) {
    static uint32_t n = 100;
    return ++n;
}
int64_t esp_timer_get_time(void) { return clock_ms * 1000; }
void cyd_log(const char *a, const char *b) {
    (void)a;
    (void)b;
}
#ifdef CONFIG_CYD_LINK_ENABLE
void vTaskDelay(unsigned ms) {
    clock_ms += ms;
    cyd_link_tick(&remote, clock_ms);
    uint8_t bytes[CYD_LINK_FRAME];
    size_t n;
    while (cyd_link_take_tx(&remote, bytes, sizeof(bytes), &n)) {
        assert(incoming_size + n <= sizeof(incoming));
        memcpy(incoming + incoming_size, bytes, n);
        incoming_size += n;
    }
    if (--loops == 0)
        longjmp(stop, 1);
}
esp_err_t uart_param_config(int p, const uart_config_t *c) {
    assert(p == UART_NUM_1 && c->baud_rate == 115200);
    driver_calls++;
    return failure == 1 ? ESP_FAIL : 0;
}
esp_err_t uart_set_pin(int p, int tx, int rx, int a, int b) {
    assert(p == 1 && tx == fake_tx && rx == fake_rx && a == -1 && b == -1);
    return failure == 2 ? ESP_FAIL : 0;
}
esp_err_t uart_driver_install(int p, int rx, int tx, int count, void *q, int flags) {
    assert(p == 1 && rx == 2048 && tx == 0 && !count && !q && !flags);
    return failure == 3 ? ESP_FAIL : 0;
}
esp_err_t uart_driver_delete(int p) {
    assert(p == 1);
    driver_deletes++;
    return 0;
}
int uart_read_bytes(int p, void *out, size_t cap, unsigned timeout) {
    assert(p == 1 && !timeout);
    size_t n = incoming_size < cap ? incoming_size : cap;
    if (n > 3)
        n = 3;
    memcpy(out, incoming, n);
    memmove(incoming, incoming + n, incoming_size - n);
    incoming_size -= n;
    return n;
}
int uart_tx_chars(int p, const char *bytes, uint32_t n) {
    assert(p == 1);
    if (clock_ms % 5 == 0)
        return 0;
    if (n > 7)
        n = 7;
    cyd_link_feed(&remote, bytes, n);
    return n;
}
#endif
void peer_reset(int fail, int tx, int rx) {
    failure = fail;
    driver_calls = driver_deletes = mutex_deletes = 0;
    clock_ms = 0;
    task_entry = NULL;
    incoming_size = 0;
    fake_tx = tx;
    fake_rx = rx;
#ifdef CONFIG_CYD_LINK_ENABLE
    running = false;
    guard = NULL;
    latency_ms = 0;
    last_result = -1;
#endif
}
int peer_start(void) { return cyd_peer_start(); }
int peer_counters(int which) {
    return which == 0 ? driver_calls : which == 1 ? driver_deletes : mutex_deletes;
}
int peer_status_json(char *out, size_t cap) {
    cJSON *s = cyd_peer_status();
    char *json = cJSON_PrintUnformatted(s);
    cJSON_Delete(s);
    if (!json)
        return 0;
    snprintf(out, cap, "%s", json);
    free(json);
    return 1;
}
int peer_run(void) {
#ifdef CONFIG_CYD_LINK_ENABLE
    assert(task_entry);
    cyd_link_init(&remote, role() == CYD_LINK_MAIN ? CYD_LINK_COMPANION : CYD_LINK_MAIN, 999);
    loops = 6000;
    if (!setjmp(stop))
        task_entry(NULL);
    return engine.ready && remote.ready && last_result == 0;
#else
    return 0;
#endif
}
