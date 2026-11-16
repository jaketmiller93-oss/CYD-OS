#pragma once
#include "cyd.h"
cJSON *cyd_pair_ble_status(void);
esp_err_t cyd_pair_ble_scan(void);
esp_err_t cyd_pair_ble_connect(unsigned index);
esp_err_t cyd_pair_ble_disconnect(void);
