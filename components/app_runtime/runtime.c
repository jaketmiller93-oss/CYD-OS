#ifdef ESP_PLATFORM
#include "sdkconfig.h"
#endif
#include "cyd.h"
#include "cyd_credentials.h"
#include "cyd_network.h"
#ifdef CONFIG_CYD_PAIRED_SYSTEM
#include "cyd_builtin.h"
#include "cyd_pair.h"
#endif
#include "cyd_services.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "freertos/queue.h"
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
#include <math.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
_Static_assert(sizeof(lua_Integer) == 8, "CYD Lua bindings require 64-bit integers");
_Static_assert(sizeof(lua_Number) == 8, "CYD Lua bindings require doubles");
extern unsigned cyd_lua_integer_bytes(void);
extern unsigned cyd_lua_number_bytes(void);
#define LUA_LIMIT (1024 * 1024)
#define UI_LIMIT 96
static SemaphoreHandle_t done, guard, log_guard;

static atomic_bool cancel;
static atomic_size_t lua_bytes, lua_peak;
static char state[16] = "stopped", revision[17] = "", package_id[65] = "", last_error[256] = "";
#if defined(CONFIG_CYD_PAIRED_SYSTEM) && defined(CONFIG_IDF_TARGET_ESP32C5) && \
    !defined(CONFIG_IDF_TARGET_ESP32S3)
/* The manager's log history is neither DMA nor ISR data. Its 20 KiB must not
 * occupy the internal RAM required by HTTPD, UART and service task stacks. */
