static int result_error(lua_State *, const char *);
/* Only this task calls Lua. Service/UI producers enqueue bounded owned events.
 */
static void service_worker(void *arg) {
    app_t *a = arg;
    while (!a->closing) {
        xSemaphoreTake(a->service_guard, portMAX_DELAY);
        cyd_services_tick(a->services, (uint64_t)esp_timer_get_time() / 1000);
        xSemaphoreGive(a->service_guard);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    xSemaphoreTake(a->service_guard, portMAX_DELAY);
    cyd_services_close(a->services);
    xSemaphoreGive(a->service_guard);
    xSemaphoreGive(a->service_done);
    vTaskDelete(NULL);
}
static bool services_start(app_t *a) {
    a->service_guard = xSemaphoreCreateMutex();
    a->services = heap_caps_calloc(1, sizeof(*a->services), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!a->service_guard || !a->services)
        return false;
    cyd_io_t io = cyd_services_esp_io();
    cyd_services_init(a->services, &io);
    a->services->now = esp_timer_get_time() / 1000;
    /* Offline timers and touch are dispatched by the Lua task. They do not need
     * a separate internal stack. Network IO retains its independent worker. */
    if (!a->manifest.network && !a->manifest.mqtt)
        return true;
    a->service_done = xSemaphoreCreateBinary();
    if (!a->service_done)
        return false;
    if (xTaskCreate(service_worker, "app_services", 12288, a, 3, NULL) != pdPASS)
        return false;
    a->worker_started = true;
    return true;
}
static bool services_next(app_t *a) {
    xSemaphoreTake(a->service_guard, portMAX_DELAY);
    if (!a->worker_started)
        cyd_services_tick(a->services, (uint64_t)esp_timer_get_time() / 1000);
    bool have = cyd_services_pop(a->services, &a->delivery);
    xSemaphoreGive(a->service_guard);
    return have;
}
static void services_stop(app_t *a) {
    if (a->worker_started)
        xSemaphoreTake(a->service_done, portMAX_DELAY);
    if (a->service_guard)
        xSemaphoreTake(a->service_guard, portMAX_DELAY);
    if (a->services) {
        if (!a->worker_started)
            cyd_services_close(a->services);
        volatile unsigned char *p = (void *)a->services;
        for (size_t i = 0; i < sizeof(*a->services); i++)
            p[i] = 0;
        free(a->services);
        a->services = NULL;
    }
    if (a->service_guard)
        xSemaphoreGive(a->service_guard);
    /* Mutex remains until LVGL delete callbacks are removed. */
}
static void permission(lua_State *L, bool allowed, const char *name) {
    (void)context(L);
    if (!allowed)
        luaL_error(L, "%s permission required", name);
}
static void unref(lua_State *L, int *ref) {
    if (*ref != LUA_NOREF && *ref != 0)
        luaL_unref(L, LUA_REGISTRYINDEX, *ref);
    *ref = LUA_NOREF;
}
typedef struct {
    app_t *owner;
    unsigned slot;
    uint32_t generation;
    bool timer, mqtt;
} service_handle_t;
static int push_service(lua_State *L, app_t *a, unsigned slot, uint32_t gen, bool timer,
                        bool mqtt) {
    service_handle_t *h = lua_newuserdatauv(L, sizeof(*h), 0);
    *h = (service_handle_t){a, slot, gen, timer, mqtt};
    luaL_setmetatable(L, "cyd.service");
    return 1;
}
static service_handle_t *service_handle(lua_State *L) {
    app_t *a = context(L);
    service_handle_t *h = luaL_checkudata(L, 1, "cyd.service");
    if (h->owner != a) {
        luaL_error(L, "Foreign service handle");
        return NULL;
    }
    if (!h->timer)
        permission(L, h->mqtt ? a->manifest.mqtt : a->manifest.network,
                   h->mqtt ? "mqtt" : "network");
    return h;
}
static int service_cancel(lua_State *L) {
    service_handle_t *h = service_handle(L);
    app_t *a = context(L);
    xSemaphoreTake(a->service_guard, portMAX_DELAY);
    bool ok = h->timer ? cyd_services_timer_cancel(a->services, h->slot, h->generation)
                       : cyd_services_disconnect(a->services, h->slot, h->generation);
    xSemaphoreGive(a->service_guard);
    if (ok) {
        if (h->timer)
            unref(L, &a->timer_refs[h->slot]);
        else if (h->mqtt) {
            unref(L, &a->mqtt_refs[h->slot - 1][0]);
            unref(L, &a->mqtt_refs[h->slot - 1][1]);
        } else
            unref(L, &a->http_ref);
    }
    lua_pushboolean(L, ok);
    return 1;
}
static int timer_new(lua_State *L, bool repeat) {
    app_t *a = context(L);
    lua_Integer ms = luaL_checkinteger(L, 1);
    if (ms < 20 || ms > 86400000)
        return luaL_error(L, "Timer interval: 20..86400000 ms");
    luaL_checktype(L, 2, LUA_TFUNCTION);
    uint32_t gen;
    xSemaphoreTake(a->service_guard, portMAX_DELAY);
    if (!a->worker_started) {
        uint64_t now = (uint64_t)esp_timer_get_time() / 1000;
        if (now > a->services->now)
            cyd_services_tick(a->services, now);
    }
    int slot = cyd_services_timer(a->services, ms, repeat, &gen);
    xSemaphoreGive(a->service_guard);
    if (slot < 0)
        return result_error(L, "Timer quota exceeded");
    unref(L, &a->timer_refs[slot]);
    lua_pushvalue(L, 2);
    a->timer_refs[slot] = luaL_ref(L, LUA_REGISTRYINDEX);
    return push_service(L, a, slot, gen, true, false);
}
static int timer_every(lua_State *L) { return timer_new(L, true); }
static int timer_after(lua_State *L) { return timer_new(L, false); }
static int network_info(lua_State *L) {
    app_t *a = context(L);
    permission(L, a->manifest.network, "network");
    cyd_net_status_t s;
    cyd_network_snapshot(&s);
    lua_newtable(L);
    lua_pushboolean(L, s.station_enabled);
    lua_setfield(L, -2, "station_enabled");
    lua_pushboolean(L, s.station_enabled && s.state == CYD_NET_CONNECTED && *s.station_ip);
    lua_setfield(L, -2, "station_connected");
    lua_pushstring(L, s.station_enabled && s.state == CYD_NET_CONNECTED ? s.station_ip : "");
    lua_setfield(L, -2, "station_ip");
    lua_pushboolean(L, s.ap_enabled);
    lua_setfield(L, -2, "ap_enabled");
    lua_pushstring(L, s.ap_ip);
    lua_setfield(L, -2, "ap_ip");
    return 1;
}
static bool endpoint_options(lua_State *L, int table, cyd_endpoint_t *out, bool mqtt) {
    const char *forbidden[] = {"insecure", "skip_verification", "skip_cert_common_name_check",
                               "skip_common_name", "verify"};
    for (unsigned i = 0; i < 5; i++) {
        option_field(L, table, forbidden[i]);
        bool present = !lua_isnil(L, -1);
        lua_pop(L, 1);
        if (present)
            return false;
    }

    const char *url = field_text(L, table, "url", "", 768);
    if (mqtt ? strncmp(url, "mqtt://", 7) && strncmp(url, "mqtts://", 8)
             : strncmp(url, "http://", 7) && strncmp(url, "https://", 8))
        return false;
    return cyd_endpoint_parse(url, field_text(L, table, "ca_pem", "", CYD_CA_MAX),
                              field_text(L, table, "verification_name", "", 253), out);
}
static int http_request(lua_State *L) {
    app_t *a = context(L);
    permission(L, a->manifest.network, "network");
    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    cyd_endpoint_t *ep = lua_newuserdatauv(
        L, sizeof(*ep), 0); /* Validate Lua fields before allocating backend state. */
    bool valid = endpoint_options(L, 1, ep, false);
    if (!valid) {
        return luaL_error(L, "Invalid HTTP(S) URL/TLS options");
    }
    const char *method = field_text(L, 1, "method", "GET", 4);
    if (strcmp(method, "GET") && strcmp(method, "POST")) {
        return luaL_error(L, "HTTP method must be GET or POST");
    }
    const char *headers = field_text(L, 1, "headers", "", CYD_HEADERS_MAX);
    unsigned timeout = field_int(L, 1, "timeout_ms", 5000, 100, 30000);
    option_field(L, 1, "body");
    size_t size = 0;
    const char *body = lua_isnil(L, -1) ? "" : luaL_checklstring(L, -1, &size);
    if (size > CYD_REQUEST_MAX) {
        return luaL_error(L, "HTTP request body exceeds 4096 bytes");
    }
    uint32_t gen;
    xSemaphoreTake(a->service_guard, portMAX_DELAY);
    int slot = cyd_services_http(a->services, ep, !strcmp(method, "POST"), headers, body, size,
                                 timeout, &gen);
    xSemaphoreGive(a->service_guard);
    lua_pop(L, 1);
    if (slot < 0)
        return result_error(L, "HTTP slot busy, invalid headers or request frame too large");
    unref(L, &a->http_ref);
    lua_pushvalue(L, 2);
    a->http_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    return push_service(L, a, 0, gen, false, false);
}
static int mqtt_connect(lua_State *L) {
    app_t *a = context(L);
    permission(L, a->manifest.mqtt, "mqtt");
    luaL_checktype(L, 1, LUA_TTABLE);
    cyd_endpoint_t *ep = lua_newuserdatauv(L, sizeof(*ep), 0);
    if (!endpoint_options(L, 1, ep, true)) {
        return luaL_error(L, "Invalid MQTT(S) URL/TLS options");
    }
    char generated_id[24];
    snprintf(generated_id, sizeof(generated_id), "cyd-%08lx%08lx", (unsigned long)esp_random(),
             (unsigned long)esp_random());
    const char *id = field_text(L, 1, "client_id", generated_id, 64),
               *user = field_text(L, 1, "username", "", 128),
               *password = field_text(L, 1, "password", "", 256);
    uint32_t gen;
    xSemaphoreTake(a->service_guard, portMAX_DELAY);
    int slot = cyd_services_mqtt(a->services, ep, id, user, password, &gen);
    xSemaphoreGive(a->service_guard);
    if (slot < 0)
        return result_error(L, "MQTT client quota or credentials invalid");
    const char *names[] = {"on_message", "on_state"};
    for (unsigned i = 0; i < 2; i++) {
        unref(L, &a->mqtt_refs[slot - 1][i]);
        option_field(L, 1, names[i]);
        if (!lua_isnil(L, -1)) {
            luaL_checktype(L, -1, LUA_TFUNCTION);
            a->mqtt_refs[slot - 1][i] = luaL_ref(L, LUA_REGISTRYINDEX);
        } else
            lua_pop(L, 1);
    }
    return push_service(L, a, slot, gen, false, true);
}
static int mqtt_subscribe(lua_State *L) {
    service_handle_t *h = service_handle(L);
    if (!h->mqtt)
        return luaL_error(L, "Expected MQTT handle");
    const char *topic = bounded_string(L, 2, 128);
    app_t *a = context(L);
    xSemaphoreTake(a->service_guard, portMAX_DELAY);
    bool ok = cyd_services_subscribe(a->services, h->slot, h->generation, topic);
    xSemaphoreGive(a->service_guard);
    lua_pushboolean(L, ok);
    return 1;
}
static int mqtt_publish(lua_State *L) {
    service_handle_t *h = service_handle(L);
    if (!h->mqtt)
        return luaL_error(L, "Expected MQTT handle");
    const char *topic = bounded_string(L, 2, 128);
    size_t size;
    const char *body = luaL_checklstring(L, 3, &size);
    bool retain = lua_toboolean(L, 4);
    app_t *a = context(L);
    xSemaphoreTake(a->service_guard, portMAX_DELAY);
    bool ok = cyd_services_publish(a->services, h->slot, h->generation, topic, body, size, retain);
    xSemaphoreGive(a->service_guard);
    lua_pushboolean(L, ok);
    return 1;
}
static int mqtt_state(lua_State *L) {
    service_handle_t *h = service_handle(L);
    if (!h->mqtt)
        return luaL_error(L, "Expected MQTT handle");
    app_t *a = context(L);
    char error[96];
    xSemaphoreTake(a->service_guard, portMAX_DELAY);
    cyd_service_conn_t *c = &a->services->conn[h->slot];
    bool valid = c->used && c->generation == h->generation, ready = valid && c->ready,
         retrying = valid && c->retry_at > a->services->now;
    unsigned dropped = c->dropped_messages;
    snprintf(error, sizeof(error), "%s", valid ? c->error : "");
    xSemaphoreGive(a->service_guard);
    lua_pushstring(L, !valid     ? "closed"
                      : ready    ? "connected"
                      : retrying ? "reconnecting"
                                 : "connecting");
    lua_pushstring(L, error);
    lua_pushinteger(L, dropped);
    return 3;
}
static int credentials(lua_State *L, int action) {
    app_t *a = context(L);
    permission(L, a->manifest.credentials, "credentials");
    const char *key = bounded_string(L, 1, 32);
    unsigned char value[256];
    size_t size = sizeof(value);
    esp_err_t e;
    if (action == 0) {
        e = cyd_credentials_get(a->manifest.id, key, value, &size);
        if (!e)
            lua_pushlstring(L, (char *)value, size);
        else
            lua_pushnil(L);
    } else if (action == 1) {
        const char *s = luaL_checklstring(L, 2, &size);
        if (size > 256)
            return luaL_error(L, "Credential value limit 256 bytes");
        e = cyd_credentials_set(a->manifest.id, key, s, size);
        lua_pushboolean(L, !e);
    } else {
        e = cyd_credentials_delete(a->manifest.id, key);
        lua_pushboolean(L, !e);
    }
    volatile unsigned char *p = value;
    for (unsigned i = 0; i < sizeof(value); i++)
        p[i] = 0;
    if (e && e != ESP_ERR_NOT_FOUND)
        return luaL_error(L, "Credential operation failed (quota, record or NVS error)");
    return 1;
}
static int cred_get(lua_State *L) { return credentials(L, 0); }
static int cred_set(lua_State *L) { return credentials(L, 1); }
static int cred_delete(lua_State *L) { return credentials(L, 2); }
static int cred_encrypted(lua_State *L) {
    permission(L, context(L)->manifest.credentials, "credentials");
    lua_pushboolean(L, false);
    return 1;
}
static int resource_read(lua_State *L) {
    app_t *a = context(L);
    const char *name = bounded_string(L, 1, CYD_MAX_PATH - 1);
    if (!cyd_safe_path(name) || !strcmp(name, "package.cydapp"))
        return luaL_error(L, "Invalid resource path");
    native_holder_t *h = native_holder(L);
    uint8_t *bytes;
    size_t n;
    if (read_resource(a, name, &bytes, &n, CYD_MAX_SCRIPT)) {
        lua_pushnil(L);
        return 1;
    }
    h->data = bytes;
    lua_pushlstring(L, (char *)bytes, n);
    free(bytes);
    h->data = NULL;
    return 1;
}
static int result_error(lua_State *L, const char *message) {
    lua_pushnil(L);
    lua_pushstring(L, message);
    return 2;
}
static char json_null;
static bool utf8_valid(const unsigned char *p, size_t n) {
    for (size_t i = 0; i < n;) {
        unsigned c = p[i++];
        if (c < 128)
            continue;
        unsigned count = c >= 0xc2 && c <= 0xdf   ? 1
                         : c >= 0xe0 && c <= 0xef ? 2
                         : c >= 0xf0 && c <= 0xf4 ? 3
                                                  : 0;
        if (!count || i + count > n)
            return false;
        unsigned value = c & ((1u << (6 - count)) - 1);
        for (unsigned j = 0; j < count; j++) {
            unsigned b = p[i++];
            if ((b & 0xc0) != 0x80)
                return false;
            value = (value << 6) | (b & 63);
        }
        if (value < (count == 1   ? 128
                     : count == 2 ? 2048
                                  : 65536) ||
            value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))
            return false;
    }
    return true;
}
static bool json_valid_tree(cJSON *v, unsigned *nodes) {
    if (++*nodes > 256)
        return false;
    if (cJSON_IsNumber(v) && !isfinite(v->valuedouble))
        return false;
    for (cJSON *p = v->child; p; p = p->next) {
        if (!json_valid_tree(p, nodes))
            return false;
        if (cJSON_IsObject(v))
            for (cJSON *q = p->next; q; q = q->next)
                if (!strcmp(p->string, q->string))
                    return false;
    }
    return true;
}
static void json_push(lua_State *L, cJSON *v) {
    if (cJSON_IsNull(v))
        lua_pushlightuserdata(L, &json_null);
    else if (cJSON_IsBool(v))
        lua_pushboolean(L, cJSON_IsTrue(v));
    else if (cJSON_IsNumber(v))
        lua_pushnumber(L, v->valuedouble);
    else if (cJSON_IsString(v))
        lua_pushstring(L, v->valuestring);
    else {
        lua_newtable(L);
        unsigned i = 1;
        for (cJSON *p = v->child; p; p = p->next) {
            json_push(L, p);
            if (cJSON_IsObject(v))
                lua_setfield(L, -2, p->string);
            else
                lua_rawseti(L, -2, i++);
        }
    }
}
/* cJSON lifetime is protected across a Lua OOM using an inner protected
 * conversion. */
