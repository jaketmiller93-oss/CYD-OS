#pragma once
#include "lvgl.h"
void cyd_settings_init(lv_obj_t *parent, unsigned width, unsigned height);
void cyd_settings_open(void);
void cyd_settings_close(void);
void cyd_settings_resize(unsigned width, unsigned height);
