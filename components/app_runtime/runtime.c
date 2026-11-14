#include "cyd.h"
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/queue.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdatomic.h>
#define LUA_LIMIT (1024 * 1024)
#define UI_LIMIT 96
static SemaphoreHandle_t done, guard, log_guard;
static QueueHandle_t events;
static atomic_bool cancel;
static atomic_size_t lua_bytes, lua_peak;
static char state[16] = "stopped", revision[17] = "", package_id[65] = "", last_error[256] = "";
static char logs[64][320];
static unsigned log_count, log_next;
typedef struct {
    char path[256], rev[17];
    cyd_manifest_t manifest;
    bool closing;
    size_t memory;
    unsigned objects;
    int64_t deadline;
    lv_obj_t *root;
} app_t;
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
void cyd_runtime_request_stop(void) {
    atomic_store(&cancel, true);
}
esp_err_t cyd_runtime_stop(void) {
    cyd_runtime_request_stop();
    if (xSemaphoreTake(done, pdMS_TO_TICKS(2500)) != pdTRUE)
        return ESP_ERR_TIMEOUT;
    xSemaphoreGive(done);
    return ESP_OK;
}
void cyd_runtime_event(int ref) {
    xQueueSend(events, &ref, 0);
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
    void *p = realloc(ptr, n);
    if (p) {
        a->memory = a->memory - old + n;
        atomic_store(&lua_bytes, a->memory);
        if (a->memory > atomic_load(&lua_peak))
            atomic_store(&lua_peak, a->memory);
    }
    return p;
}
static void hook(lua_State *L, lua_Debug *ar) {
    (void)ar;
    if (atomic_load(&cancel))
        luaL_error(L, "Application stopped");
    if (esp_timer_get_time() > context(L)->deadline)
        luaL_error(L, "Execution budget exceeded (500 ms per callback)");
    vTaskDelay(1);
}
static const char *bounded_string(lua_State *L, int index, size_t max) {
    size_t n;
    const char *s = luaL_checklstring(L, index, &n);
    luaL_argcheck(L, n <= max && !memchr(s, 0, n), index, "String too long or contains NUL");
    return s;
}
static lv_obj_t *object(lua_State *L, int i) {
    (void)context(L);
    return *(lv_obj_t **)luaL_checkudata(L, i, "cyd.object");
}
static int push_object(lua_State *L, lv_obj_t *o) {
    lv_obj_t **p = lua_newuserdatauv(L, sizeof(*p), 0);
    *p = o;
    luaL_setmetatable(L, "cyd.object");
    return 1;
}
static int create_screen(lua_State *L) {
    return push_object(L, context(L)->root);
}
static int show(lua_State *L) {
    (void)object(L, 1);
    return 0;
}
static int viewport(lua_State *L) {
    app_t *a = context(L);
    cyd_ui_lock();
    lv_obj_update_layout(a->root);
    int w = lv_obj_get_content_width(a->root), h = lv_obj_get_content_height(a->root);
    cyd_ui_unlock();
    lua_pushinteger(L, w);
    lua_pushinteger(L, h);
    return 2;
}
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
static int set_text(lua_State *L) {
    lv_obj_t *o = object(L, 1);
    const char *s = bounded_string(L, 2, 1024);
    cyd_ui_lock();
    if (lv_obj_check_type(o, &lv_label_class))
        lv_label_set_text(o, s);
    cyd_ui_unlock();
    return 0;
}
static void click(lv_event_t *e) {
    cyd_runtime_event((int)(intptr_t)lv_event_get_user_data(e));
}
static int add_widget(lua_State *L, bool button) {
    app_t *a = context(L);
    lv_obj_t *parent = object(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    if (a->objects >= UI_LIMIT)
        return luaL_error(L, "UI object limit");
    lua_getfield(L, 2, "text");
    const char *text = lua_isnil(L, -1) ? "" : bounded_string(L, -1, 1024);
    lua_getfield(L, 2, "x");
    int x = luaL_optinteger(L, -1, 20);
    lua_pop(L, 1);
    lua_getfield(L, 2, "y");
    int y = luaL_optinteger(L, -1, 20);
    lua_pop(L, 1);
    int ref = LUA_NOREF;
    if (button) {
        lua_getfield(L, 2, "on_click");
        if (lua_isfunction(L, -1))
            ref = luaL_ref(L, LUA_REGISTRYINDEX);
        else
            lua_pop(L, 1);
    }
    cyd_ui_lock();
    lv_obj_t *o = button ? lv_button_create(parent) : lv_label_create(parent);
    a->objects++;
    lv_obj_set_pos(o, x, y);
    if (button) {
        lv_obj_set_size(o, 180, 52);
        lv_obj_t *label = lv_label_create(o);
        lv_label_set_text(label, text);
        lv_obj_center(label);
        if (ref != LUA_NOREF)
            lv_obj_add_event_cb(o, click, LV_EVENT_CLICKED, (void *)(intptr_t)ref);
    } else {
        lv_label_set_text(o, text);
        lv_obj_set_width(o, lv_obj_get_content_width(cyd_ui_app_root()) - 24);
        lv_label_set_long_mode(o, LV_LABEL_LONG_WRAP);
    }
    cyd_ui_unlock();
    return push_object(L, o);
}
static int label(lua_State *L) {
    return add_widget(L, false);
}
static int button(lua_State *L) {
    return add_widget(L, true);
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
    uint8_t *b;
    size_t n;
    if (cyd_read_file(p, &b, &n, 16384)) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushlstring(L, (char *)b, n);
    free(b);
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
static int bind_api(lua_State *L) {
    luaL_requiref(L, "_G", luaopen_base, 1);
    lua_pop(L, 1);
    luaL_requiref(L, "string", luaopen_string, 1);
    lua_pop(L, 1);
    luaL_requiref(L, "table", luaopen_table, 1);
    lua_pop(L, 1);
    luaL_requiref(L, "math", luaopen_math, 1);
    lua_pop(L, 1);
    /* Do not expose file loaders, dynamic code, protected calls or coroutine hook bypasses. */
    const char *removed[] = {"dofile", "loadfile", "load", "collectgarbage", "pcall", "xpcall"};
    for (unsigned i = 0; i < sizeof(removed) / sizeof(*removed); i++) {
        lua_pushnil(L);
        lua_setglobal(L, removed[i]);
    }
    lua_pushcfunction(L, print_lua);
    lua_setglobal(L, "print");
    luaL_newmetatable(L, "cyd.object");
    lua_pushliteral(L, "protected CYD widget");
    lua_setfield(L, -2, "__metatable");
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    table_fn(L, "add_label", label);
    table_fn(L, "add_button", button);
    table_fn(L, "set_text", set_text);
    lua_pop(L, 1);
    lua_newtable(L);
    lua_newtable(L);
    table_fn(L, "create_screen", create_screen);
    table_fn(L, "size", viewport);
    table_fn(L, "show", show);
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
    lua_setfield(L, -2, "time");
    lua_setglobal(L, "cyd");
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
        snprintf(error, sizeof(error), "Lua memory allocation failed");
        failed = true;
        goto cleanup;
    }
    *(app_t **)lua_getextraspace(L) = a;
    /* API setup is protected too, so low-memory initialization cannot panic the OS. */
    cyd_ui_lock();
    a->root = cyd_ui_app_root();
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
    esp_err_t e = cyd_read_file(a->path, &source, &size, CYD_MAX_SCRIPT);
    if (e) {
        snprintf(error, sizeof(error), "Entry point unavailable");
        failed = true;
        goto cleanup;
    }
    a->deadline = esp_timer_get_time() + 500000;
    int rc = luaL_loadbufferx(L, (char *)source, size, a->manifest.entry, "t");
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
        int ref;
        if (xQueueReceive(events, &ref, pdMS_TO_TICKS(20)) == pdTRUE) {
            lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
            a->deadline = esp_timer_get_time() + 500000;
            if (lua_pcall(L, 0, 0, 0)) {
                snprintf(error, sizeof(error), "%s",
                         lua_tostring(L, -1) ? lua_tostring(L, -1) : "Non-string Lua error");
                failed = !atomic_load(&cancel);
                break;
            }
        }
    }
cleanup:
    set_state("stopping", NULL);
    atomic_store(&cancel, true);
    a->closing = true;
    if (L) {
        lua_close(L);
        L = NULL;
    }
    cyd_ui_lock();
    if (a->root)
        lv_obj_clean(a->root);
    cyd_ui_show_app(false);
    cyd_ui_unlock();
    free(source);
    xQueueReset(events);
    cyd_log(a->manifest.id, failed ? error : "Stopped; UI and Lua resources released");
    set_state(failed ? "crashed" : "stopped", failed ? error : "");
    free(a);
    xSemaphoreGive(done);
    vTaskDelete(NULL);
}
esp_err_t cyd_runtime_init(void) {
    guard = xSemaphoreCreateMutex();
    log_guard = xSemaphoreCreateMutex();
    done = xSemaphoreCreateBinary();
    events = xQueueCreate(16, sizeof(int));
    if (!guard || !log_guard || !done || !events)
        return ESP_ERR_NO_MEM;
    xSemaphoreGive(done);
    return ESP_OK;
}
esp_err_t cyd_runtime_start(const char *p, const char *r, const cyd_manifest_t *m) {
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
    app_t *a = calloc(1, sizeof(*a));
    if (!a) {
        xSemaphoreGive(done);
        return ESP_ERR_NO_MEM;
    }
    snprintf(a->path, sizeof(a->path), "%s", p);
    snprintf(a->rev, sizeof(a->rev), "%s", r);
    a->manifest = *m;
    xSemaphoreTake(guard, portMAX_DELAY);
    snprintf(package_id, sizeof(package_id), "%s", m->id);
    snprintf(revision, sizeof(revision), "%s", r);
    strcpy(state, "starting");
    last_error[0] = 0;
    xSemaphoreGive(guard);
    atomic_store(&lua_bytes, 0);
    atomic_store(&lua_peak, 0);
    atomic_store(&cancel, false);
    if (xTaskCreate(run_task, "lua_app", 12288, a, 3, NULL) != pdPASS) {
        free(a);
        set_state("crashed", "Task allocation failed");
        xSemaphoreGive(done);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
