#pragma once
#include "cyd.h"
#include "cyd_link.h"
/* Bounded snapshots; no LVGL/JSON allocation while holding the UART mutex. */
typedef struct {
    bool configured, running, connected, remote_valid, services;
    cyd_link_role_t role;
    uint32_t age_ms, latency_ms, errors, drops, resets;
    uint64_t local_boot, remote_boot;
    int echo_result;
    const char *message;
    cyd_link_status_t remote;
} cyd_peer_info_t;
esp_err_t cyd_peer_start(void);
void cyd_peer_snapshot(cyd_peer_info_t *);
cJSON *cyd_peer_status(void);

/* Service transport. Each exchange has an explicit finite timeout and no retry. */
typedef struct { uint64_t boot; cyd_link_event_t event; } cyd_peer_request_t;
esp_err_t cyd_peer_exchange(const void *,size_t,void *,size_t,size_t *,unsigned);
bool cyd_peer_receive(cyd_peer_request_t *,unsigned wait_ms);
bool cyd_peer_reply(const cyd_peer_request_t *,const void *,size_t);
