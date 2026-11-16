/* Exercise the actual paired C5 runtime, allocator and bounded log API. */
#define main legacy_runtime_main
#include "runtime_host.c"
#undef main

bool cyd_pair_installing(void) { return false; }
const char *cyd_builtin_source(const char *path, size_t *size) {
    (void)path; (void)size; return NULL;
}

int main(void) {
    _Static_assert(sizeof(logs) == sizeof(void *), "C5 history must leave internal BSS");
    host_psram_fail = 1;
    assert(cyd_runtime_init() == ESP_ERR_NO_MEM && !logs && !log_guard);
    host_psram_fail = 0;
    unsigned before = host_psram_calls;
    assert(cyd_runtime_init() == ESP_OK && logs && host_psram_calls == before + 1);
    char (*history)[320] = logs;
    for (unsigned i = 0; i < 70; i++) {
        char message[40]; snprintf(message, sizeof(message), "message %u", i);
        cyd_log("fixture", message);
    }
    cJSON *a = cyd_logs();
    assert(cJSON_GetArraySize(a) == 64);
    assert(strstr(cJSON_GetArrayItem(a, 0)->valuestring, "message 6"));
    assert(strstr(cJSON_GetArrayItem(a, 63)->valuestring, "message 69"));
    cJSON_Delete(a);
    char long_message[1000]; memset(long_message, 'x', sizeof(long_message) - 1);
    long_message[sizeof(long_message) - 1] = 0;
    cyd_log("fixture", long_message);
    a = cyd_logs();
    assert(cJSON_GetArraySize(a) == 64);
    assert(strlen(cJSON_GetArrayItem(a, 63)->valuestring) < 320);
    cJSON_Delete(a);
    launch_checks();
    assert(logs == history); /* Reinitialization does not replace/leak history. */
    free(logs); logs = NULL;
    puts("PASS C5 PSRAM log history: allocation failure, bounded wrap, truncation and runtime relaunch");
    return 0;
}
