/* Included after the local authenticated handlers. C5 is the radio ingress;
 * the S3 owns app installation and the paired firmware coordinator. */
static esp_err_t pair_result(httpd_req_t *r, unsigned op, const void *p, size_t n) {
    char *out = heap_caps_malloc(CYD_PAIR_RESPONSE_MAX + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!out)
        return failure(r, ESP_ERR_NO_MEM, "Response memory unavailable");
    size_t got = 0;
    esp_err_t e = cyd_pair_rpc(op, p, n, out, CYD_PAIR_RESPONSE_MAX, &got);
    if (e) {
        free(out);
        return failure(r, e,
                       "S3 service unavailable or request rejected; check both boards and SD");
    }
    if (!got) {
        free(out);
        return json(r, cJSON_CreateObject());
    }
    out[got] = 0;
    httpd_resp_set_type(r, "application/json");
    httpd_resp_set_hdr(r, "Cache-Control", "no-store");
    e = httpd_resp_send(r, out, got);
    free(out);
    return e;
}
static esp_err_t pair_small(httpd_req_t *r, unsigned op, cJSON *o) {
    char *text = o ? cJSON_PrintUnformatted(o) : NULL;
    cJSON_Delete(o);
    esp_err_t e = pair_result(r, op, text, text ? strlen(text) : 0);
    free(text);
    return e;
}
static esp_err_t pair_upload(httpd_req_t *r, bool package, bool inspect) {
    size_t max = package ? CYD_MAX_PACKAGE : CYD_BUNDLE_LIMIT;
    if (r->content_len < (package ? 22 : 1536) || r->content_len > max)
        return failure(r, ESP_ERR_INVALID_SIZE,
                       package ? "Package limit 512 KiB"
                               : "Choose one paired .cydos update bundle");
    uint8_t prefix[CYD_BUNDLE_HEADER];
    size_t initial = 0;
    if (!package) {
        while (initial < sizeof(prefix)) {
            int got = httpd_req_recv(r, (char *)prefix + initial, sizeof(prefix) - initial);
            if (got <= 0)
                return failure(r, ESP_ERR_TIMEOUT, "Bundle upload interrupted");
            initial += (size_t)got;
        }
        cyd_bundle_t manifest;
        if (!cyd_bundle_parse(prefix, sizeof(prefix), r->content_len, &manifest)) {
            httpd_resp_set_hdr(r, "Connection", "close");
            failure(r, ESP_ERR_INVALID_ARG,
                    "Choose the paired .cydos bundle for System update. Board .bin files belong to "
                    "bootstrap/recovery.");
            return ESP_FAIL;
        }
    }
    uint8_t reply[256];
    size_t got = 0;
    esp_err_t e;
    if (package) {
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "total", r->content_len);
        char *s = cJSON_PrintUnformatted(o);
        cJSON_Delete(o);
        if (!s)
            return ESP_ERR_NO_MEM;
        e = cyd_pair_rpc(CYD_PAIR_APP_BEGIN, s, strlen(s), reply, sizeof(reply), &got);
        free(s);
    } else {
        uint32_t total = r->content_len;
        e = cyd_pair_rpc(CYD_PAIR_BUNDLE_BEGIN, &total, 4, reply, sizeof(reply), &got);
    }
    if (e)
        return failure(r, e, "S3 not ready to receive upload; check its SD and connection");
    uint8_t *buffer = heap_caps_malloc(2052, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buffer)
        return ESP_ERR_NO_MEM;
    uint32_t offset = 0;
    if (initial) {
        memcpy(buffer, &offset, 4);
        memcpy(buffer + 4, prefix, initial);
        e = cyd_pair_rpc(CYD_PAIR_BUNDLE_WRITE, buffer, initial + 4, reply, sizeof(reply), &got);
        if (e) {
            free(buffer);
            return failure(r, e, "S3 did not accept the bundle header");
        }
        offset = (uint32_t)initial;
    }
    while (offset < r->content_len) {
        size_t want = r->content_len - offset;
        if (want > 2048)
            want = 2048;
        int count = httpd_req_recv(r, (char *)buffer + 4, want);
        if (count <= 0) {
            e = ESP_ERR_TIMEOUT;
            break;
        }
        memcpy(buffer, &offset, 4);
        e = cyd_pair_rpc(package ? CYD_PAIR_APP_WRITE : CYD_PAIR_BUNDLE_WRITE, buffer,
                         (size_t)count + 4, reply, sizeof(reply), &got);
        if (e)
            break;
        offset += (uint32_t)count;
    }
    free(buffer);
    if (e) {
        httpd_resp_set_hdr(r, "Connection", "close");
        failure(
            r, e,
            "Transfer interrupted. No restart was requested; retry after checking both boards.");
        return ESP_FAIL;
    }
    httpd_resp_set_status(r, "201 Created");
    uint8_t mode = inspect ? 1 : 0;
    return pair_result(r, package ? CYD_PAIR_APP_FINISH : CYD_PAIR_BUNDLE_FINISH,
                       package ? &mode : NULL, package ? 1 : 0);
}
static esp_err_t pair_gateway(httpd_req_t *r) {
    if (!auth(r))
        return ESP_OK;
    const char *uri = r->uri;
    cyd_peer_info_t peer;
    cyd_peer_snapshot(&peer);
    if (!peer.connected || !peer.services) {
        if (!strcmp(r->uri, "/api/os")) {
            cJSON *o = cJSON_CreateObject();
            cJSON_AddStringToObject(o, "state", "unavailable");
            cJSON_AddStringToObject(o, "message",
                                    "C5 manager is available. Connect the S3 with matching paired "
                                    "firmware for System update.");
            cJSON_AddStringToObject(o, "version", cyd_system_version());
            cJSON_AddStringToObject(o, "board", "paired_tablet");
            cJSON_AddStringToObject(o, "running_slot", "S3 unavailable");
            cJSON_AddBoolToObject(o, "boot_confirmed", false);
            cJSON_AddNumberToObject(o, "max_image_bytes", CYD_BUNDLE_LIMIT);
            return json(r, o);
        }
        return failure(r, ESP_ERR_TIMEOUT,
                       "S3 services unavailable. C5 Settings and recovery remain available.");
    }

    if (!strncmp(uri, "/api/packages/inspect", 21))
        return pair_upload(r, true, true);
    if (!strncmp(uri, "/api/packages", 13))
        return pair_upload(r, true, false);
    if (!strncmp(uri, "/api/os/update", 14))
        return pair_upload(r, false, false);
    if (!strcmp(uri, "/api/os/reboot"))
        return pair_result(r, CYD_PAIR_BUNDLE_COMMIT, NULL, 0);
    if (!strcmp(uri, "/api/os/cancel"))
        return pair_result(r, CYD_PAIR_BUNDLE_CANCEL, NULL, 0);
    if (!strcmp(uri, "/api/os"))
        return pair_result(r, CYD_PAIR_BUNDLE_STATUS, NULL, 0);
    if (!strcmp(uri, "/api/versions") && r->method == HTTP_GET)
        return pair_result(r, CYD_PAIR_VERSIONS, NULL, 0);
    if (!strcmp(uri, "/api/stop"))
        return pair_result(r, CYD_PAIR_STOP, NULL, 0);
    if (!strncmp(uri, "/api/run", 8) ||
        (!strncmp(uri, "/api/versions?", 14) && r->method == HTTP_DELETE)) {
        char rev[17];
        if (!revision(r, rev))
            return failure(r, ESP_ERR_INVALID_ARG, "Expected revision");
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "revision", rev);
        return pair_small(r, r->method == HTTP_DELETE ? CYD_PAIR_DELETE : CYD_PAIR_RUN, o);
    }
    if (!strcmp(uri, "/api/projects"))
        return r->method == HTTP_GET ? pair_result(r, CYD_PAIR_CATALOG, NULL, 0)
                                     : pair_small(r, CYD_PAIR_CATALOG_EDIT, request_json(r));
    if (!strncmp(uri, "/api/download", 13)) {
        char rev[17];
        if (!revision(r, rev))
            return failure(r, ESP_ERR_INVALID_ARG, "Expected revision");
        uint8_t data[2048];
        size_t got = 0;
        uint32_t offset = 0;
        httpd_resp_set_type(r, "application/zip");
        httpd_resp_set_hdr(r, "Content-Disposition", "attachment; filename=application.cydapp");
        for (;;) {
            char query[128];
            int n = snprintf(query, sizeof(query), "{\"revision\":\"%s\",\"offset\":%lu}", rev,
                             (unsigned long)offset);
            esp_err_t e =
                cyd_pair_rpc(CYD_PAIR_APP_READ, query, (size_t)n, data, sizeof(data), &got);
            if (e)
                return ESP_FAIL;
            if (!got)
                break;
            if (httpd_resp_send_chunk(r, (char *)data, got))
                return ESP_FAIL;
            offset += (uint32_t)got;
        }
        return httpd_resp_send_chunk(r, NULL, 0);
    }
    return failure(r, ESP_ERR_NOT_SUPPORTED,
                   "Use System update for paired firmware; recovery is a separate operation");
}