static int json_convert(lua_State *L) {
    native_holder_t *h = lua_touserdata(L, lua_upvalueindex(1));
    json_push(L, h->data);
    return 1;
}
static int json_decode(lua_State *L) {
    (void)context(L);
    size_t size;
    const char *text = luaL_checklstring(L, 1, &size);
    if (size > 8192 || !utf8_valid((const unsigned char *)text, size))
        return result_error(L, "JSON size/UTF-8 invalid");
    unsigned complexity = 0;
    bool quoted = false, escape = false;
    for (size_t i = 0; i < size; i++) {
        char c = text[i];
        if (quoted) {
            if (escape)
                escape = false;
            else if (c == '\\')
                escape = true;
            else if (c == '"')
                quoted = false;
        } else if (c == '"')
            quoted = true;
        else if (c == ',' || c == ':' || c == '[' || c == '{') {
            if (++complexity > 256)
                return result_error(L, "JSON complexity limit");
        }
    }
    native_holder_t *holder = native_holder(L);
    holder->json = true;
    cJSON *root = cyd_json_parse(text, size, 8192);
    holder->data = root;
    unsigned nodes = 0;
    if (!root || !json_valid_tree(root, &nodes)) {
        cJSON_Delete(root);
        holder->data = NULL;
        return result_error(L, "Malformed JSON or complexity limit");
    }
    lua_pushvalue(L, -1);
    lua_pushcclosure(L, json_convert, 1);
    int rc = lua_pcall(L, 0, 1, 0);
    cJSON_Delete(root);
    holder->data = NULL;
    if (rc)
        return lua_error(L);
    return 1;
}
static int dispatch_event(lua_State *L) {
    app_t *a = context(L);
    cyd_service_event_t *e = &a->delivery;
    int ref = LUA_NOREF, args = 0;
    if (e->kind == CYD_EV_TOUCH) {
        unsigned slot = e->slot / 3, kind = e->slot % 3;
        if (slot >= UI_LIMIT || a->widgets[slot].generation != e->generation ||
            !a->widgets[slot].object)
            return 0;
        ref = a->widgets[slot].refs[kind];
    } else {
        xSemaphoreTake(a->service_guard, portMAX_DELAY);
        bool valid = cyd_services_valid(a->services, e);
        xSemaphoreGive(a->service_guard);
        if (!valid)
            return 0;
        if (e->kind == CYD_EV_TIMER)
            ref = a->timer_refs[e->slot];
        else if (e->kind == CYD_EV_HTTP)
            ref = a->http_ref;
        else
            ref = a->mqtt_refs[e->slot - 1][e->kind == CYD_EV_STATE ? 1 : 0];
    }
    if (ref == LUA_NOREF)
        return 0;
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
    if (e->kind == CYD_EV_HTTP) {
        lua_newtable(L);
        lua_pushboolean(L, e->status > 0);
        lua_setfield(L, -2, "ok");
        lua_pushinteger(L, e->status);
        lua_setfield(L, -2, "status");
        lua_pushlstring(L, (char *)e->body, e->size);
        lua_setfield(L, -2, "body");
        lua_pushstring(L, e->error);
        lua_setfield(L, -2, "error");
        args = 1;
    }
    if (e->kind == CYD_EV_MESSAGE) {
        lua_pushstring(L, e->topic);
        lua_pushlstring(L, (char *)e->body, e->size);
        args = 2;
    }
    if (e->kind == CYD_EV_STATE) {
        lua_pushstring(L, e->status ? "connected" : "reconnecting");
        lua_pushstring(L, e->error);
        args = 2;
    }
    if (e->kind == CYD_EV_TOUCH && e->slot % 3) {
        widget_t *w = &a->widgets[e->slot / 3];
        char text[257] = "";
        cyd_ui_lock();
        if (lv_obj_check_type(w->object, &lv_textarea_class))
            snprintf(text, sizeof(text), "%s", lv_textarea_get_text(w->object));
        cyd_ui_unlock();
        lua_pushstring(L, text);
        args = 1;
    }
    lua_call(L, args, 0);
    return 0;
}
static void bind_services_api(lua_State *L) {
    luaL_newmetatable(L, "cyd.service");
    lua_pushliteral(L, "protected app service");
    lua_setfield(L, -2, "__metatable");
    lua_newtable(L);
    const luaL_Reg funcs[] = {{"cancel", service_cancel},    {"disconnect", service_cancel},
                              {"subscribe", mqtt_subscribe}, {"publish", mqtt_publish},
                              {"state", mqtt_state},         {NULL, NULL}};
    luaL_setfuncs(L, funcs, 0);
    lua_setfield(L, -2, "__index");
    lua_pop(L, 1);
    lua_getglobal(L, "cyd");
    lua_newtable(L);
    table_fn(L, "info", network_info);
    table_fn(L, "request", http_request);
    lua_setfield(L, -2, "network");
    lua_newtable(L);
    table_fn(L, "connect", mqtt_connect);
    lua_setfield(L, -2, "mqtt");
    lua_newtable(L);
    table_fn(L, "after", timer_after);
    table_fn(L, "every", timer_every);
    lua_setfield(L, -2, "timer");
    lua_newtable(L);
    table_fn(L, "get", cred_get);
    table_fn(L, "set", cred_set);
    table_fn(L, "delete", cred_delete);
    table_fn(L, "encrypted_at_rest", cred_encrypted);
    lua_setfield(L, -2, "credentials");
    lua_newtable(L);
    table_fn(L, "read", resource_read);
    lua_setfield(L, -2, "resources");
    lua_newtable(L);
    table_fn(L, "decode", json_decode);
    lua_pushlightuserdata(L, &json_null);
    lua_setfield(L, -2, "null");
    lua_setfield(L, -2, "json");
    lua_pop(L, 1);
}
