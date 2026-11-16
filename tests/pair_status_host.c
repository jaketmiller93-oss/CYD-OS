#include "cyd_pair.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    cJSON *
        local = cJSON_Parse(
           "{\"firmware\":\"0.5.0\",\"model\":\"C5\",\"target\":\"esp32c5\",\"ip\":\"192.168.4.1\","
           "\"sd\":true,\"storage_free\":100,\"heap_free\":5,\"app\":{\"state\":\"stopped\"}}"),
       *host = cJSON_Parse("{\"firmware\":\"0.5.0\",\"boot_confirmed\":true,\"sd\":true,\"storage_"
                           "free\":200,\"heap_free\":900,\"app\":{\"state\":\"running\",\"package_"
                           "id\":\"com.cydos.calculator\"}}"),
       *manager = cJSON_Parse(
           "{\"firmware\":\"0.5.0\",\"boot_confirmed\":true,\"sd\":true,\"storage_free\":100}");
    cJSON *o = cyd_pair_dashboard_merge(local, host, manager);
    assert(o);
    assert(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(o, "system_ready")));
    assert(!strcmp(
        cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(o, "app"), "state")
            ->valuestring,
        "running"));
    assert(cJSON_GetObjectItemCaseSensitive(o, "storage_free")->valueint == 200);
    assert(cJSON_GetObjectItemCaseSensitive(o, "heap_free")->valueint == 900);
    assert(!strcmp(cJSON_GetObjectItemCaseSensitive(o, "ip")->valuestring, "192.168.4.1"));
    assert(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(o, "manager"),
                                            "storage_free")
               ->valueint == 100);
    cJSON_Delete(o);
    o = cyd_pair_dashboard_merge(
        cJSON_Parse("{\"firmware\":\"0.5.0\",\"app\":{\"state\":\"stopped\"}}"), NULL,
        cJSON_Parse("{\"boot_confirmed\":true,\"storage_free\":100}"));
    assert(o && !cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(o, "system_ready")));
    assert(!strcmp(
        cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(o, "app"), "state")
            ->valuestring,
        "unavailable"));
    assert(!cJSON_GetObjectItemCaseSensitive(o, "storage_free")->valueint);
    assert(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(o, "manager"),
                                            "storage_free")
               ->valueint == 100);
    cJSON_Delete(o);
    o = cyd_pair_dashboard_merge(
        cJSON_Parse("{\"firmware\":\"0.5.0\"}"),
        cJSON_Parse("{\"firmware\":\"0.4.2s\",\"boot_confirmed\":true,\"app\":{}}"),
        cJSON_Parse("{\"boot_confirmed\":true}"));
    assert(o && !cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(o, "system_ready")));
    cJSON_Delete(o);
    cJSON *logs = cyd_pair_logs_merge(cJSON_Parse("[\"old\",\"manager\"]"),
                                      cJSON_Parse("[\"application error\"]"), 1);
    assert(cJSON_GetArraySize(logs) == 2);
    assert(!strcmp(cJSON_GetArrayItem(logs, 0)->valuestring, "[C5] manager"));
    assert(!strcmp(cJSON_GetArrayItem(logs, 1)->valuestring, "[S3] application error"));
    cJSON_Delete(logs);
    logs = cyd_pair_logs_merge(cJSON_Parse("[\"manager\"]"), NULL, 32);
    assert(cJSON_GetArraySize(logs) == 2);
    assert(strstr(cJSON_GetArrayItem(logs, 1)->valuestring, "unavailable"));
    cJSON_Delete(logs);
    puts("PASS unified status defaults to S3 apps/storage, keeps C5 access/manager metrics, and "
         "detects missing or mismatched hosts");
    return 0;
}
