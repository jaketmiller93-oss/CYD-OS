#pragma once
#include "lvgl.h"
#include "esp_err.h"
esp_err_t cyd_projects_ui_init(lv_obj_t *parent, unsigned width, unsigned height);
void cyd_projects_ui_open(void);
void cyd_projects_ui_close(void);

void cyd_projects_ui_resize(unsigned width, unsigned height);
