#pragma once
#include "cyd.h"
esp_err_t cyd_pair_manager_start(void);
esp_err_t cyd_pair_manager_remember(const cJSON *settings);
