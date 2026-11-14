#pragma once
#include "esp_err.h"
#include "ota_policy.h"
#include <stdbool.h>
#include <stddef.h>
#define CYD_HEALTH_CORE 1u
#define CYD_HEALTH_BOARD 2u
#define CYD_HEALTH_API 4u
#define CYD_HEALTH_UI 8u
#define CYD_HEALTH_NETWORK 16u
#define CYD_HEALTH_ALL 31u
typedef enum {
    CYD_OS_BOOTING,
    CYD_OS_IDLE,
    CYD_OS_UPLOADING,
    CYD_OS_VERIFYING,
    CYD_OS_READY,
    CYD_OS_REBOOTING,
    CYD_OS_FAILED
} cyd_ota_stage_t;
typedef struct {
    cyd_ota_stage_t stage;
    bool boot_confirmed, pending_verify, rollback_available;
    size_t received, total;
    char running_slot[17], target_slot[17], version[32], candidate_version[32], sha256[65],
        message[160];
} cyd_ota_status_t;
esp_err_t cyd_ota_init(void);
void cyd_ota_health_report(unsigned bits);
void cyd_ota_health_heartbeat(unsigned bit);
void cyd_ota_snapshot(cyd_ota_status_t *out);
const cyd_os_image_desc_t *cyd_ota_image_descriptor(void);
esp_err_t cyd_ota_begin(const uint8_t *prefix, size_t n, size_t total, char *error,
                        size_t error_size);
esp_err_t cyd_ota_write(const void *, size_t);
esp_err_t cyd_ota_finish(char *error, size_t error_size);
void cyd_ota_abort(const char *reason);
esp_err_t cyd_ota_cancel(void);
esp_err_t cyd_ota_reboot(void);
esp_err_t cyd_ota_rollback(void);
