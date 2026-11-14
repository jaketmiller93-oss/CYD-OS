#include "cyd_link.h"
#include <limits.h>
#include <string.h>
#define HEADER 18
#define RAW_MAX (HEADER + CYD_LINK_PAYLOAD + 4)
enum { HELLO = 1, HELLO_ACK = 2, PING = 3, PONG = 4 };
_Static_assert(sizeof(cyd_link_t) <= 8192, "Link engine native quota");
static uint32_t crc(const uint8_t *p, size_t n) {
    uint32_t c = ~0u;
    while (n--) {
        c ^= *p++;
        for (unsigned i = 0; i < 8; i++)
            c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1)));
    }
    return ~c;
}
static uint64_t get64(const uint8_t *p) {
    uint64_t n = 0;
    for (unsigned i = 0; i < 8; i++)
        n |= (uint64_t)p[i] << (i * 8);
    return n;
}
static uint32_t get32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static void put64(uint8_t *p, uint64_t n) {
    for (unsigned i = 0; i < 8; i++)
        p[i] = n >> (i * 8);
}
static void put32(uint8_t *p, uint32_t n) {
    for (unsigned i = 0; i < 4; i++)
        p[i] = n >> (i * 8);
}
static size_t encode(const uint8_t *p, size_t n, uint8_t *out) {
    size_t at = 1, code_at = 0;
    unsigned code = 1;
    for (size_t i = 0; i < n; i++) {
        if (!p[i]) {
            out[code_at] = code;
            code_at = at++;
            code = 1;
        } else {
            out[at++] = p[i];
            if (++code == 255) {
                out[code_at] = code;
                code_at = at++;
                code = 1;
            }
        }
    }
    out[code_at] = code;
    out[at++] = 0;
    return at;
}
static size_t decode(const uint8_t *p, size_t n, uint8_t *out) {
    size_t at = 0, i = 0;
    while (i < n) {
        unsigned code = p[i++];
        if (!code || code - 1 > n - i || at + code - 1 > RAW_MAX)
            return 0;
        for (unsigned k = 1; k < code; k++)
            out[at++] = p[i++];
        if (code != 255 && i < n) {
            if (at == RAW_MAX)
                return 0;
            out[at++] = 0;
        }
    }
    return at;
}
static bool emit(cyd_link_t *s, unsigned kind, uint32_t id, const void *data, size_t n) {
    if (n > CYD_LINK_PAYLOAD || s->tx_count == CYD_LINK_QUEUE) {
        s->dropped_frames++;
        return false;
    }
    uint8_t raw[RAW_MAX] = {CYD_LINK_PROTOCOL, kind, s->role, 0};
    put64(raw + 4, s->boot);
    put32(raw + 12, id);
    raw[16] = n;
    raw[17] = n >> 8;
    if (n)
        memcpy(raw + HEADER, data, n);
    put32(raw + HEADER + n, crc(raw, HEADER + n));
    cyd_link_frame_t *f = &s->tx[(s->tx_head + s->tx_count) % CYD_LINK_QUEUE];
    f->size = encode(raw, HEADER + n + 4, f->bytes);
    s->tx_count++;
    return true;
}
static void deliver(cyd_link_t *s) {
    for (unsigned i = 0; i < CYD_LINK_WINDOW && s->event_count < CYD_LINK_WINDOW; i++) {
        cyd_link_pending_t *p = &s->pending[i];
        if (!p->used || !p->complete)
            continue;
        s->events[(s->event_head + s->event_count) % CYD_LINK_WINDOW] = p->event;
        s->event_count++;
        memset(p, 0, sizeof(*p));
    }
}
static void lost(cyd_link_t *s, cyd_link_result_t why) {
    s->ready = false; /* Previously queued success results belong to the old peer session. */
    for (unsigned i = 0; i < s->event_count; i++) {
        cyd_link_event_t *e = &s->events[(s->event_head + i) % CYD_LINK_WINDOW];
        if (e->result == CYD_LINK_OK) {
            e->result = why;
            e->size = 0;
        }
    }
    for (unsigned i = 0; i < CYD_LINK_WINDOW; i++)
        if (s->pending[i].used) {
            s->pending[i].complete = true;
            s->pending[i].event = (cyd_link_event_t){.id = s->pending[i].id, .result = why};
        }
    deliver(s);
    s->tx_head = s->tx_count = 0;
}
bool cyd_link_init(cyd_link_t *s, cyd_link_role_t role, uint64_t boot) {
    if (!s || !boot || (role != CYD_LINK_MAIN && role != CYD_LINK_COMPANION))
        return false;
    memset(s, 0, sizeof(*s));
    s->role = role;
    s->boot = boot;
    return true;
}
void cyd_link_tick(cyd_link_t *s, uint64_t now) {
    if (now < s->now)
        return;
    s->now = now;
    if (s->ready && now - s->last_peer >= 3500) {
        lost(s, CYD_LINK_PEER_LOST);
        s->next_hello = now;
    }
    for (unsigned i = 0; i < CYD_LINK_WINDOW; i++) {
        cyd_link_pending_t *p = &s->pending[i];
        if (p->used && !p->complete && now >= p->deadline) {
            p->complete = true;
            p->event = (cyd_link_event_t){.id = p->id, .result = CYD_LINK_TIMEOUT};
        }
    }
    deliver(s);
    uint8_t caps[4];
    put32(caps, CYD_LINK_DIAGNOSTICS);
    if (now >= s->next_hello && emit(s, HELLO, 0, caps, 4))
        s->next_hello = now + (s->ready ? 1000 : 250);
}
static void receive(cyd_link_t *s, const uint8_t *r, size_t n) {
    if (n < HEADER + 4 || r[0] != CYD_LINK_PROTOCOL || r[2] == s->role ||
        (r[2] != CYD_LINK_MAIN && r[2] != CYD_LINK_COMPANION) || r[3])
        goto bad;
    size_t size = r[16] | (unsigned)r[17] << 8;
    if (size > CYD_LINK_PAYLOAD || size + HEADER + 4 != n || get32(r + n - 4) != crc(r, n - 4))
        goto bad;
    uint64_t boot = get64(r + 4);
    uint32_t id = get32(r + 12);
    unsigned kind = r[1];
    if (!boot || boot == s->boot)
        goto bad;
    if (kind == HELLO) {
        if (size != 4 || id || boot == s->retired_boot ||
            !(get32(r + HEADER) & CYD_LINK_DIAGNOSTICS))
            goto bad;
        if (s->peer_boot != boot) {
            if (s->peer_boot) {
                s->retired_boot = s->peer_boot;
                s->peer_resets++;
                lost(s, CYD_LINK_PEER_RESET);
            }
            s->peer_boot = boot;
            s->next_hello = s->now;
        }
        s->peer_capabilities = get32(r + HEADER) & CYD_LINK_DIAGNOSTICS;
        uint8_t echo[12];
        put64(echo, boot);
        put32(echo + 8, CYD_LINK_DIAGNOSTICS);
        emit(s, HELLO_ACK, 0, echo, 12);
        return;
    }
    if (kind == HELLO_ACK) {
        if (boot == s->retired_boot || id || size != 12 || get64(r + HEADER) != s->boot ||
            !(get32(r + HEADER + 8) & CYD_LINK_DIAGNOSTICS))
            goto bad;
        /* An ACK echoes our unpredictable boot nonce and can arrive before the
         * peer's HELLO. Learn its session only after validating the entire ACK. */
        if (s->peer_boot != boot) {
            if (s->peer_boot) {
                s->retired_boot = s->peer_boot;
                s->peer_resets++;
                lost(s, CYD_LINK_PEER_RESET);
            }
            s->peer_boot = boot;
            s->next_hello = s->now;
        }
        s->peer_capabilities = get32(r + HEADER + 8) & CYD_LINK_DIAGNOSTICS;
        s->last_peer = s->now;
        s->ready = true;
        return;
    }
    if (boot != s->peer_boot || !s->ready)
        return;
    if (kind == PING) {
        if (!id)
            goto bad;
        emit(s, PONG, id, r + HEADER, size);
        return;
    }
    if (kind == PONG) {
        if (!id)
            goto bad;
        for (unsigned i = 0; i < CYD_LINK_WINDOW; i++) {
            cyd_link_pending_t *p = &s->pending[i];
            if (p->used && !p->complete && p->id == id) {
                p->complete = true;
                p->event = (cyd_link_event_t){.id = id, .result = CYD_LINK_OK, .size = size};
                memcpy(p->event.data, r + HEADER, size);
                deliver(s);
                return;
            }
        }
        return;
    }
bad:
    s->bad_frames++;
}
void cyd_link_feed(cyd_link_t *s, const void *data, size_t n) {
    const uint8_t *p = data;
    for (size_t i = 0; i < n; i++) {
        uint8_t b = p[i];
        if (!b) {
            if (!s->discard && s->input_size) {
                uint8_t raw[RAW_MAX];
                size_t count = decode(s->input, s->input_size, raw);
                if (count)
                    receive(s, raw, count);
                else
                    s->bad_frames++;
            }
            s->discard = false;
            s->input_size = 0;
        } else if (!s->discard) {
            if (s->input_size == sizeof(s->input)) {
                s->bad_frames++;
                s->discard = true;
                s->input_size = 0;
            } else
                s->input[s->input_size++] = b;
        }
    }
}
bool cyd_link_take_tx(cyd_link_t *s, void *out, size_t cap, size_t *n) {
    if (!s->tx_count)
        return false;
    cyd_link_frame_t *f = &s->tx[s->tx_head];
    if (cap < f->size)
        return false;
    memcpy(out, f->bytes, f->size);
    *n = f->size;
    s->tx_head = (s->tx_head + 1) % CYD_LINK_QUEUE;
    s->tx_count--;
    return true;
}
uint32_t cyd_link_ping(cyd_link_t *s, const void *data, size_t n, unsigned timeout) {
    if (!s->ready || n > CYD_LINK_PAYLOAD || (n && !data) || timeout < 100 || timeout > 5000 ||
        s->next_id == UINT32_MAX)
        return 0;
    for (unsigned i = 0; i < CYD_LINK_WINDOW; i++) {
        cyd_link_pending_t *p = &s->pending[i];
        if (p->used)
            continue;
        uint32_t id = s->next_id + 1;
        if (!emit(s, PING, id, data, n))
            return 0;
        s->next_id = id;
        *p = (cyd_link_pending_t){.used = true, .id = id, .deadline = s->now + timeout};
        return id;
    }
    return 0;
}
bool cyd_link_cancel(cyd_link_t *s, uint32_t id) {
    bool found = false;
    for (unsigned i = 0; i < CYD_LINK_WINDOW; i++)
        if (s->pending[i].used && s->pending[i].id == id) {
            memset(&s->pending[i], 0, sizeof(s->pending[i]));
            found = true;
        }
    cyd_link_event_t kept[CYD_LINK_WINDOW];
    unsigned count = 0;
    while (s->event_count) {
        cyd_link_event_t e = s->events[s->event_head];
        s->event_head = (s->event_head + 1) % CYD_LINK_WINDOW;
        s->event_count--;
        if (e.id == id)
            found = true;
        else
            kept[count++] = e;
    }
    s->event_head = 0;
    s->event_count = count;
    for (unsigned i = 0; i < count; i++)
        s->events[i] = kept[i];
    return found;
}
bool cyd_link_pop(cyd_link_t *s, cyd_link_event_t *out) {
    deliver(s);
    if (!s->event_count)
        return false;
    *out = s->events[s->event_head];
    s->event_head = (s->event_head + 1) % CYD_LINK_WINDOW;
    s->event_count--;
    deliver(s);
    return true;
}
