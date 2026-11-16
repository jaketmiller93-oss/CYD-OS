#pragma once
#include "lvgl.h"
void cyd_settings_init(lv_obj_t *parent, unsigned width, unsigned height);
void cyd_settings_open(void);
void cyd_settings_close(void);
void cyd_settings_resize(unsigned width, unsigned height);

void cyd_settings_open_diagnostics(void);

#include "pager.h"
bool cyd_settings_nav(int direction, cyd_nav_state_t *state);
