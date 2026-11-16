#pragma once
#include "lvgl.h"
void cyd_connections_init(lv_obj_t *parent, unsigned width, unsigned height);
void cyd_connections_open(void);
void cyd_connections_close(void);

void cyd_connections_open_device(void);
void cyd_connections_resize(unsigned width, unsigned height);

#include "pager.h"
bool cyd_connections_nav(int direction, cyd_nav_state_t *state);
