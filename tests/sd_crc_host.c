#include "../components/board/sd_crc_boot.c"
#include <assert.h>
#include <string.h>
static sdmmc_card_t card;
static int first, second;
static unsigned calls, waits, logs;
static bool expected_enable;
esp_err_t __real_sdmmc_send_cmd_crc_on_off(sdmmc_card_t *arg, bool enable) {
    assert(arg == &card && enable == expected_enable && calls < 2);
    return calls++ ? second : first;
}
void vTaskDelay(unsigned ticks) { assert(ticks == 10); waits++; }
void cyd_log(const char *tag, const char *message) {
    assert(!strcmp(tag, "sd") && message[0]); logs++;
}
static void check(int a, int b, bool enable, int expected, unsigned wanted_calls) {
    first = a; second = b; calls = waits = logs = 0; expected_enable = enable;
    assert(__wrap_sdmmc_send_cmd_crc_on_off(&card, enable) == expected);
    assert(calls == wanted_calls && waits == wanted_calls - 1);
    assert(logs == (wanted_calls == 2 ? 2u : 0u));
}
int main(void) {
    check(ESP_OK, ESP_ERR_TIMEOUT, true, ESP_OK, 1);
    check(ESP_ERR_NOT_SUPPORTED, ESP_OK, true, ESP_OK, 2);
    check(ESP_ERR_NOT_SUPPORTED, ESP_ERR_NOT_SUPPORTED, true, ESP_ERR_NOT_SUPPORTED, 2);
    check(ESP_ERR_NOT_SUPPORTED, ESP_ERR_TIMEOUT, true, ESP_ERR_TIMEOUT, 2);
    check(ESP_ERR_TIMEOUT, ESP_OK, true, ESP_ERR_TIMEOUT, 1);
    check(ESP_ERR_INVALID_CRC, ESP_OK, true, ESP_ERR_INVALID_CRC, 1);
    check(ESP_ERR_NOT_SUPPORTED, ESP_OK, false, ESP_ERR_NOT_SUPPORTED, 1);
    puts("PASS exact CMD59 retry, original errors, no CRC bypass and bounded attempts");
    return 0;
}
