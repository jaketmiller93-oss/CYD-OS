#pragma once
#include "cyd.h"
/* Experimental diagnostic UART link. Does not route app or network services. */
esp_err_t cyd_peer_start(void);
cJSON *cyd_peer_status(void);
