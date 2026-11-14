#include "cyd_link.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static cyd_link_t nodes[2];
int link_reset(unsigned node, int role, uint64_t boot) {
    return node < 2 && cyd_link_init(&nodes[node], role, boot);
}
void link_tick(unsigned node, uint64_t now) { cyd_link_tick(&nodes[node], now); }
void link_feed(unsigned node, const void *bytes, size_t n) {
    cyd_link_feed(&nodes[node], bytes, n);
}
size_t link_tx(unsigned node, void *bytes, size_t cap) {
    size_t n = 0;
    cyd_link_take_tx(&nodes[node], bytes, cap, &n);
    return n;
}
uint32_t link_ping(unsigned node, const void *data, size_t n, unsigned timeout) {
    return cyd_link_ping(&nodes[node], data, n, timeout);
}
int link_cancel(unsigned node, uint32_t id) { return cyd_link_cancel(&nodes[node], id); }
int link_event(unsigned node, cyd_link_event_t *out) { return cyd_link_pop(&nodes[node], out); }
unsigned link_ready(unsigned node) { return nodes[node].ready; }
unsigned link_bad(unsigned node) { return nodes[node].bad_frames; }
unsigned link_dropped(unsigned node) { return nodes[node].dropped_frames; }
unsigned link_pending(unsigned node) {
    unsigned n = 0;
    for (unsigned i = 0; i < 4; i++)
        n += nodes[node].pending[i].used;
    return n;
}
void link_serial(unsigned node, uint32_t n) { nodes[node].next_id = n; }
#ifdef LINK_FUZZ_MAIN
int main(void) {
    assert(link_reset(0, CYD_LINK_MAIN, 11));
    assert(link_reset(1, CYD_LINK_COMPANION, 22));
    unsigned x = 4311;
    for (unsigned step = 0; step < 50000; step++) {
        unsigned char bytes[512];
        for (unsigned i = 0; i < sizeof(bytes); i++) {
            x = x * 1664525 + 1013904223;
            bytes[i] = x >> 24;
        }
        link_feed(step % 2, bytes, step % sizeof(bytes));
        if (step % 3 == 0) {
            unsigned char zero = 0;
            link_feed(step % 2, &zero, 1);
        }
        link_tick(0, step);
        link_tick(1, step);
        for (unsigned direction = 0; direction < 2; direction++) {
            size_t n;
            while ((n = link_tx(direction, bytes, sizeof(bytes))))
                link_feed(1 - direction, bytes, n);
        }
        if (nodes[0].ready && step % 11 == 0)
            link_ping(0, bytes, 256, 100);
        cyd_link_event_t e;
        while (link_event(0, &e))
            assert(e.size <= 256);
        while (link_event(1, &e))
            assert(e.size <= 256);
        for (unsigned node = 0; node < 2; node++) {
            assert(nodes[node].tx_count <= 8 && nodes[node].event_count <= 4 &&
                   nodes[node].input_size <= CYD_LINK_FRAME);
        }
    }
    puts("50000 malformed/valid interleaved link iterations passed under ASan/UBSan");
}
#endif
