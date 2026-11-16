#pragma once
#include "cyd_services.h"
#include "esp_err.h"
cyd_io_t cyd_pair_io(void);
esp_err_t cyd_pair_io_start(void);
