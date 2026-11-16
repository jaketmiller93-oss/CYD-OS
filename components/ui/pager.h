#pragma once
#include "lvgl.h"
typedef struct { bool active, back, next; unsigned page, pages; } cyd_nav_state_t;
/* Native flex layouts become discrete, non-scrollable pages. Long labels use a
 * clipped viewport with whole-line page cuts; original label handles stay live.
 * LV_OBJ_FLAG_USER_1 on a body label permits a page break before its next control. */
void cyd_pager_attach(lv_obj_t *);
void cyd_pager_refresh(lv_obj_t *);
void cyd_pager_reset(lv_obj_t *);
bool cyd_pager_move(lv_obj_t *,int);
void cyd_pager_state(lv_obj_t *,cyd_nav_state_t *);
bool cyd_pager_reveal(lv_obj_t *,lv_obj_t *);
