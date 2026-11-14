#include "system_sdk/cyd.h"
struct host_queue {
    unsigned count, head, capacity;
    size_t size;
    unsigned char data[16][512];
};
static struct host_queue queues[16];
static unsigned queue_count;
static unsigned char stored[8][2048], pending[8][2048];
static size_t sizes[8], pending_sizes[8];
static char spaces[8][32];
static unsigned space_count;
static bool fail_save, fail_task, maintenance;
static int64_t clock_ms;
static uint32_t random_state = 431;
void host_reset(void) {
    memset(stored, 0, sizeof(stored));
    memset(sizes, 0, sizeof(sizes));
    memset(pending_sizes, 0, sizeof(pending_sizes));
    space_count = queue_count = 0;
    maintenance = fail_save = fail_task = false;
    clock_ms = 1000;
}
void host_tick(unsigned ms) {
    clock_ms += ms;
}
void host_fail_save(bool x) {
    fail_save = x;
}
void host_fail_task(bool x) {
    fail_task = x;
}
SemaphoreHandle_t xSemaphoreCreateMutex(void) {
    return (void *)1;
}
int xSemaphoreTake(SemaphoreHandle_t s, unsigned t) {
    (void)s;
    (void)t;
    return 1;
}
int xSemaphoreGive(SemaphoreHandle_t s) {
    (void)s;
    return 1;
}
QueueHandle_t xQueueCreate(unsigned n, size_t size) {
    assert(n <= 16 && size <= 512 && queue_count < 16);
    struct host_queue *q = &queues[queue_count++];
    memset(q, 0, sizeof(*q));
    q->capacity = n;
    q->size = size;
    return q;
}
int xQueueSend(QueueHandle_t q, const void *p, unsigned t) {
    (void)t;
    if (q->count == q->capacity)
        return 0;
    memcpy(q->data[(q->head + q->count) % q->capacity], p, q->size);
    q->count++;
    return 1;
}
int xQueueReceive(QueueHandle_t q, void *p, unsigned t) {
    (void)t;
    if (!q->count)
        return 0;
    memcpy(p, q->data[q->head], q->size);
    q->head = (q->head + 1) % q->capacity;
    q->count--;
    return 1;
}
static struct {
    void (*fn)(void *);
    void *arg;
    char name[32];
} tasks[16];
static unsigned task_count;
void host_run_task(const char *name) {
    for (unsigned i = 0; i < task_count; i++)
        if (!strcmp(tasks[i].name, name))
            tasks[i].fn(tasks[i].arg);
}
int xTaskCreate(void (*f)(void *), const char *name, unsigned stack, void *arg, unsigned priority,
                void *handle) {
    if (!fail_task && task_count < 16) {
        tasks[task_count].fn = f;
        tasks[task_count].arg = arg;
        snprintf(tasks[task_count].name, 32, "%s", name);
        task_count++;
    }
    (void)name;
    (void)stack;
    (void)arg;
    (void)priority;
    (void)handle;
    return !fail_task;
}
void vTaskDelay(unsigned ms) {
    clock_ms += ms;
}
void vTaskDelete(void *p) {
    (void)p;
}
int64_t esp_timer_get_time(void) {
    return clock_ms * 1000;
}
uint32_t esp_random(void) {
    random_state = random_state * 1664525 + 1013904223;
    return random_state;
}
bool cyd_maintenance_active(void) {
    return maintenance;
}
bool cyd_maintenance_begin(void) {
    if (maintenance)
        return false;
    maintenance = true;
    return true;
}
void cyd_maintenance_end(void) {
    maintenance = false;
}
const char *cyd_system_version(void) {
    return "0.3.0";
}
void cyd_log(const char *c, const char *s) {
    (void)c;
    (void)s;
}
esp_err_t nvs_open(const char *name, int mode, nvs_handle_t *out) {
    (void)mode;
    for (unsigned i = 0; i < space_count; i++)
        if (!strcmp(spaces[i], name)) {
            *out = i;
            return 0;
        }
    assert(space_count < 8);
    strcpy(spaces[space_count], name);
    *out = space_count++;
    return 0;
}
esp_err_t nvs_get_blob(nvs_handle_t h, const char *key, void *out, size_t *n) {
    (void)key;
    if (!sizes[h])
        return ESP_ERR_NVS_NOT_FOUND;
    if (*n < sizes[h])
        return ESP_ERR_INVALID_SIZE;
    memcpy(out, stored[h], sizes[h]);
    *n = sizes[h];
    return 0;
}
esp_err_t nvs_set_blob(nvs_handle_t h, const char *key, const void *data, size_t n) {
    (void)key;
    assert(n < 2048);
    memcpy(pending[h], data, n);
    pending_sizes[h] = n;
    return 0;
}
esp_err_t nvs_get_str(nvs_handle_t h, const char *k, char *out, size_t *n) {
    return nvs_get_blob(h, k, out, n);
}
esp_err_t nvs_set_str(nvs_handle_t h, const char *k, const char *s) {
    return nvs_set_blob(h, k, s, strlen(s) + 1);
}
esp_err_t nvs_commit(nvs_handle_t h) {
    if (fail_save)
        return ESP_FAIL;
    memcpy(stored[h], pending[h], pending_sizes[h]);
    sizes[h] = pending_sizes[h];
    return 0;
}
esp_err_t esp_netif_sntp_init(const esp_sntp_config_t *c) {
    (void)c;
    return 0;
}
int host_settimeofday(const struct timeval *v, const void *z) {
    (void)v;
    (void)z;
    return 0;
}

SemaphoreHandle_t xSemaphoreCreateRecursiveMutex(void) {
    return xSemaphoreCreateMutex();
}
int xSemaphoreTakeRecursive(SemaphoreHandle_t s, unsigned t) {
    return xSemaphoreTake(s, t);
}
int xSemaphoreGiveRecursive(SemaphoreHandle_t s) {
    return xSemaphoreGive(s);
}
const char *esp_err_to_name(esp_err_t e) {
    return e ? "error" : "ESP_OK";
}
