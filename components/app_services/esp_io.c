#include "cyd_services.h"
#include "esp_crt_bundle.h"
#include "esp_tls.h"
#include "lwip/dns.h"
#include "lwip/tcpip.h"
#include <errno.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Stable OS slots outlive cancelled apps. Never free a lwIP DNS callback
 * argument. */
typedef struct {
    atomic_int state;
    atomic_bool owned;
    char host[254];
    ip_addr_t address;
    bool found;
} dns_slot_t;
static dns_slot_t dns_slots[4];
typedef struct {
    dns_slot_t *dns;
    esp_tls_t *tls;
    esp_tls_cfg_t config;
    const cyd_endpoint_t *endpoint;
    char address[48];
    bool started;
} transport_t;
static void resolved(const char *name, const ip_addr_t *ip, void *arg) {
    (void)name;
    dns_slot_t *d = arg;
    d->found = ip != NULL;
    if (ip)
        d->address = *ip;
    atomic_store_explicit(&d->state, 2, memory_order_release);
}
static void resolve_on_tcpip(void *arg) {
    dns_slot_t *d = arg;
    ip_addr_t ip;
    err_t e = dns_gethostbyname_addrtype(d->host, &ip, resolved, d, LWIP_DNS_ADDRTYPE_IPV4);
    if (e == ERR_OK)
        resolved(d->host, &ip, d);
    else if (e != ERR_INPROGRESS)
        resolved(d->host, NULL, d);
}
static void *open_transport(const cyd_endpoint_t *ep, char *error, size_t n) {
    transport_t *t = calloc(1, sizeof(*t));
    if (!t)
        return NULL;
    for (unsigned i = 0; i < 4; i++) {
        dns_slot_t *d = &dns_slots[i];
        if (atomic_load(&d->owned) || atomic_load_explicit(&d->state, memory_order_acquire) == 1)
            continue;
        bool expected = false;
        if (!atomic_compare_exchange_strong(&d->owned, &expected, true))
            continue;
        t->dns = d;
        snprintf(d->host, sizeof(d->host), "%s", ep->host);
        d->found = false;
        atomic_store(&d->state, 1);
        if (tcpip_callback(resolve_on_tcpip, d) != ERR_OK) {
            atomic_store(&d->state, 0);
            atomic_store(&d->owned, false);
            t->dns = NULL;
        }
        break;
    }
    if (!t->dns) {
        snprintf(error, n, "DNS slots busy; retry later");
        free(t);
        return NULL;
    }
    t->endpoint = ep;
    return t;
}
static int connect_transport(void *ctx, char *error, size_t n) {
    transport_t *t = ctx;
    if (!t->started) {
        if (atomic_load_explicit(&t->dns->state, memory_order_acquire) != 2)
            return 0;
        if (!t->dns->found) {
            snprintf(error, n, "DNS lookup failed");
            return -1;
        }
        if (!ipaddr_ntoa_r(&t->dns->address, t->address, sizeof(t->address)))
            return -1;
        t->tls = esp_tls_init();
        if (!t->tls)
            return -1;
        t->config = (esp_tls_cfg_t){.non_block = true,
                                    .timeout_ms = 100,
                                    .is_plain_tcp = !t->endpoint->tls,
                                    .common_name = t->endpoint->verify_name};
        if (t->endpoint->tls) {
            if (*t->endpoint->ca) {
                t->config.cacert_buf = (const unsigned char *)t->endpoint->ca;
                t->config.cacert_bytes = strlen(t->endpoint->ca) + 1;
            } else
                t->config.crt_bundle_attach = esp_crt_bundle_attach;
        }
        t->started = true;
    }
    int e = esp_tls_conn_new_async(t->address, strlen(t->address), t->endpoint->port, &t->config,
                                   t->tls);
    if (e < 0)
        snprintf(error, n, "TCP/TLS connection or certificate verification failed");
    return e;
}
static int read_transport(void *ctx, void *data, size_t size) {
    transport_t *t = ctx;
    ssize_t n = esp_tls_conn_read(t->tls, data, size);
    if (n == ESP_TLS_ERR_SSL_WANT_READ || n == ESP_TLS_ERR_SSL_WANT_WRITE ||
        (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)))
        return 0;
    return n <= 0 ? -1 : (int)n;
}
static int write_transport(void *ctx, const void *data, size_t size) {
    transport_t *t = ctx;
    ssize_t n = esp_tls_conn_write(t->tls, data, size);
    if (n == ESP_TLS_ERR_SSL_WANT_READ || n == ESP_TLS_ERR_SSL_WANT_WRITE ||
        (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)))
        return 0;
    return n <= 0 ? -1 : (int)n;
}
static void close_transport(void *ctx) {
    transport_t *t = ctx;
    if (t->tls)
        esp_tls_conn_destroy(t->tls);
    if (t->dns)
        atomic_store_explicit(&t->dns->owned, false, memory_order_release);
    free(t);
}
cyd_io_t cyd_services_esp_io(void) {
    return (cyd_io_t){open_transport, connect_transport, read_transport, write_transport,
                      close_transport};
}
