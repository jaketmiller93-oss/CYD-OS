#include "../components/device_link/peer.c"
#include "cyd.h"
#include "cyd_link.h"
#include <assert.h>
#include <setjmp.h>
int fake_tx = 4, fake_rx = 5;
static int failure, driver_calls, driver_deletes, mutex_deletes, loops;
static int heap_calls, heap_frees, mutex_creations;
static int64_t clock_ms;
static void (*task_entry)(void *);
static jmp_buf stop;
static cyd_link_t remote;
static unsigned char incoming[4096];
static size_t incoming_size;
SemaphoreHandle_t xSemaphoreCreateMutex(void) {mutex_creations++;return failure==4||(failure==9&&mutex_creations==2)?NULL:(void *)1;}
#ifdef CONFIG_CYD_PAIRED_SYSTEM
SemaphoreHandle_t xSemaphoreCreateBinary(void){return failure==8?NULL:(void *)2;}
struct host_queue {unsigned head,count,item;unsigned char bytes[2*sizeof(cyd_peer_request_t)];};
static struct host_queue rpc_queue;
QueueHandle_t xQueueCreate(unsigned capacity,size_t item){assert(capacity==2&&item==sizeof(cyd_peer_request_t));memset(&rpc_queue,0,sizeof(rpc_queue));rpc_queue.item=item;return failure==7?NULL:&rpc_queue;}
int xQueueSend(QueueHandle_t q,const void *p,unsigned wait){(void)wait;if(q->count==2)return 0;memcpy(q->bytes+((q->head+q->count)%2)*q->item,p,q->item);q->count++;return 1;}
int xQueueReceive(QueueHandle_t q,void *p,unsigned wait){(void)wait;if(!q->count)return 0;memcpy(p,q->bytes+q->head*q->item,q->item);q->head=(q->head+1)%2;q->count--;return 1;}
void vQueueDelete(QueueHandle_t q){q->count=0;}
#endif
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
const char *cyd_system_version(void) { return "0.4.1-pair.1"; }
bool cyd_storage_ready(void) { return true; }
void cyd_network_address(char *out, size_t n) { snprintf(out, n, "192.168.4.1"); }
uint32_t esp_get_free_heap_size(void) { return 123456; }
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
void *heap_caps_calloc(size_t n, size_t size, unsigned caps) {
    assert(n == 1 && size == sizeof(cyd_link_t) && size <= 8192);
    assert(caps == (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    heap_calls++;
    return failure == 6 ? NULL : calloc(n, size);
}
void heap_caps_free(void *p) {
    if (p) heap_frees++;
    free(p);
}
void vTaskDelay(unsigned ms) {
    clock_ms += ms;
    cyd_link_tick(&remote, clock_ms);
    if (clock_ms % 1000 == 0) {
        cyd_link_status_t health = {.uptime_ms = clock_ms, .free_heap = 234567,
                                    .sd_ready = true, .version = "remote-fixture"};
        cyd_link_send_status(&remote, &health);
    }
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
#ifdef CONFIG_CYD_LINK_ENABLE
    free(engine);
    engine = NULL;
#endif
    failure = fail;mutex_creations=0;
    heap_calls = heap_frees = 0;
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
    return which == 0 ? driver_calls : which == 1 ? driver_deletes : which == 2 ? mutex_deletes : which == 3 ? heap_calls : heap_frees;
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
    cyd_link_status_t local;
    return engine->ready && remote.ready && last_result == 0 &&
        cyd_link_get_status(&remote, &local, NULL) && local.free_heap == 123456 &&
        local.sd_ready && local.network_address && !strcmp(local.version, "0.4.1-pair.1");
#else
    return 0;
#endif
}

#ifdef CONFIG_CYD_PAIRED_SYSTEM
int peer_queued_epoch_case(void) {
    peer_reset(0,5,4);assert(cyd_peer_start()==ESP_OK);
    cyd_peer_request_t r={.boot=10,.event={.id=1}},got;
    engine->ready=true;engine->peer_boot=11;
    assert(xQueueSend(requests,&r,0));assert(!cyd_peer_receive(&got,0));
    r.boot=11;assert(xQueueSend(requests,&r,0));assert(cyd_peer_receive(&got,0)&&got.boot==11);
    engine->ready=false;assert(xQueueSend(requests,&r,0));assert(!cyd_peer_receive(&got,0));
    engine->ready=true;engine->peer_boot=12;
    assert(xQueueSend(requests,&r,0));assert(!cyd_peer_receive(&got,0));assert(!cyd_peer_reply(&r,"old",3));
    r.boot=12;assert(xQueueSend(requests,&r,0));assert(cyd_peer_receive(&got,0));
    peer_reset(0,5,4);return 1;
}
int peer_rpc_failure_case(int fail) {
    peer_reset(fail,5,4);assert(cyd_peer_start()!=ESP_OK);assert(!running&&!engine&&driver_deletes==1&&heap_frees==1);
    peer_reset(0,5,4);return 1;
}
#endif
