#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define CYD_LINK_PAYLOAD 256
#define CYD_LINK_FRAME 282
#define CYD_LINK_WINDOW 4
#define CYD_LINK_QUEUE 8
#define CYD_LINK_PROTOCOL 1
#define CYD_LINK_DIAGNOSTICS 1u
/* Only diagnostic messages are enabled in protocol 1. No app/secret transport. */
typedef enum { CYD_LINK_MAIN = 1, CYD_LINK_COMPANION = 2 } cyd_link_role_t;
typedef enum {
    CYD_LINK_OK = 0,
    CYD_LINK_TIMEOUT = 1,
    CYD_LINK_PEER_LOST = 2,
    CYD_LINK_PEER_RESET = 3
} cyd_link_result_t;
typedef struct {
    uint32_t id;
    cyd_link_result_t result;
    size_t size;
    uint8_t data[CYD_LINK_PAYLOAD];
} cyd_link_event_t;
typedef struct {
    bool used, complete;
    uint32_t id;
    uint64_t deadline;
    cyd_link_event_t event;
} cyd_link_pending_t;
typedef struct {
    uint8_t bytes[CYD_LINK_FRAME];
    size_t size;
} cyd_link_frame_t;
typedef struct {
    cyd_link_role_t role;
    uint64_t boot, peer_boot, retired_boot, now, last_peer, next_hello;
    uint32_t next_id, peer_capabilities;
    bool ready, discard;
    uint8_t input[CYD_LINK_FRAME];
    size_t input_size;
    cyd_link_frame_t tx[CYD_LINK_QUEUE];
    unsigned tx_head, tx_count;
    cyd_link_pending_t pending[CYD_LINK_WINDOW];
    cyd_link_event_t events[CYD_LINK_WINDOW];
    unsigned event_head, event_count;
    uint32_t bad_frames, dropped_frames, peer_resets;
} cyd_link_t;
/* All engine operations must be serialized by the owner; engine allocates nothing. */
bool cyd_link_init(cyd_link_t *, cyd_link_role_t, uint64_t boot_nonce);
void cyd_link_tick(cyd_link_t *, uint64_t monotonic_ms);
void cyd_link_feed(cyd_link_t *, const void *, size_t);
bool cyd_link_take_tx(cyd_link_t *, void *, size_t, size_t *);
uint32_t cyd_link_ping(cyd_link_t *, const void *, size_t, unsigned timeout_ms);
bool cyd_link_cancel(cyd_link_t *, uint32_t id);
bool cyd_link_pop(cyd_link_t *, cyd_link_event_t *);
