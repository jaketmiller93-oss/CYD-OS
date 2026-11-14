#include "cyd_services.h"
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
_Static_assert(sizeof(cyd_services_t) <= 192 * 1024, "Service state allocation quota");
static uint32_t serial(cyd_services_t *s) { return s->serial == UINT32_MAX ? 0 : ++s->serial; }
static bool clean(const char *p, size_t n) {
    if (!p)
        return false;
    size_t z = strlen(p);
    if (z > n)
        return false;
    for (size_t i = 0; i < z; i++)
        if ((unsigned char)p[i] < 32 || (unsigned char)p[i] == 127)
            return false;
    return true;
}
bool cyd_endpoint_parse(const char *url, const char *ca, const char *name, cyd_endpoint_t *o) {
    if (!url || !o || strchr(url, ' ') || !clean(url, 768) || !clean(name ? name : "", 253) ||
        strlen(ca ? ca : "") > CYD_CA_MAX)
        return false;
    memset(o, 0, sizeof(*o));
    const char *p = strstr(url, "://");
    if (!p)
        return false;
    size_t scheme = p - url;
    bool mqtt =
        (scheme == 4 && !memcmp(url, "mqtt", 4)) || (scheme == 5 && !memcmp(url, "mqtts", 5));
    if (!mqtt &&
        !((scheme == 4 && !memcmp(url, "http", 4)) || (scheme == 5 && !memcmp(url, "https", 5))))
        return false;
    o->tls = scheme == 5;
    o->port = mqtt ? (o->tls ? 8883 : 1883) : (o->tls ? 443 : 80);
    p += 3;
    const char *end = p + strcspn(p, "/?#");
    size_t len = end - p;
    if (!len || len >= sizeof(o->host) || memchr(p, '@', len) || memchr(p, '[', len))
        return false;
    const char *colon = memchr(p, ':', len);
    if (colon) {
        unsigned long port = 0;
        if (colon + 1 == end)
            return false;
        for (const char *x = colon + 1; x < end; x++) {
            if (!isdigit((unsigned char)*x))
                return false;
            port = port * 10 + *x - '0';
            if (port > 65535)
                return false;
        }
        if (!port)
            return false;
        o->port = port;
        len = colon - p;
    }
    if (!len)
        return false;
    memcpy(o->host, p, len);
    for (size_t i = 0; i < len; i++)
        if (!(isalnum((unsigned char)o->host[i]) || o->host[i] == '.' || o->host[i] == '-'))
            return false;
    if (strchr(end, '#') || strlen(end) > 510 || (mqtt && *end && strcmp(end, "/")))
        return false;
    snprintf(o->path, sizeof(o->path), "%s%s", *end == '?' ? "/" : "", *end ? end : "/");
    snprintf(o->verify_name, sizeof(o->verify_name), "%s", name && *name ? name : o->host);
    snprintf(o->ca, sizeof(o->ca), "%s", ca ? ca : "");
    if (!o->tls && (*o->ca || (name && *name)))
        return false;
    return true;
}
void cyd_services_init(cyd_services_t *s, const cyd_io_t *io) {
    memset(s, 0, sizeof(*s));
    s->io = *io;
}
static void close_io(cyd_services_t *s, cyd_service_conn_t *c) {
    if (c->io) {
        s->io.close(c->io);
        c->io = NULL;
    }
}
void cyd_services_close(cyd_services_t *s) {
    s->closing = true;
    for (unsigned i = 0; i < CYD_CONNECTIONS_MAX; i++) {
        close_io(s, &s->conn[i]);
        memset(&s->conn[i], 0, sizeof(s->conn[i]));
    }
    memset(s->timers, 0, sizeof(s->timers));
    memset(s->events, 0, sizeof(s->events));
    s->event_count = 0;
}
static cyd_service_event_t *event(cyd_services_t *s, cyd_event_kind_t kind, unsigned slot,
                                  uint32_t gen) {
    if (s->closing || s->event_count == CYD_EVENTS_MAX ||
        ((kind == CYD_EV_TIMER || kind == CYD_EV_TOUCH || kind == CYD_EV_MESSAGE) &&
         s->event_count >= CYD_EVENTS_MAX - 2))
        return NULL;
    cyd_service_event_t *e = &s->events[(s->event_head + s->event_count++) % CYD_EVENTS_MAX];
    memset(e, 0, sizeof(*e));
    e->kind = kind;
    e->slot = slot;
    e->generation = gen;
    return e;
}
bool cyd_services_valid(cyd_services_t *s, const cyd_service_event_t *e) {
    if (s->closing)
        return false;
    if (e->kind == CYD_EV_TIMER)
        return e->slot < CYD_TIMERS_MAX && s->timers[e->slot].generation == e->generation;
    if (e->kind == CYD_EV_TOUCH)
        return true;
    return e->slot < CYD_CONNECTIONS_MAX &&
           (e->kind == CYD_EV_MESSAGE || e->kind == CYD_EV_STATE
                ? s->conn[e->slot].epoch
                : s->conn[e->slot].generation) == e->generation &&
           !s->conn[e->slot].cancelled;
}
bool cyd_services_pop(cyd_services_t *s, cyd_service_event_t *out) {
    while (s->event_count) {
        *out = s->events[s->event_head];
        s->event_head = (s->event_head + 1) % CYD_EVENTS_MAX;
        s->event_count--;
        if (out->kind == CYD_EV_TIMER && out->slot < CYD_TIMERS_MAX &&
            s->timers[out->slot].generation == out->generation)
            s->timers[out->slot].queued = false;
        if (cyd_services_valid(s, out))
            return true;
    }
    return false;
}
bool cyd_services_touch(cyd_services_t *s, unsigned slot, uint32_t gen) {
    if (slot % 3)
        for (unsigned i = 0; i < s->event_count; i++) {
            cyd_service_event_t *e = &s->events[(s->event_head + i) % CYD_EVENTS_MAX];
            if (e->kind == CYD_EV_TOUCH && e->slot == slot && e->generation == gen)
                return true;
        }
    return event(s, CYD_EV_TOUCH, slot, gen) != NULL;
}
int cyd_services_timer(cyd_services_t *s, unsigned ms, bool repeat, uint32_t *gen) {
    if (ms < 20 || ms > 86400000 || s->closing || s->serial == UINT32_MAX)
        return -1;
    for (unsigned i = 0; i < CYD_TIMERS_MAX; i++)
        if (!s->timers[i].active && !s->timers[i].queued) {
            cyd_service_timer_t *t = &s->timers[i];
            *t = (cyd_service_timer_t){.active = true,
                                       .repeat = repeat,
                                       .due = s->now + ms,
                                       .interval = ms,
                                       .generation = serial(s)};
            *gen = t->generation;
            return i;
        }
    return -1;
}
bool cyd_services_timer_cancel(cyd_services_t *s, unsigned n, uint32_t gen) {
    if (n >= CYD_TIMERS_MAX || s->timers[n].generation != gen)
        return false;
    memset(&s->timers[n], 0, sizeof(s->timers[n]));
    return true;
}
static void report(cyd_services_t *s, unsigned n, cyd_event_kind_t kind, int code,
                   const char *error) {
    cyd_service_conn_t *c = &s->conn[n];
    cyd_service_event_t *e = event(s, kind, n, c->mqtt ? c->epoch : c->generation);
    if (e) {
        e->status = code;
        snprintf(e->error, sizeof(e->error), "%s", error ? error : "");
        if (kind == CYD_EV_HTTP && code > 0) {
            e->size = c->body_len;
            memcpy(e->body, c->response, e->size);
        }
    }
}
static void failure(cyd_services_t *s, unsigned n, const char *why) {
    cyd_service_conn_t *c = &s->conn[n];
    close_io(s, c);
    c->connected = c->ready = false;
    c->rx_len = c->tx_count = c->tx_sent = c->tx_head = 0;
    c->packet_at = c->ping_at = c->write_at = 0;
    if (c->mqtt && !c->cancelled) {
        snprintf(c->error, sizeof(c->error), "%s", why);
        if (s->serial == UINT32_MAX) {
            c->cancelled = true;
            c->used = false;
            return;
        }
        c->epoch = serial(s);
        c->retry_at = s->now + c->backoff;
        c->backoff = c->backoff < 16000 ? c->backoff * 2 : 30000;
        report(s, n, CYD_EV_STATE, 0, why);
    } else {
        c->http_status = -1;
        c->body_len = 0;
        snprintf(c->error, sizeof(c->error), "%s", why);
        c->phase = 4;
    }
}
static bool queue_tx(cyd_service_conn_t *c, const void *p, size_t n) {
    if (c->tx_count == 2 || n > sizeof(c->tx[0]))
        return false;
    unsigned at = (c->tx_head + c->tx_count++) % 2;
    memcpy(c->tx[at], p, n);
    c->tx_len[at] = n;
    return true;
}
static bool headers_ok(const char *h) {
    if (!h || strlen(h) > CYD_HEADERS_MAX)
        return false;
    const char *p = h;
    while (*p) {
        const char *end = strstr(p, "\r\n"), *colon = strchr(p, ':');
        if (!end || !colon || colon >= end || colon == p)
            return false;
        size_t n = colon - p;
        for (const char *x = p; x < colon; x++)
            if (!(isalnum((unsigned char)*x) || *x == '-'))
                return false;
        const char *reserved[] = {"host", "content-length", "transfer-encoding", "connection"};
        for (unsigned i = 0; i < 4; i++)
            if (strlen(reserved[i]) == n && !strncasecmp(p, reserved[i], n))
                return false;
        for (const char *x = colon + 1; x < end; x++)
            if ((unsigned char)*x < 32 && *x != '\t')
                return false;
        p = end + 2;
    }
    return true;
}
int cyd_services_http(cyd_services_t *s, const cyd_endpoint_t *ep, bool post, const char *headers,
                      const void *body, size_t n, unsigned timeout, uint32_t *gen) {
    if (s->closing || s->serial == UINT32_MAX || s->conn[0].used || n > CYD_REQUEST_MAX ||
        (!post && n) || timeout < 100 || timeout > 30000 || !headers_ok(headers))
        return -1;
    cyd_service_conn_t *c = &s->conn[0];
    memset(c, 0, sizeof(*c));
    c->used = true;
    c->generation = serial(s);
    c->endpoint = *ep;
    c->deadline = s->now + timeout;
    c->remaining = -1;
    int k = snprintf((char *)c->tx[0], sizeof(c->tx[0]),
                     "%s %s HTTP/1.1\r\nHost: %s:%u\r\nConnection: "
                     "close\r\n%sContent-Length: %u\r\n\r\n",
                     post ? "POST" : "GET", ep->path, ep->host, ep->port, headers, (unsigned)n);
    if (k < 0 || (size_t)k + n > sizeof(c->tx[0])) {
        c->used = false;
        return -1;
    }
    if (n)
        memcpy(c->tx[0] + k, body, n);
    c->tx_len[0] = k + n;
    c->tx_count = 1;
    *gen = c->generation;
    return 0;
}
bool cyd_services_disconnect(cyd_services_t *s, unsigned n, uint32_t gen) {
    if (n >= CYD_CONNECTIONS_MAX || s->conn[n].generation != gen)
        return false;
    close_io(s, &s->conn[n]);
    memset(&s->conn[n], 0, sizeof(s->conn[n]));
    s->conn[n].cancelled = true;
    return true;
}
static bool http_header(cyd_service_conn_t *c, size_t length) {
    char h[2049];
    if (length > 2048)
        return false;
    memcpy(h, c->rx, length);
    h[length] = 0;
    char *line = strstr(h, "\r\n");
    if (!line)
        return false;
    *line = 0;
    if (strlen(h) < 13 || (strncmp(h, "HTTP/1.0 ", 9) && strncmp(h, "HTTP/1.1 ", 9)) ||
        h[12] != ' ' || h[9] < '2' || h[9] > '5' || !isdigit((unsigned char)h[10]) ||
        !isdigit((unsigned char)h[11]))
        return false;
    int status = (h[9] - '0') * 100 + (h[10] - '0') * 10 + h[11] - '0';
    c->http_status = status;
    bool cl = false, te = false;
    for (char *p = line + 2; *p;) {
        char *end = strstr(p, "\r\n");
        if (!end)
            return false;
        if (end == p)
            break;
        *end = 0;
        char *colon = strchr(p, ':');
        if (!colon)
            return false;
        *colon = 0;
        char *v = colon + 1;
        while (*v == ' ' || *v == '\t')
            v++;
        if (!strcasecmp(p, "content-length")) {
            if (cl || !*v)
                return false;
            cl = true;
            size_t amount = 0;
            for (char *x = v; *x; x++) {
                if (!isdigit((unsigned char)*x) || amount > CYD_HTTP_MAX)
                    return false;
                amount = amount * 10 + *x - '0';
            }
            if (amount > CYD_HTTP_MAX)
                return false;
            c->remaining = amount;
        }
        if (!strcasecmp(p, "transfer-encoding")) {
            if (te || strcasecmp(v, "chunked"))
                return false;
            te = true;
            c->chunked = true;
        }
        p = end + 2;
    }
    if (cl && te)
        return false;
    c->close_body = !cl && !te;
    if (status == 204 || status == 304) {
        c->remaining = 0;
        c->chunked = c->close_body = false;
    }
    return true;
}
/* Returns 1 complete, 0 needs bytes, -1 invalid/oversized. Strict bounded chunk
 * framing. */