static char (*logs)[320];
#else
static char logs[64][320];
#endif
static unsigned log_count, log_next;
typedef struct app app_t;
typedef struct {
    lv_obj_t *object, *label;
    app_t *owner;
    unsigned slot;
    uint32_t generation;
    int refs[3];
    bool view;
    void *pixels;
    lv_image_dsc_t image;
} widget_t;
struct app {
    char path[256], rev[17];
    cyd_manifest_t manifest;
    atomic_bool closing;
    size_t memory;
    unsigned objects, images;
    int64_t deadline, wall_deadline;
    lv_obj_t *root, *input_overlay, *input_edit;
    widget_t widgets[UI_LIMIT + 1];
    widget_t *input_target;
    uint32_t input_generation;
    cyd_services_t *services;
    SemaphoreHandle_t service_guard, service_done;
    bool worker_started;
    int timer_refs[CYD_TIMERS_MAX], http_ref, mqtt_refs[2][2];
    cyd_service_event_t delivery;
};
_Static_assert(sizeof(app_t) <= 32768, "App ownership state quota");
static void set_state(const char *s, const char *error) {
    xSemaphoreTake(guard, portMAX_DELAY);
    snprintf(state, sizeof(state), "%s", s);
    if (error)
        snprintf(last_error, sizeof(last_error), "%s", error);
    xSemaphoreGive(guard);
}
void cyd_log(const char *ctx, const char *msg) {
    if (!log_guard)
        return;
    xSemaphoreTake(log_guard, portMAX_DELAY);
    snprintf(logs[log_next], sizeof(logs[0]), "%lld [%s] %.240s", esp_timer_get_time() / 1000, ctx,
             msg);
    log_next = (log_next + 1) % 64;
    if (log_count < 64)
        log_count++;
    xSemaphoreGive(log_guard);
    ESP_LOGI("cyd", "[%s] %.240s", ctx, msg);
}
cJSON *cyd_logs(void) {
    cJSON *a = cJSON_CreateArray();
    xSemaphoreTake(log_guard, portMAX_DELAY);
    for (unsigned i = 0; i < log_count; i++)
        cJSON_AddItemToArray(a, cJSON_CreateString(logs[(log_next + 64 - log_count + i) % 64]));
    xSemaphoreGive(log_guard);
    return a;
}
cJSON *cyd_runtime_status(void) {
    cJSON *o = cJSON_CreateObject();
    xSemaphoreTake(guard, portMAX_DELAY);
    cJSON_AddStringToObject(o, "state", state);
    cJSON_AddStringToObject(o, "revision", revision);
    cJSON_AddStringToObject(o, "package_id", package_id);
    cJSON_AddStringToObject(o, "error", last_error);
    cJSON_AddNumberToObject(o, "lua_bytes", atomic_load(&lua_bytes));
    cJSON_AddNumberToObject(o, "lua_peak_bytes", atomic_load(&lua_peak));
    xSemaphoreGive(guard);
    return o;
}
void cyd_runtime_request_stop(void) { atomic_store(&cancel, true); }
esp_err_t cyd_runtime_stop(void) {
    cyd_runtime_request_stop();
    if (xSemaphoreTake(done, pdMS_TO_TICKS(2500)) != pdTRUE)
        return ESP_ERR_TIMEOUT;
    xSemaphoreGive(done);
    return ESP_OK;
}
static app_t *context(lua_State *L) {
    app_t *a = *(app_t **)lua_getextraspace(L);
    if (a->closing)
        luaL_error(L, "Application is closing");
    return a;
}
static void *allocator(void *ud, void *ptr, size_t old, size_t n) {
    app_t *a = ud;
    if (!ptr)
        old = 0;
    if (!n) {
        free(ptr);
        a->memory -= old;
        atomic_store(&lua_bytes, a->memory);
        return NULL;
    }
    if (n > old && n - old > LUA_LIMIT - a->memory)
        return NULL;
    void *p = heap_caps_realloc(ptr, n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (p) {
        a->memory = a->memory - old + n;
        atomic_store(&lua_bytes, a->memory);
        if (a->memory > atomic_load(&lua_peak))
            atomic_store(&lua_peak, a->memory);
    }
    return p;
}
static void execution_begin(app_t *a) {
    int64_t now = esp_timer_get_time();
    a->deadline = now + 500000;
    a->wall_deadline = now + 5000000;
}
static void execution_check(lua_State *L) {
    if (atomic_load(&cancel))
        luaL_error(L, "Application stopped");
    app_t *a = context(L);
    int64_t now = esp_timer_get_time();
    if (a->wall_deadline && now > a->wall_deadline)
        luaL_error(L, "Callback wall time limit exceeded (5 s)");
    if (now > a->deadline)
        luaL_error(L, "Execution budget exceeded (500 ms of script time per callback)");
}
static void hook(lua_State *L, lua_Debug *ar) {
    (void)ar;
    execution_check(L);
    int64_t before = esp_timer_get_time();
    vTaskDelay(1);
    context(L)->deadline += esp_timer_get_time() - before;
    execution_check(L);
}
typedef struct { lua_CFunction call; } ui_function_t;
static int call_ui_function(lua_State *L) {
    ui_function_t *fn = lua_touserdata(L, lua_upvalueindex(1));
    app_t *a = context(L);
    execution_check(L);
    int64_t before = esp_timer_get_time();
    int result = fn->call(L);
    /* UI work and waiting for the renderer are bounded native operations, not
     * script execution. Still enforce the independent wall limit and Stop. */
    a->deadline += esp_timer_get_time() - before;
    execution_check(L);
    return result;
}
static void ui_table_fn(lua_State *L, const char *name, lua_CFunction call) {
    ui_function_t *fn = lua_newuserdatauv(L, sizeof(*fn), 0);
    fn->call = call;
    lua_pushcclosure(L, call_ui_function, 1);
    lua_setfield(L, -2, name);
}
static const char *bounded_string(lua_State *L, int index, size_t max) {
    size_t n;
    const char *s = luaL_checklstring(L, index, &n);
    luaL_argcheck(L, n <= max && !memchr(s, 0, n), index, "String too long or contains NUL");
    return s;
}
typedef struct {
    void *data;
    bool json;
} native_holder_t;
static int native_gc(lua_State *L) {
    native_holder_t *h = lua_touserdata(L, 1);
    if (h->json)
        cJSON_Delete(h->data);
    else
        free(h->data);
    h->data = NULL;
    return 0;
}
static native_holder_t *native_holder(lua_State *L) {
    native_holder_t *h = lua_newuserdatauv(L, sizeof(*h), 0);
    *h = (native_holder_t){0};
    luaL_setmetatable(L, "cyd.native");
    return h;
}
static esp_err_t read_resource(app_t *a, const char *name, uint8_t **bytes, size_t *n,
                               size_t limit) {
    if (!cyd_safe_path(name) || !strcmp(name, "package.cydapp"))
        return ESP_ERR_INVALID_ARG;
    char path[256];
    snprintf(path, sizeof(path), CYD_ROOT "/projects/%s/%s", a->rev, name);
    return cyd_read_file(path, bytes, n, limit);
}
static bool utf8_valid(const unsigned char *, size_t);
#include "ui_bindings.inc"
static int log_lua(lua_State *L) {
    cyd_log(context(L)->manifest.id, bounded_string(L, 1, 240));
    return 0;
}
static int print_lua(lua_State *L) {
    char b[241] = "";
    size_t used = 0;
    for (int i = 1; i <= lua_gettop(L) && used < 240; i++) {
        const char *s = luaL_tolstring(L, i, NULL);
        int n = snprintf(b + used, sizeof(b) - used, "%s%s", i == 1 ? "" : "\t", s);
        lua_pop(L, 1);
        if (n < 0)
            break;
        used += (size_t)n;
        if (used > 240)
            used = 240;
    }
    cyd_log(context(L)->manifest.id, b);
    return 0;
}
static void data_path(lua_State *L, char *p, size_t n) {
    app_t *a = context(L);
    if (!a->manifest.storage)
        luaL_error(L, "storage permission required");
    const char *file = bounded_string(L, 1, 64);
    if (!cyd_safe_path(file) || strchr(file, '/'))
        luaL_error(L, "Storage expects a lowercase basename");
    snprintf(p, n, CYD_ROOT "/data/%s", a->manifest.id);
    if (cyd_mkdirs(p))
        luaL_error(L, "SD unavailable");
    size_t len = strlen(p);
    snprintf(p + len, n - len, "/%s", file);
}
static int storage_write(lua_State *L) {
    char p[256];
    data_path(L, p, sizeof(p));
    size_t n;
    const char *s = luaL_checklstring(L, 2, &n);
    if (n > 16384)
        return luaL_error(L, "Storage file limit 16 KiB");
    if (cyd_write_file(p, s, n))
        return luaL_error(L, "Storage write failed");
    return 0;
}
static int storage_read(lua_State *L) {
    char p[256];
    data_path(L, p, sizeof(p));
    native_holder_t *h = native_holder(L);
    uint8_t *b;
    size_t n;
    if (cyd_read_file(p, &b, &n, 16384)) {
        lua_pushnil(L);
        return 1;
    }
    h->data = b;
    lua_pushlstring(L, (char *)b, n);
    free(b);
    h->data = NULL;
    return 1;
}
static int uptime(lua_State *L) {
    lua_pushinteger(L, esp_timer_get_time() / 1000);
    return 1;
}
static void table_fn(lua_State *L, const char *name, lua_CFunction fn) {
    lua_pushcfunction(L, fn);
    lua_setfield(L, -2, name);
}
#include "essentials_bindings.inc"
#include "services_bindings.inc"
/* Lua disables hooks in __gc. Copy and protect user metatables so finalizers
   cannot be installed indirectly after validation. Native userdata has private
   GC. */
static int safe_setmetatable(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    if (lua_getmetatable(L, 1)) {
        lua_getfield(L, -1, "__metatable");
        if (!lua_isnil(L, -1))
            return luaL_error(L, "Protected table metatable");
        lua_pop(L, 2);
    }
    if (lua_isnil(L, 2)) {
        lua_pushnil(L);
        lua_setmetatable(L, 1);
        lua_settop(L, 1);
        return 1;
    }
    luaL_checktype(L, 2, LUA_TTABLE);
    lua_pushnil(L);
    while (lua_next(L, 2)) {
        if (lua_type(L, -2) == LUA_TSTRING) {
            const char *key = lua_tostring(L, -2);
            if (!strcmp(key, "__gc") || !strcmp(key, "__close"))
                return luaL_error(L, "Application finalizers are disabled");
        }
        lua_pop(L, 1);
    }
    lua_newtable(L);
    lua_pushnil(L);
    while (lua_next(L, 2)) {
        lua_pushvalue(L, -2);
        lua_pushvalue(L, -2);
        lua_rawset(L, -5);
        lua_pop(L, 1);
    }
    lua_pushliteral(L, "protected application metatable");
    lua_setfield(L, -2, "__metatable");
    lua_setmetatable(L, 1);
    lua_settop(L, 1);
    return 1;
}
static int safe_getmetatable(lua_State *L) {
    if (!lua_getmetatable(L, 1)) {
        lua_pushnil(L);
        return 1;
    }
    lua_pop(L, 1);
    lua_pushliteral(L, "protected application metatable");
    return 1;
}
static int bind_api(lua_State *L) {
    luaL_requiref(L, "_G", luaopen_base, 1);
    lua_pop(L, 1);
    luaL_requiref(L, "string", luaopen_string, 1);
    lua_pop(L, 1);
    luaL_requiref(L, "table", luaopen_table, 1);
    lua_pop(L, 1);
    luaL_requiref(L, "math", luaopen_math, 1);
    lua_pop(L, 1);
    /* Do not expose file loaders, dynamic code, protected calls or coroutine hook
     * bypasses. */
    const char *removed[] = {"dofile", "loadfile", "load", "collectgarbage", "pcall", "xpcall"};
    for (unsigned i = 0; i < sizeof(removed) / sizeof(*removed); i++) {
        lua_pushnil(L);
        lua_setglobal(L, removed[i]);
    }
    lua_pushcfunction(L, safe_setmetatable);
    lua_setglobal(L, "setmetatable");
    lua_pushcfunction(L, safe_getmetatable);
    lua_setglobal(L, "getmetatable");
    lua_pushcfunction(L, print_lua);
    lua_setglobal(L, "print");
    luaL_newmetatable(L, "cyd.native");
    lua_pushcfunction(L, native_gc);
    lua_setfield(L, -2, "__gc");
    lua_pushliteral(L, "protected native buffer");
    lua_setfield(L, -2, "__metatable");
    lua_pop(L, 1);
    ui_metatable(L);
    lua_newtable(L);
    lua_newtable(L);
    ui_table_fn(L, "create_screen", create_screen);
    ui_table_fn(L, "size", viewport);
    ui_table_fn(L, "show", show);
    ui_table_fn(L, "text_lines", text_lines);
    lua_setfield(L, -2, "ui");
    lua_newtable(L);
    table_fn(L, "write", storage_write);
    table_fn(L, "read", storage_read);
    lua_setfield(L, -2, "storage");
    lua_newtable(L);
    table_fn(L, "info", log_lua);
    lua_setfield(L, -2, "log");
    lua_newtable(L);
    table_fn(L, "uptime_ms", uptime);
    table_fn(L, "now", wall_clock);
    lua_setfield(L, -2, "time");
    lua_newtable(L);
    table_fn(L, "list", documents_list);
    table_fn(L, "read", documents_read);
    table_fn(L, "write", documents_write);
    table_fn(L, "rename", documents_rename);
    table_fn(L, "delete", documents_delete);
    lua_setfield(L, -2, "documents");
    lua_setglobal(L, "cyd");
    bind_services_api(L);
    bind_ui_api(L);
    return 0;
}
static void run_task(void *arg) {
    app_t *a = arg;
    bool failed = false;
    char error[256] = "";
    uint8_t *source = NULL;
    size_t size;
    lua_State *L = lua_newstate(allocator, a);
    if (!L) {
        snprintf(error, sizeof(error), "Lua memory allocation failed (PSRAM)");
        failed = true;
        goto cleanup;
    }
    if (!services_start(a)) {
        snprintf(error, sizeof(error), "App service allocation failed");
        failed = true;
        goto cleanup;
    }
    *(app_t **)lua_getextraspace(L) = a;
    /* API setup is protected too, so low-memory initialization cannot panic the
     * OS. */
    cyd_ui_lock();
    a->root = cyd_ui_app_root();
    a->widgets[UI_LIMIT] = (widget_t){.object = a->root,
                                      .owner = a,
                                      .slot = UI_LIMIT,
                                      .generation = 1,
                                      .refs = {LUA_NOREF, LUA_NOREF, LUA_NOREF}};
    lv_obj_clean(a->root);
    cyd_ui_show_app(true);
    cyd_ui_unlock();
    lua_pushcfunction(L, bind_api);
    if (lua_pcall(L, 0, 0, 0)) {
        snprintf(error, sizeof(error), "API setup allocation failed");
        failed = true;
        goto cleanup;
    }
    lua_sethook(L, hook, LUA_MASKCOUNT, 1000);
    const char *embedded = NULL;
#ifdef CONFIG_CYD_PAIRED_SYSTEM
    embedded = cyd_builtin_source(a->path, &size);
#endif
    esp_err_t e = ESP_OK;
    if (!embedded) e = cyd_read_file(a->path, &source, &size, CYD_MAX_SCRIPT);
    if (e) {
        snprintf(error, sizeof(error), "Entry point unavailable");
        failed = true;
        goto cleanup;
    }
    execution_begin(a);
    int rc = luaL_loadbufferx(L, embedded ? embedded : (char *)source, size, a->manifest.entry, "t");
    free(source);
    source = NULL;
    if (!rc)
        rc = lua_pcall(L, 0, 0, 0);
    if (rc) {
        snprintf(error, sizeof(error), "%s",
                 lua_tostring(L, -1) ? lua_tostring(L, -1) : "Non-string Lua error");
        failed = !atomic_load(&cancel);
        goto cleanup;
    }
    set_state("running", NULL);
    cyd_log(a->manifest.id, "Running");
    while (!atomic_load(&cancel)) {
        bool have = services_next(a);
        if (have) {
            lua_pushcfunction(L, dispatch_event);
            execution_begin(a);
            if (lua_pcall(L, 0, 0, 0)) {
                snprintf(error, sizeof(error), "%s",
                         lua_tostring(L, -1) ? lua_tostring(L, -1) : "Callback error");
                failed = !atomic_load(&cancel);
                break;
            }
        } else
            vTaskDelay(pdMS_TO_TICKS(10));
    }

cleanup:
    set_state("stopping", NULL);
    atomic_store(&cancel, true);
    a->closing = true;
    services_stop(a);
    if (L) {
        lua_close(L);
        L = NULL;
    }
    cyd_ui_lock();
    if (a->input_overlay)
        lv_obj_delete(a->input_overlay);
    if (a->root)
        lv_obj_clean(a->root);
    cyd_ui_show_app(false);
    cyd_ui_unlock();
    free(source);

    cyd_log(a->manifest.id, failed ? error : "Stopped; UI and Lua resources released");
    set_state(failed ? "crashed" : "stopped", failed ? error : "");
    if (a->service_guard)
        vSemaphoreDelete(a->service_guard);
    if (a->service_done)
        vSemaphoreDelete(a->service_done);
    free(a);
    xSemaphoreGive(done);
    vTaskDelete(NULL);
}
esp_err_t cyd_runtime_init(void) {
    if (cyd_lua_integer_bytes() != sizeof(lua_Integer) ||
        cyd_lua_number_bytes() != sizeof(lua_Number)) {
        ESP_LOGE("runtime", "Lua library/binding number ABI mismatch");
        return ESP_ERR_INVALID_STATE;
    }
#if defined(CONFIG_CYD_PAIRED_SYSTEM) && defined(CONFIG_IDF_TARGET_ESP32C5) && \
    !defined(CONFIG_IDF_TARGET_ESP32S3)
    if (!logs)
        logs = heap_caps_calloc(64, sizeof(*logs), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!logs)
        return ESP_ERR_NO_MEM;
#endif
    guard = xSemaphoreCreateMutex();
    log_guard = xSemaphoreCreateMutex();
    done = xSemaphoreCreateBinary();
    if (!guard || !log_guard || !done)
        return ESP_ERR_NO_MEM;
    xSemaphoreGive(done);
    return ESP_OK;
}
static esp_err_t launch_nomem(const char *stage) {
    char message[224];
    snprintf(message, sizeof(message), "%s; internal free=%u largest=%u; PSRAM free=%u largest=%u",
             stage, (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    atomic_store(&cancel, true);
    set_state("crashed", message);
    cyd_log("runtime", message);
    return ESP_ERR_NO_MEM;
}
esp_err_t cyd_runtime_start(const char *p, const char *r, const cyd_manifest_t *m) {
#ifdef CONFIG_CYD_PAIRED_SYSTEM
    if(cyd_pair_installing())return ESP_ERR_INVALID_STATE;
#endif
    if (cyd_maintenance_active())
        return ESP_ERR_INVALID_STATE;
    esp_err_t e = cyd_runtime_stop();
    if (e)
        return e;
    if (xSemaphoreTake(done, 0) != pdTRUE)
        return ESP_ERR_INVALID_STATE;
    if (cyd_maintenance_active()) {
        xSemaphoreGive(done);
        return ESP_ERR_INVALID_STATE;
    }
    xSemaphoreTake(guard, portMAX_DELAY);
    snprintf(package_id, sizeof(package_id), "%s", m->id);
    snprintf(revision, sizeof(revision), "%s", r);
    strcpy(state, "starting");
    last_error[0] = 0;
    xSemaphoreGive(guard);
    atomic_store(&lua_bytes, 0);
    atomic_store(&lua_peak, 0);
    atomic_store(&cancel, false);
    app_t *a = heap_caps_calloc(1, sizeof(*a), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!a) {
        esp_err_t error = launch_nomem("App context allocation failed (PSRAM)");
        xSemaphoreGive(done);
        return error;
    }
    for (unsigned i = 0; i < CYD_TIMERS_MAX; i++)
        a->timer_refs[i] = LUA_NOREF;
    a->http_ref = LUA_NOREF;
    for (unsigned i = 0; i < 2; i++)
        a->mqtt_refs[i][0] = a->mqtt_refs[i][1] = LUA_NOREF;
    snprintf(a->path, sizeof(a->path), "%s", p);
    snprintf(a->rev, sizeof(a->rev), "%s", r);
    a->manifest = *m;
    if (xTaskCreate(run_task, "lua_app", 12288, a, 3, NULL) != pdPASS) {
        free(a);
        esp_err_t error = launch_nomem("Lua task allocation failed (12288-byte internal stack)");
        xSemaphoreGive(done);
        return error;
    }
    return ESP_OK;
}
