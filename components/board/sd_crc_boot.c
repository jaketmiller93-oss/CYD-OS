/* Scoped backport of the newer IDF SPI CMD59 retry for the CrowPanel target.
 * Keep the installed SDK untouched and retain CRC checks on all data transfers. */
#include "cyd.h"
#include "sdmmc_cmd.h"
#include "freertos/task.h"
#include <stdio.h>

esp_err_t __real_sdmmc_send_cmd_crc_on_off(sdmmc_card_t *card, bool enable);
esp_err_t __wrap_sdmmc_send_cmd_crc_on_off(sdmmc_card_t *card, bool enable) {
    esp_err_t result = __real_sdmmc_send_cmd_crc_on_off(card, enable);
    if (enable && result == ESP_ERR_NOT_SUPPORTED) {
        cyd_log("sd", "CRC-enable command rejected (0x106); retrying once after 10 ms");
        vTaskDelay(pdMS_TO_TICKS(10));
        result = __real_sdmmc_send_cmd_crc_on_off(card, enable);
        if (!result) {
            cyd_log("sd", "CRC-enable retry accepted; integrity checks remain enabled");
        } else {
            char message[112];
            snprintf(message, sizeof(message), "CRC-enable retry failed (0x%x); card not mounted",
                     (unsigned)result);
            cyd_log("sd", message);
        }
    }
    return result;
}