static int http_parse(cyd_service_conn_t *c) {
    if (!c->header_done) {
        if (c->rx_len < 4)
            return 0;
        size_t i;
        for (i = 3; i < c->rx_len; i++)
            if (!memcmp(c->rx + i - 3, "\r\n\r\n", 4))
                break;
        if (i == c->rx_len)
            return c->rx_len >= 2048 ? -1 : 0;
        size_t len = i + 1;
        if (!http_header(c, len))
            return -1;
        memmove(c->rx, c->rx + len, c->rx_len - len);
        c->rx_len -= len;
        c->header_done = true;
    }
    if (!c->chunked) {
        size_t n = c->rx_len;
        if (!c->close_body && n > (size_t)c->remaining)
            return -1;
        if (n > CYD_HTTP_MAX - c->body_len)
            return -1;
        memcpy(c->response + c->body_len, c->rx, n);
        c->body_len += n;
        c->rx_len = 0;
        if (!c->close_body)
            c->remaining -= n;
        return !c->close_body && c->remaining == 0 ? 1 : 0;
    }
    while (c->rx_len) {
        if (c->chunk_phase == 0) {
            size_t i;
            for (i = 1; i < c->rx_len; i++)
                if (c->rx[i - 1] == '\r' && c->rx[i] == '\n')
                    break;
            if (i == c->rx_len)
                return c->rx_len > 10 ? -1 : 0;
            if (i == 1 || i > 8)
                return -1;
            size_t amount = 0;
            for (size_t j = 0; j < i - 1; j++) {
                int d = c->rx[j];
                d = d >= '0' && d <= '9'   ? d - '0'
                    : d >= 'a' && d <= 'f' ? d - 'a' + 10
                    : d >= 'A' && d <= 'F' ? d - 'A' + 10
                                           : -1;
                if (d < 0 || amount > CYD_HTTP_MAX)
                    return -1;
                amount = amount * 16 + d;
            }
            if (amount > CYD_HTTP_MAX - c->body_len)
                return -1;
            memmove(c->rx, c->rx + i + 1, c->rx_len - i - 1);
            c->rx_len -= i + 1;
            c->chunk_remaining = amount;
            c->chunk_phase = amount ? 1 : 3;
        } else if (c->chunk_phase == 1) {
            size_t n = c->rx_len < c->chunk_remaining ? c->rx_len : c->chunk_remaining;
            memcpy(c->response + c->body_len, c->rx, n);
            c->body_len += n;
            c->chunk_remaining -= n;
            memmove(c->rx, c->rx + n, c->rx_len - n);
            c->rx_len -= n;
            if (!c->chunk_remaining)
                c->chunk_phase = 2;
        } else {
            if (c->rx_len < 2)
                return 0;
            if (memcmp(c->rx, "\r\n", 2))
                return -1;
            memmove(c->rx, c->rx + 2, c->rx_len - 2);
            c->rx_len -= 2;
            if (c->chunk_phase == 3)
                return c->rx_len ? -1 : 1;
            c->chunk_phase = 0;
        }
    }
    return 0;
}
static bool topic_ok(const char *p, bool filter) {
    if (!clean(p, CYD_TOPIC_MAX) || !*p)
        return false;
    for (size_t i = 0; p[i]; i++) {
        if ((unsigned char)p[i] > 126)
            return false;
        if (p[i] == '#' && (!filter || p[i + 1] || (i && p[i - 1] != '/')))
            return false;
        if (p[i] == '+' && (!filter || (i && p[i - 1] != '/') || (p[i + 1] && p[i + 1] != '/')))
            return false;
    }
    return true;
}
static size_t mqtt_header(unsigned char *p, unsigned type, size_t len) {
    p[0] = type;
    size_t n = 1;
    do {
        unsigned b = len % 128;
        len /= 128;
        p[n++] = b | (len ? 128 : 0);
    } while (len);
    return n;
}
static size_t mqtt_string(unsigned char *p, const void *text, size_t n) {
    p[0] = n >> 8;
    p[1] = n;
    memcpy(p + 2, text, n);
    return n + 2;
}
static bool mqtt_packet(cyd_service_conn_t *c, unsigned type, const void *body, size_t n) {
    unsigned char p[CYD_MQTT_MAX + 260];
    size_t h = mqtt_header(p, type, n);
    if (h + n > sizeof(p))
        return false;
    memcpy(p + h, body, n);
    return queue_tx(c, p, h + n);
}
static bool subscribe_send(cyd_service_conn_t *c, const char *topic) {
    unsigned char p[134];
    uint16_t id = ++c->packet_id;
    if (!id)
        id = ++c->packet_id;
    p[0] = id >> 8;
    p[1] = id;
    size_t n = 2 + mqtt_string(p + 2, topic, strlen(topic));
    p[n++] = 0;
    return mqtt_packet(c, 0x82, p, n);
}
int cyd_services_mqtt(cyd_services_t *s, const cyd_endpoint_t *ep, const char *id, const char *user,
                      const char *password, uint32_t *gen) {
    if (s->closing || s->serial == UINT32_MAX || !clean(id, 64) || !*id || !clean(user, 128) ||
        !clean(password, 256) || (!*user && *password))
        return -1;
    for (unsigned n = 1; n < 3; n++)
        if (!s->conn[n].used) {
            cyd_service_conn_t *c = &s->conn[n];
            memset(c, 0, sizeof(*c));
            c->used = c->mqtt = true;
            c->endpoint = *ep;
            c->generation = c->epoch = serial(s);
            c->backoff = 1000;
            c->retry_at = s->now;
            snprintf(c->client_id, sizeof(c->client_id), "%s", id);
            snprintf(c->username, sizeof(c->username), "%s", user);
            snprintf(c->password, sizeof(c->password), "%s", password);
            *gen = c->generation;
            return n;
        }
    return -1;
}
bool cyd_services_subscribe(cyd_services_t *s, unsigned n, uint32_t gen, const char *topic) {
    if (n < 1 || n >= 3 || !topic_ok(topic, true))
        return false;
    cyd_service_conn_t *c = &s->conn[n];
    if (!c->used || c->generation != gen)
        return false;
    for (unsigned i = 0; i < c->subscriptions_count; i++)
        if (!strcmp(c->subscriptions[i], topic))
            return true;
    if (c->subscriptions_count == 8)
        return false;
    snprintf(c->subscriptions[c->subscriptions_count++], 129, "%s", topic);
    return true;
}
bool cyd_services_publish(cyd_services_t *s, unsigned n, uint32_t gen, const char *topic,
                          const void *body, size_t size, bool retain) {
    if (n < 1 || n >= 3 || size > CYD_MQTT_MAX || !topic_ok(topic, false))
        return false;
    cyd_service_conn_t *c = &s->conn[n];
    if (!c->used || !c->ready || c->generation != gen || c->restoring != c->subscriptions_count)
        return false;
    unsigned char packet[CYD_MQTT_MAX + 130];
    size_t len = mqtt_string(packet, topic, strlen(topic));
    memcpy(packet + len, body, size);
    return mqtt_packet(c, retain ? 0x31 : 0x30, packet, len + size);
}
static bool mqtt_connect_packet(cyd_service_conn_t *c) {
    unsigned char p[600] = {0, 4, 'M', 'Q', 'T', 'T', 4, 2, 0, 20};
    if (*c->username)
        p[7] |= 128;
    if (*c->password)
        p[7] |= 64;
    size_t n = 10;
    n += mqtt_string(p + n, c->client_id, strlen(c->client_id));
    if (*c->username)
        n += mqtt_string(p + n, c->username, strlen(c->username));
    if (*c->password)
        n += mqtt_string(p + n, c->password, strlen(c->password));
    return mqtt_packet(c, 0x10, p, n);
}
static int mqtt_process(cyd_services_t *s, unsigned n) {
    cyd_service_conn_t *c = &s->conn[n];
    while (c->rx_len >= 2) {
        unsigned char type = c->rx[0];
        size_t len = 0, mult = 1, h = 1;
        for (;;) {
            if (h >= c->rx_len)
                return 0;
            if (h > 4)
                return -1;
            unsigned b = c->rx[h++];
            len += (b & 127) * mult;
            if (!(b & 128))
                break;
            mult *= 128;
        }
        if (len + h > CYD_MQTT_MAX + 260 || h > 3 || (h == 3 && len < 128))
            return -1;
        if (c->rx_len < len + h)
            return 0;
        unsigned char *p = c->rx + h;
        c->last_rx = s->now;
        if (!c->ready) {
            if (type != 0x20 || len != 2 || p[0] != 0)
                return -1;
            if (p[1] != 0)
                return -2;
            c->ready = true;
            c->phase = 3;
            c->backoff = 1000;
            c->restoring = 0;
            c->error[0] = 0;
            report(s, n, CYD_EV_STATE, 1, "");
        } else if ((type & 0xf0) == 0x30) {
            unsigned qos = (type >> 1) & 3;
            if (qos > 1 || len < 2)
                return -1;
            size_t topic = (p[0] << 8) | p[1], off = topic + 2;
            if (!topic || topic > CYD_TOPIC_MAX || off + (qos ? 2 : 0) > len)
                return -1;
            char name[129];
            memcpy(name, p + 2, topic);
            name[topic] = 0;
            if (memchr(name, 0, topic) || !topic_ok(name, false))
                return -1;
            uint16_t id = 0;
            if (qos) {
                id = (p[off] << 8) | p[off + 1];
                off += 2;
                if (!id)
                    return -1;
            }
            size_t size = len - off;
            if (size > CYD_MQTT_MAX)
                return -1;
            cyd_service_event_t *e = event(s, CYD_EV_MESSAGE, n, c->epoch);
            if (e) {
                e->size = size;
                memcpy(e->body, p + off, size);
                snprintf(e->topic, sizeof(e->topic), "%s", name);
            } else {
                c->dropped_messages++;
                snprintf(c->error, sizeof(c->error),
                         "MQTT event queue full; whole message dropped");
            }
            if (qos) {
                unsigned char ack[2] = {id >> 8, id};
                if (!mqtt_packet(c, 0x40, ack, 2))
                    return -1;
            }
        } else if (type == 0xd0 && len == 0)
            c->ping_at = 0;
        else if (type == 0x90) {
            if (len != 3 || !((p[0] << 8) | p[1]) || p[2] != 0)
                return -3;
        } else
            return -1;
        memmove(c->rx, c->rx + h + len, c->rx_len - h - len);
        c->rx_len -= h + len;
        c->packet_at = c->rx_len ? s->now : 0;
    }
    return 0;
}
void cyd_services_tick(cyd_services_t *s, uint64_t now) {
    if (s->closing)
        return;
    s->now = now;
    unsigned timer_start = s->timer_cursor;
    for (unsigned offset = 0; offset < CYD_TIMERS_MAX; offset++) {
        unsigned i = (timer_start + offset) % CYD_TIMERS_MAX;
        cyd_service_timer_t *t = &s->timers[i];
        if (t->active && now >= t->due) {
            if (!t->queued && event(s, CYD_EV_TIMER, i, t->generation)) {
                t->queued = true;
                s->timer_cursor = (i + 1) % CYD_TIMERS_MAX;
                if (!t->repeat)
                    t->active = false;
            }
            if (t->repeat)
                t->due = now + t->interval;
        }
    }
    for (unsigned n = 0; n < 3; n++) {
        cyd_service_conn_t *c = &s->conn[n];
        if (!c->used)
            continue;
        if (!c->mqtt && c->phase == 4) {
            if (s->event_count < CYD_EVENTS_MAX) {
                report(s, n, CYD_EV_HTTP, c->http_status, c->error);
                c->used = false;
            }
            continue;
        }
        if (!c->mqtt && now >= c->deadline) {
            failure(s, n, "HTTP deadline exceeded");
            continue;
        }
        if (!c->io) {
            if (c->mqtt && now < c->retry_at)
                continue;
            char error[96] = "Connection allocation failed";
            c->io = s->io.open(&c->endpoint, error, sizeof(error));
            if (!c->io) {
                failure(s, n, error);
                continue;
            }
            c->connected = false;
            c->phase = 0;
            if (c->mqtt)
                c->deadline = now + 10000;
        }
        if (!c->connected) {
            char error[96] = "Connection/TLS validation failed";
            int status = s->io.connect(c->io, error, sizeof(error));
            if (status < 0) {
                failure(s, n, error);
                continue;
            }
            if (!status) {
                if (c->mqtt && now >= c->deadline)
                    failure(s, n, "MQTT connect deadline exceeded");
                continue;
            }
            c->connected = true;
            c->last_rx = now;
            if (c->mqtt) {
                c->phase = 2;
                if (!mqtt_connect_packet(c)) {
                    failure(s, n, "MQTT CONNECT overflow");
                    continue;
                }
            }
        }
        if (c->mqtt && !c->ready && now >= c->deadline) {
            failure(s, n, "MQTT CONNACK timeout");
            continue;
        }
        if (c->tx_count) {
            if (!c->write_at)
                c->write_at = now + 1;
            if (c->mqtt && now >= c->write_at + 5000) {
                failure(s, n, "MQTT write deadline exceeded");
                continue;
            }
            size_t remaining = c->tx_len[c->tx_head] - c->tx_sent;
            int sent = s->io.write(c->io, c->tx[c->tx_head] + c->tx_sent, remaining);
            if (sent < 0) {
                failure(s, n, "Network write failed");
                continue;
            }
            if ((size_t)sent > remaining) {
                failure(s, n, "Invalid transport write");
                continue;
            }
            c->tx_sent += sent;
            if (c->tx_sent == c->tx_len[c->tx_head]) {
                c->tx_head = (c->tx_head + 1) % 2;
                c->tx_count--;
                c->tx_sent = 0;
                c->write_at = 0;
            }
        }
        size_t room = (c->mqtt ? CYD_MQTT_MAX + 260 : sizeof(c->rx)) - c->rx_len;
        if (!room) {
            failure(s, n, "Receive limit exceeded");
            continue;
        }
        int got = s->io.read(c->io, c->rx + c->rx_len, room);
        if (got < 0) {
            if (!c->mqtt && c->header_done && c->close_body) {
                close_io(s, c);
                c->phase = 4;
            } else
                failure(s, n, "Peer closed or transport failed");
            continue;
        }
        if ((size_t)got > room) {
            failure(s, n, "Invalid transport read");
            continue;
        }
        if (got) {
            if (c->mqtt && !c->rx_len)
                c->packet_at = now;
            c->rx_len += got;
            int result = c->mqtt ? mqtt_process(s, n) : http_parse(c);
            if (result < 0) {
                failure(s, n,
                        result == -2   ? "MQTT CONNACK rejected"
                        : result == -3 ? "MQTT subscription rejected"
                                       : "Malformed, incomplete or oversized response");
                continue;
            }
            if (!c->mqtt && result == 1) {
                close_io(s, c);
                c->phase = 4;
                continue;
            }
        }
        if (c->mqtt && c->ready) {
            if (c->rx_len && now - c->packet_at >= 5000) {
                failure(s, n, "MQTT fragmented packet deadline exceeded");
                continue;
            }
            if (c->restoring < c->subscriptions_count && c->tx_count < 2) {
                if (subscribe_send(c, c->subscriptions[c->restoring]))
                    c->restoring++;
            }
            if (c->ping_at && now - c->ping_at >= 5000) {
                failure(s, n, "MQTT keepalive timeout");
                continue;
            }
            if (!c->ping_at && now - c->last_rx >= 10000) {
                if (mqtt_packet(c, 0xc0, "", 0))
                    c->ping_at = now;
            }
        }
    }
}
