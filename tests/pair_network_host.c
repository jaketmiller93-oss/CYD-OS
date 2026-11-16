#include "cyd.h"
#include "cyd_peer.h"
#include "cyd_services.h"
#include "cyd_pair_io.h"
#include <assert.h>
#include <setjmp.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static jmp_buf cycle;
static void (*worker)(void *);
static bool in_server, drop_write, connected = true;
static uint64_t manager_boot = 22, host_boot = 11;
static unsigned opens, closes, writes;
static uint8_t sent[100];
static size_t sent_size;
SemaphoreHandle_t xSemaphoreCreateMutex(void) {
    return (void *)1;
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
int xTaskCreate(void (*fn)(void *), const char *name, unsigned stack, void *arg, unsigned p,
                void *handle) {
    (void)name;
    (void)stack;
    (void)arg;
    (void)p;
    (void)handle;
    worker = fn;
    return pdPASS;
}
void vTaskDelay(unsigned n) {
    (void)n;
    longjmp(cycle, 1);
}
void vTaskDelete(void *p) {
    (void)p;
}
int64_t esp_timer_get_time(void) {
    return 1000000;
}
uint32_t esp_random(void) {
    static uint32_t n = 100;
    return ++n;
}
void cyd_peer_snapshot(cyd_peer_info_t *s) {
    memset(s, 0, sizeof(*s));
    s->connected = connected;
    s->local_boot = in_server ? manager_boot : host_boot;
    s->remote_boot = in_server ? host_boot : manager_boot;
}
typedef struct {
    bool read;
} local_t;
static void *open_local(const cyd_endpoint_t *ep, char *error, size_t n) {
    (void)error;
    (void)n;
    assert(!strcmp(ep->host, "example.test") && ep->port == 443 && ep->tls &&
           !strcmp(ep->verify_name, "example.test") && !strcmp(ep->ca, "test-ca"));
    opens++;
    return calloc(1, sizeof(local_t));
}
static int connect_local(void *p, char *error, size_t n) {
    (void)p;
    (void)error;
    (void)n;
    return 1;
}
static int read_local(void *p, void *out, size_t n) {
    local_t *t = p;
    if (t->read)
        return 0;
    assert(n >= 5);
    memcpy(out, "reply", 5);
    t->read = true;
    return 5;
}
static int write_local(void *p, const void *data, size_t n) {
    (void)p;
    writes++;
    if (n > 2)
        n = 2;
    assert(n <= sizeof(sent) - sent_size);
    memcpy(sent + sent_size, data, n);
    sent_size += n;
    return (int)n;
}
static void close_local(void *p) {
    closes++;
    free(p);
}
cyd_io_t cyd_services_esp_io(void) {
    return (cyd_io_t){open_local, connect_local, read_local, write_local, close_local};
}
#include "../components/paired_system/network.c"
esp_err_t cyd_pair_rpc(unsigned op, const void *in, size_t n, void *out, size_t cap, size_t *got) {
    in_server = true;
    esp_err_t e = cyd_pair_network(op, in, n, out, cap, got);
    in_server = false;
    if (op == CYD_PAIR_NET_WRITE && drop_write) {
        drop_write = false;
        return ESP_ERR_TIMEOUT;
    }
    return e;
}
#include "../components/paired_system/proxy_io.c"
static void step(void) {
    assert(worker);
    if (!setjmp(cycle))
        worker(NULL);
}
int main(void) {
    assert(!cyd_pair_io_start());
    cyd_io_t io = cyd_pair_io();
    cyd_endpoint_t ep = {.host = "example.test",
                         .verify_name = "example.test",
                         .ca = "test-ca",
                         .port = 443,
                         .tls = true};
    char error[100];
    void *p = io.open(&ep, error, sizeof(error));
    assert(p && opens == 0);
    assert(!io.connect(p, error, sizeof(error)));
    step();
    assert(opens == 1);
    step();
    assert(io.connect(p, error, sizeof(error)) == 1);
    step();
    char b[20];
    assert(io.read(p, b, 2) == 2 && !memcmp(b, "re", 2));
    assert(io.read(p, b, 20) == 3 && !memcmp(b, "ply", 3));
    assert(io.write(p, "abcdef", 6) == 6 && writes == 0);
    step();
    step();
    step();
    assert(writes == 3 && sent_size == 6 && !memcmp(sent, "abcdef", 6));
    drop_write = true;
    assert(io.write(p, "uncertain", 9) == 9);
    step();
    assert(writes == 4 && sent_size == 8);
    step();
    assert(closes == 1);
    assert(io.connect(p, error, sizeof(error)) < 0);
    /* A failed slot remains owned until close; it cannot alias a new app. */
    void *q = io.open(&ep, error, sizeof(error));
    assert(q && q != p);
    io.close(p);
    step();
    step();
    step();
    assert(opens == 2 && writes == 4);
    /* Host reset invalidates the C5 connection even if readiness already returned. */
    host_boot = 33;
    in_server = true;
    cyd_pair_network_tick();
    in_server = false;
    assert(closes == 2);
    io.close(q);
    step();
    p = io.open(&ep, error, sizeof(error));
    assert(p);
    step();
    step();
    assert(io.connect(p, error, sizeof(error)) == 1);
    manager_boot = 44;
    step();
    assert(io.connect(p, error, sizeof(error)) < 0);
    io.close(p);
    step();
    /* Three remote slots and three stable proxy owners are the hard limit. */
    void *owned[3];
    for (unsigned i = 0; i < 3; i++) {
        owned[i] = io.open(&ep, error, sizeof(error));
        assert(owned[i]);
    }
    assert(!io.open(&ep, error, sizeof(error)));
    step();
    for (unsigned i = 0; i < 3; i++)
        io.close(owned[i]);
    step();
    connected = false;
    in_server = true;
    cyd_pair_network_tick();
    in_server = false;
    free(slots);
    free(channels);
    free(request_buffer);
    free(reply_buffer);
    puts("PASS nonblocking IO callbacks, TLS fields, exact partial writes, uncertain send, owner "
         "lifetime, boot resets and slot limits");
    return 0;
}
