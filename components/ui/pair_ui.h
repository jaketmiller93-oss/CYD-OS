#pragma once
#include "pager.h"
void cyd_pair_ui_init(lv_obj_t *, unsigned, unsigned);
void cyd_pair_ui_resize(unsigned, unsigned);
void cyd_pair_ui_close(void);
bool cyd_pair_ui_nav(int, cyd_nav_state_t *);
bool cyd_pair_ui_action(int);
bool cyd_ui_post_action(int);
