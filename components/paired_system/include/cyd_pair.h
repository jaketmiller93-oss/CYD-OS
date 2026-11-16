#pragma once
#include "cyd.h"
#include "sdkconfig.h"
#if defined(CONFIG_CYD_PAIRED_SYSTEM) && defined(CONFIG_IDF_TARGET_ESP32S3)
#define CYD_PAIR_HOST 1
#else
#define CYD_PAIR_HOST 0
#endif
#if defined(CONFIG_CYD_PAIRED_SYSTEM) && defined(CONFIG_IDF_TARGET_ESP32C5) &&                     \
    !defined(CONFIG_IDF_TARGET_ESP32S3)
#define CYD_PAIR_MANAGER 1
#else
#define CYD_PAIR_MANAGER 0
#endif
#ifdef CONFIG_CYD_PAIRED_SYSTEM
#define CYD_PAIR_ENABLED 1
#else
#define CYD_PAIR_ENABLED 0
#endif
#define CYD_PAIR_REQUEST_MAX 8192u
#define CYD_PAIR_RESPONSE_MAX 32768u
/* Calls are for service workers, never the LVGL thread or UART worker. */
esp_err_t cyd_pair_memory_init(void);
esp_err_t cyd_pair_start(void);
esp_err_t cyd_pair_rpc(unsigned op, const void *, size_t, void *, size_t, size_t *);
cJSON *cyd_pair_json(unsigned op, const cJSON *request);
cJSON *cyd_pair_status(void);

bool cyd_pair_installing(void);

/* Takes ownership of all three objects; primary app/storage metrics are S3. */
cJSON *cyd_pair_dashboard_merge(cJSON *local, cJSON *host, cJSON *manager);

void cyd_pair_elf_sha256(char out[65]);

/* Takes ownership; recent logs are source-tagged and bounded per board. */
cJSON *cyd_pair_logs_merge(cJSON *manager,cJSON *host,unsigned per_board);
