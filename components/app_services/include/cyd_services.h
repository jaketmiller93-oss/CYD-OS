#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define CYD_HTTP_MAX 8192
#define CYD_REQUEST_MAX 4096
#define CYD_MQTT_MAX 2048
#define CYD_TOPIC_MAX 128
#define CYD_EVENTS_MAX 8
#define CYD_TIMERS_MAX 16
#define CYD_CONNECTIONS_MAX 3
#define CYD_CA_MAX 4096
#define CYD_HEADERS_MAX 768
/* All engine calls are serialized by the owner. IO callbacks MUST be
 * nonblocking. */
typedef struct {
    char host[254], path[512], verify_name[254], ca[CYD_CA_MAX + 1];
    unsigned port;
    bool tls;
} cyd_endpoint_t;
typedef struct {
    void *(*open)(const cyd_endpoint_t *, char *, size_t);
    int (*connect)(void *, char *, size_t); /* 1 ready, 0 pending, -1 failed */
    int (*read)(void *, void *, size_t);    /* 0 pending, -1 closed/error, >0 bytes */
    int (*write)(void *, const void *, size_t);
    void (*close)(void *);
} cyd_io_t;
typedef enum {
    CYD_EV_HTTP,
    CYD_EV_MESSAGE,
    CYD_EV_STATE,
    CYD_EV_TIMER,
    CYD_EV_TOUCH
} cyd_event_kind_t;
typedef struct {
    cyd_event_kind_t kind;
    uint32_t generation;
    unsigned slot;
    int status;
    size_t size;
    char topic[129], error[96];
    unsigned char body[CYD_HTTP_MAX + 1];
} cyd_service_event_t;
typedef struct {
    bool active, repeat, queued;
    uint32_t generation;
    uint64_t due;
    unsigned interval;
} cyd_service_timer_t;
typedef struct {
    bool used, mqtt, ready, cancelled, connected;
    uint32_t generation, epoch;
    char error[96];
    unsigned phase, backoff, restoring, dropped_messages;
    uint64_t deadline, retry_at, last_rx, ping_at, packet_at, write_at;
    cyd_endpoint_t endpoint;
    void *io;
    char username[129], password[257], client_id[65], subscriptions[8][129];
    unsigned subscriptions_count;
    uint16_t packet_id;
    unsigned char tx[2][CYD_REQUEST_MAX + 1024], rx[CYD_HTTP_MAX + 2048];
    size_t tx_len[2], tx_sent;
    unsigned tx_head, tx_count;
    size_t rx_len;
    bool header_done, chunked, close_body;
    int http_status;
    int64_t remaining;
    size_t body_len, chunk_remaining;
    unsigned chunk_phase;
    char response[CYD_HTTP_MAX + 1];
} cyd_service_conn_t;
typedef struct {
    cyd_io_t io;
    cyd_service_conn_t conn[CYD_CONNECTIONS_MAX];
    cyd_service_timer_t timers[CYD_TIMERS_MAX];
    cyd_service_event_t events[CYD_EVENTS_MAX];
    unsigned event_head, event_count, timer_cursor;
    uint32_t serial;
    bool closing;
    uint64_t now;
} cyd_services_t;
bool cyd_endpoint_parse(const char *, const char *, const char *, cyd_endpoint_t *);
void cyd_services_init(cyd_services_t *, const cyd_io_t *);
void cyd_services_close(cyd_services_t *);
void cyd_services_tick(cyd_services_t *, uint64_t);
bool cyd_services_pop(cyd_services_t *, cyd_service_event_t *);
bool cyd_services_valid(cyd_services_t *, const cyd_service_event_t *);
bool cyd_services_touch(cyd_services_t *, unsigned, uint32_t);
int cyd_services_timer(cyd_services_t *, unsigned, bool, uint32_t *);
bool cyd_services_timer_cancel(cyd_services_t *, unsigned, uint32_t);
int cyd_services_http(cyd_services_t *, const cyd_endpoint_t *, bool, const char *, const void *,
                      size_t, unsigned, uint32_t *);
int cyd_services_mqtt(cyd_services_t *, const cyd_endpoint_t *, const char *, const char *,
                      const char *, uint32_t *);
bool cyd_services_disconnect(cyd_services_t *, unsigned, uint32_t);
bool cyd_services_subscribe(cyd_services_t *, unsigned, uint32_t, const char *);
bool cyd_services_publish(cyd_services_t *, unsigned, uint32_t, const char *, const void *, size_t,
                          bool);
/* ESP adapter creates no application-owned background tasks. */
cyd_io_t cyd_services_esp_io(void);
