#pragma once
#include "esp_err.h"
#include <stdint.h>
typedef struct {
    void *context;
    esp_err_t (*write)(void *,uint8_t);
    esp_err_t (*reset_bus)(void *);
    void (*wait_ms)(void *,unsigned);
    void (*retry_note)(void *,uint8_t,unsigned,esp_err_t);
} cyd_control_boot_io_t;
/* Startup only: no touch/UI workers may be using the bus yet. Commands are
 * idempotent STC enable-touch (250) / full backlight (0), never flash writes. */
esp_err_t cyd_control_boot(const cyd_control_boot_io_t *);
