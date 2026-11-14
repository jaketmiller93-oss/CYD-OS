#pragma once
#include "lvgl.h"
void cyd_connections_init(lv_obj_t *parent, unsigned width, unsigned height);
void cyd_connections_open(void);
void cyd_connections_close(void);

void cyd_connections_open_device(void);
void cyd_connections_resize(unsigned width, unsigned height);
