#include "cyd.h"
#include "sdkconfig.h"
#include "connections.h"
#include "projects.h"
#include "settings.h"
#ifdef CONFIG_CYD_PAIRED_SYSTEM
#include "cyd_pair.h"
#include "pair_ops.h"
#include "pair_ui.h"
#include "cyd_builtin.h"
#endif
#include "cyd_device.h"
#include "display_geometry.h"
#include "cyd_ota.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_psram.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/queue.h"
static void text_if_changed(lv_obj_t *label, const char *value) {
    if (strcmp(lv_label_get_text(label), value)) lv_label_set_text(label, value);
}
static SemaphoreHandle_t lock;
static lv_obj_t *home, *app, *status, *details, *content, *clock_text, *home_button, *info;
static lv_obj_t *back_button,*next_button,*page_counter;
static void navigation_update(lv_timer_t *);
static void navigation_layout(void);
static lv_display_t *display;
static uint16_t *rotated;
static unsigned native_width, native_height;
static void home_grid(void);
static QueueHandle_t actions;
static unsigned width, height;
static uint32_t peak_handler_us, peak_touch_us, peak_flush_us, flush_calls;
static uint32_t performance_at;
static void input_read(lv_indev_t *input, lv_indev_data_t *data) {
    int64_t started = esp_timer_get_time();
    cyd_board_touch(input, data);
    uint32_t elapsed = (uint32_t)(esp_timer_get_time() - started);
    if (elapsed > peak_touch_us) peak_touch_us = elapsed;
}
typedef struct {
    int kind;
    char revision[17];
} action_t;
void cyd_ui_lock(void) {
    xSemaphoreTakeRecursive(lock, portMAX_DELAY);
}
void cyd_ui_unlock(void) {
    xSemaphoreGiveRecursive(lock);
}
lv_obj_t *cyd_ui_app_root(void) {
    return app;
}
void cyd_ui_show_app(bool show) {
    if (show) {
        cyd_connections_close();
        cyd_projects_ui_close();
        cyd_settings_close();
        lv_obj_add_flag(home, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(app, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(app, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(home, LV_OBJ_FLAG_HIDDEN);
    }
}
static uint32_t tick(void) {
    return esp_timer_get_time() / 1000;
}
static void go_home(lv_event_t *e) {
    (void)e;
    cyd_runtime_request_stop();
    cyd_device_revert();
    cyd_settings_close();
#ifdef CONFIG_CYD_PAIRED_SYSTEM
    cyd_pair_ui_close();
#endif
    lv_obj_add_flag(info, LV_OBJ_FLAG_HIDDEN);
    cyd_connections_close();
    cyd_projects_ui_close();
    lv_obj_scroll_to_y(home, 0, LV_ANIM_OFF);
}
static bool navigation_route(int direction,cyd_nav_state_t *state) {
#ifdef CONFIG_CYD_PAIRED_SYSTEM
    if(cyd_pair_ui_nav(direction,state))return true;
#endif
    if(cyd_connections_nav(direction,state))return true;
    if(cyd_projects_ui_nav(direction,state))return true;
    if(cyd_settings_nav(direction,state))return true;
    if(!lv_obj_has_flag(info,LV_OBJ_FLAG_HIDDEN)) {
        if(direction && !cyd_pager_move(content,direction) && direction<0)lv_obj_add_flag(info,LV_OBJ_FLAG_HIDDEN);
        cyd_pager_state(content,state);state->active=true;state->back=true;return true;
    }
    if(!lv_obj_has_flag(app,LV_OBJ_FLAG_HIDDEN)) {
        *state=(cyd_nav_state_t){.active=true,.back=true,.page=1,.pages=1};
        if(direction<0)go_home(NULL);
        return true;
    }
    *state=(cyd_nav_state_t){.page=1,.pages=1};return false;
}
static void navigation_click(lv_event_t *e) {
    cyd_nav_state_t state;
    navigation_route((int)(intptr_t)lv_event_get_user_data(e),&state);
    navigation_update(NULL);
}
static void navigation_update(lv_timer_t *timer) {
    (void)timer;
    cyd_nav_state_t state;navigation_route(0,&state);
    if(state.back)lv_obj_remove_state(back_button,LV_STATE_DISABLED);else lv_obj_add_state(back_button,LV_STATE_DISABLED);
    if(state.next)lv_obj_remove_state(next_button,LV_STATE_DISABLED);else lv_obj_add_state(next_button,LV_STATE_DISABLED);
    char text[32];snprintf(text,sizeof(text),"%u / %u",state.page,state.pages);
    text_if_changed(page_counter,text);
}
static void navigation_layout(void) {
    int gap=8,cell=((int)width-4*gap)/3;
    lv_obj_t *buttons[]={back_button,home_button,next_button};
    for(unsigned i=0;i<3;i++) {
        lv_obj_set_pos(buttons[i],gap+i*(cell+gap),height-50);
        lv_obj_set_size(buttons[i],cell,44);
    }
}
static void menu(lv_event_t *e) {
    action_t a = {.kind = (int)(intptr_t)lv_event_get_user_data(e)};
    xQueueSend(actions, &a, 0);
}
#ifdef CONFIG_CYD_PAIRED_SYSTEM
bool cyd_ui_post_action(int kind) {action_t a={.kind=kind};return xQueueSend(actions,&a,0)==pdTRUE;}
#endif
static void action_task(void *arg) {
    (void)arg;
    action_t a;
    while (xQueueReceive(actions, &a, portMAX_DELAY)) {
#ifdef CONFIG_CYD_PAIRED_SYSTEM
        if(a.kind>=100 && a.kind<100+(int)cyd_builtin_count()) {cyd_builtin_launch((unsigned)(a.kind-100));continue;}
        if(CYD_PAIR_MANAGER && cyd_pair_ui_action(a.kind))continue;
        if(a.kind==2){cyd_ui_lock();cyd_settings_open_diagnostics();cyd_ui_unlock();continue;}
        if(a.kind==4){cyd_ui_lock();cyd_connections_open();cyd_ui_unlock();continue;}
#endif
        if (a.kind == 1) {
            cyd_ui_lock();
            cyd_projects_ui_open();
            cyd_ui_unlock();
            continue;
        }
        if (a.kind == 0) {
            cyd_ui_lock();
            cyd_settings_open();
            cyd_ui_unlock();
            continue;
        }
        cJSON *data = a.kind == 3 ? cyd_logs() : NULL;
#ifdef CONFIG_CYD_PAIRED_SYSTEM
        if(CYD_PAIR_MANAGER && a.kind==3) {
            while(cJSON_GetArraySize(data)>6)cJSON_DeleteItemFromArray(data,0);
            cJSON *remote=cyd_pair_json(CYD_PAIR_LOGS,NULL),*v;int count=cJSON_GetArraySize(remote),index=0;
            cJSON_ArrayForEach(v,remote)if(cJSON_IsString(v)&&index++>=count-6) {
                char line[352];snprintf(line,sizeof(line),"[S3] %s",v->valuestring);cJSON_AddItemToArray(data,cJSON_CreateString(line));
            }
            cJSON_Delete(remote);
        }
#endif
        cyd_ui_lock();
        lv_obj_remove_flag(info, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(info);
        cyd_pager_reset(content);
        lv_obj_clean(content);
        lv_obj_set_flex_flow(content,LV_FLEX_FLOW_COLUMN);
        details = lv_label_create(content);
        lv_obj_set_width(details, width - 80);
        if (a.kind == 3) {
            char b[4096] = "";
            size_t used = 0;
            cJSON *v;
            cJSON_ArrayForEach(v, data) {
                size_t n = strlen(v->valuestring);
                if (n + used + 2 >= sizeof(b))
                    break;
                memcpy(b + used, v->valuestring, n);
                used += n;
                b[used++] = '\n';
                b[used] = 0;
            }
            lv_label_set_text(details, used ? b : "No logs yet.");
        }
        cyd_pager_attach(content);
        cyd_ui_unlock();
        cJSON_Delete(data);
    }
}
static void refresh(lv_timer_t *t) {
    (void)t;
    char ip[32];
    cyd_network_address(ip, sizeof(ip));
    text_if_changed(status, ip);
    cyd_device_status_t state;
    cyd_device_snapshot(&state);
    text_if_changed(clock_text, state.clock_valid ? state.clock + 11 : "CYD OS");
}
static void flush(lv_display_t *d, const lv_area_t *a, uint8_t *pixels) {
    int64_t started = esp_timer_get_time();
    flush_calls++;
    unsigned r = lv_display_get_rotation(d);
    if (!r) {
#if defined(CONFIG_CYD_PAIRED_SYSTEM) && CYD_PAIR_MANAGER
        /* LVGL draws in PSRAM on the manager. SPI always receives the retained
         * internal DMA buffer, whose lifetime ends at color_done/flush_ready. */
        size_t bytes = (size_t)(a->x2 - a->x1 + 1) * (a->y2 - a->y1 + 1) * 2;
        memcpy(rotated, pixels, bytes);
        cyd_board_flush(d, a, (uint8_t *)rotated);
#else
        cyd_board_flush(d, a, pixels);
#endif
        uint32_t elapsed = (uint32_t)(esp_timer_get_time() - started);
        if (elapsed > peak_flush_us) peak_flush_us = elapsed;
        return;
    }
    lv_area_t physical;
    if (r == 1)
        physical = (lv_area_t){a->y1, native_height - 1 - a->x2, a->y2, native_height - 1 - a->x1};
    else if (r == 2)
        physical = (lv_area_t){native_width - 1 - a->x2, native_height - 1 - a->y2,
                               native_width - 1 - a->x1, native_height - 1 - a->y1};
    else
        physical = (lv_area_t){native_width - 1 - a->y2, a->x1, native_width - 1 - a->y1, a->x2};
    cyd_rotate_rgb565((const uint16_t *)pixels, rotated, a->x2 - a->x1 + 1, a->y2 - a->y1 + 1, r);
    cyd_board_flush(d, &physical, (uint8_t *)rotated);
    uint32_t elapsed = (uint32_t)(esp_timer_get_time() - started);
    if (elapsed > peak_flush_us) peak_flush_us = elapsed;
}
static void home_grid(void) {
    lv_obj_clean(home);
#ifdef CONFIG_CYD_PAIRED_SYSTEM
#if defined(CONFIG_IDF_TARGET_ESP32S3)
    const char *names[] = {"Calculator","Calendar","Clock","Notes","Contacts","Apps","Local logs"};
    const int menu_actions[] = {100,101,102,103,104,1,3};
    const char *icons[] = {LV_SYMBOL_PLUS,LV_SYMBOL_LIST,LV_SYMBOL_REFRESH,LV_SYMBOL_EDIT,LV_SYMBOL_CALL,LV_SYMBOL_DIRECTORY,LV_SYMBOL_WARNING};
#else
    const char *names[] = {"Settings","Diagnostics","Connections","Files","Logs","Bluetooth LE"};
    const int menu_actions[] = {0,2,4,5,3,6};
    const char *icons[] = {LV_SYMBOL_SETTINGS,LV_SYMBOL_EYE_OPEN,LV_SYMBOL_WIFI,LV_SYMBOL_DIRECTORY,LV_SYMBOL_LIST,LV_SYMBOL_BLUETOOTH};
#endif
    const uint32_t colors[] = {0x777c86,0x30a46c,0x0a84ff,0xff9500,0x8b5cf6,0x2563eb,0x64748b};
    int count=(int)(sizeof(names)/sizeof(*names));
#else
    const char *names[] = {"Settings", "Apps", "Logs"};
    const int menu_actions[] = {0,1,3};
    const char *icons[] = {LV_SYMBOL_SETTINGS, LV_SYMBOL_DIRECTORY, LV_SYMBOL_LIST};
    const uint32_t colors[] = {0x777c86, 0xff9500, 0x0a84ff};
    int count=3;
#endif
    int columns=width>=height?3:2;
    int rows=(count+columns-1)/columns;
    int cell_w=width/columns,cell_h=(height-88)/rows;
    int diameter = cell_h - 23;
    if (diameter > 96)
        diameter = 96;
    if (diameter > cell_w - 24)
        diameter = cell_w - 24;
    for (int n = 0; n < count; n++) {
        lv_obj_t *b = lv_button_create(home);
        lv_obj_set_size(b, diameter, diameter);
        int x=(count==3 && columns==2 && n==2)?((int)width-diameter)/2:
            (n%columns)*cell_w+(cell_w-diameter)/2;
        int y=(n/columns)*cell_h+(cell_h-diameter-23)/2;
        lv_obj_set_pos(b,x,y);
        lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(b, lv_color_hex(colors[n]), 0);
        lv_obj_set_style_shadow_width(b, 0, 0);
        lv_obj_t *l = lv_label_create(b);
        lv_label_set_text(l, icons[n]);
#if LV_FONT_MONTSERRAT_28
        lv_obj_set_style_text_font(l, &lv_font_montserrat_28, 0);
#endif
        lv_obj_center(l);
        lv_obj_add_event_cb(b, menu, LV_EVENT_CLICKED, (void *)(intptr_t)menu_actions[n]);
        l = lv_label_create(home);
        lv_label_set_text(l, names[n]);
        lv_obj_set_width(l, cell_w);
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(l,x+(diameter-cell_w)/2,y+diameter+5);
    }
}
esp_err_t cyd_ui_apply_rotation(unsigned rotation) {
    if (!display || rotation > 3)
        return ESP_ERR_INVALID_STATE;
    cyd_ui_lock();
    lv_display_set_rotation(display, rotation);
    width = lv_display_get_horizontal_resolution(display);
    height = lv_display_get_vertical_resolution(display);
    lv_obj_set_width(status, width - 120);
    lv_obj_set_pos(status, 112, 10);
    lv_obj_set_size(home, width, height - 88);
    lv_obj_set_size(app, width, height - 88);
    lv_obj_set_size(info, width, height - 56);
    lv_obj_set_size(content, width - 16, height - 72);
    navigation_layout();
    home_grid();
#ifdef CONFIG_CYD_PAIRED_SYSTEM
    cyd_pair_ui_resize(width,height-88);
#endif
    cyd_pager_reset(content);
    lv_obj_set_flex_flow(content,LV_FLEX_FLOW_COLUMN);
    if(details && !lv_obj_has_flag(info,LV_OBJ_FLAG_HIDDEN))cyd_pager_attach(content);
    cyd_connections_resize(width, height);
    cyd_projects_ui_resize(width, height);
    cyd_settings_resize(width, height);
    lv_obj_invalidate(lv_screen_active());
    cyd_ui_unlock();
    return ESP_OK;
}
static void task(void *arg) {
    (void)arg;
    while (true) {
        cyd_ui_lock();
        int64_t started = esp_timer_get_time();
        uint32_t ms = lv_timer_handler();
        uint32_t elapsed = (uint32_t)(esp_timer_get_time() - started);
        if (elapsed > peak_handler_us) peak_handler_us = elapsed;
        uint32_t stamp = (uint32_t)(esp_timer_get_time() / 1000);
        if (stamp - performance_at >= 10000) {
            static char metrics[160];
            snprintf(metrics, sizeof(metrics), "10s peaks: handler=%luus touch=%luus flush=%luus chunks=%lu",
                (unsigned long)peak_handler_us, (unsigned long)peak_touch_us,
                (unsigned long)peak_flush_us, (unsigned long)flush_calls);
            cyd_log("ui-perf", metrics);
            peak_handler_us = peak_touch_us = peak_flush_us = flush_calls = 0;
            performance_at = stamp;
        }
        cyd_ota_health_heartbeat(CYD_HEALTH_UI);
        cyd_ui_unlock();
        if (ms < 5)
            ms = 5;
        if (ms > 20)
            ms = 20;
        vTaskDelay(pdMS_TO_TICKS(ms));
    }
}
static esp_err_t ui_failure(const char *stage, esp_err_t error) {
    char message[192];
    snprintf(message, sizeof(message), "%s failed (%d); internal largest=%u; PSRAM largest=%u",
        stage, (int)error,
        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    cyd_log("ui", message);
    return error;
}
esp_err_t cyd_ui_init(void) {
    lock = xSemaphoreCreateRecursiveMutex();
    if (!lock)
        return ui_failure("UI mutex", ESP_ERR_NO_MEM);
    native_width = cyd_board_width();
    native_height = cyd_board_height();
    width = native_width;
    height = native_height;
    lv_init();
    lv_tick_set_cb(tick);
    lv_display_t *d = lv_display_create(width, height);
    display = d;
    if (!d)
        return ui_failure("LVGL display", ESP_ERR_NO_MEM);
    /* C5 SPI needs one retained internal DMA source. The paired manager draws
     * in PSRAM, then copies/rotates into that source before each async transfer.
     * S3's RGB driver already copies CPU scratch data into its framebuffer. */
#ifdef CONFIG_IDF_TARGET_ESP32S3
    const unsigned buffer_caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
    const unsigned rotate_caps = buffer_caps;
#elif defined(CONFIG_CYD_PAIRED_SYSTEM) && CYD_PAIR_MANAGER
    const unsigned buffer_caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
    const unsigned rotate_caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA;
#else
    const unsigned buffer_caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA;
    const unsigned rotate_caps = buffer_caps;
#endif
    void *buf = heap_caps_malloc(native_width * 24 * 2, buffer_caps);
    if (!buf) {
        lv_display_delete(d); display = NULL;
        return ui_failure("LVGL draw buffer", ESP_ERR_NO_MEM);
    }
    rotated = heap_caps_malloc(native_width * 24 * 2, rotate_caps);
    if (!rotated) {
        free(buf); lv_display_delete(d); display = NULL;
        return ui_failure("Rotation buffer", ESP_ERR_NO_MEM);
    }
    lv_display_set_color_format(d, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(d, buf, NULL, native_width * 24 * 2, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(d, flush);
    cyd_device_status_t state;
    cyd_device_snapshot(&state);
    lv_display_set_rotation(d, state.rotation);
    width = lv_display_get_horizontal_resolution(d);
    height = lv_display_get_vertical_resolution(d);
    esp_err_t backlight = cyd_board_set_brightness(state.brightness);
    if (backlight)
        return ui_failure("Backlight", backlight);
    lv_indev_t *i = lv_indev_create();
    if (!i)
        return ui_failure("Touch input object", ESP_ERR_NO_MEM);
    lv_indev_set_type(i, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(i, input_read);
    lv_theme_t *theme =
        lv_theme_default_init(d, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_CYAN),
                              true, LV_FONT_DEFAULT);
    lv_display_set_theme(d, theme);
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    clock_text = lv_label_create(screen);
    lv_obj_set_pos(clock_text, 10, 8);
    lv_label_set_text(clock_text, "CYD OS");
#if LV_FONT_MONTSERRAT_20
    lv_obj_set_style_text_font(clock_text, &lv_font_montserrat_20, 0);
#endif
    status = lv_label_create(screen);
    lv_obj_set_pos(status, 112, 10);
    lv_obj_set_width(status, width - 120);
    lv_label_set_long_mode(status, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(status, LV_TEXT_ALIGN_RIGHT, 0);
    home = lv_obj_create(screen);
    lv_obj_set_pos(home, 0, 34);
    lv_obj_set_size(home, width, height - 88);
    lv_obj_set_style_pad_all(home, 0, 0);
    lv_obj_set_style_border_width(home, 0, 0);
    lv_obj_set_style_bg_opa(home, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(home, LV_OBJ_FLAG_SCROLLABLE);
    actions = xQueueCreate(8, sizeof(action_t));
    if (!actions)
        return ui_failure("Shell action queue", ESP_ERR_NO_MEM);
    home_grid();
#ifdef CONFIG_CYD_PAIRED_SYSTEM
    cyd_pair_ui_resize(width,height-88);
#endif
    app = lv_obj_create(screen);
    lv_obj_set_pos(app, 0, 34);
    lv_obj_set_size(app, width, height - 88);
    lv_obj_set_style_pad_all(app, 12, 0);
    lv_obj_remove_flag(app,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(app, LV_OBJ_FLAG_HIDDEN);
    info = lv_obj_create(screen);
    lv_obj_set_pos(info, 0, 0);
    lv_obj_set_size(info, width, height - 56);
    lv_obj_set_style_pad_all(info, 0, 0);
    lv_obj_set_style_bg_opa(info, LV_OPA_COVER, 0);
    lv_obj_add_flag(info, LV_OBJ_FLAG_HIDDEN);
    content = lv_obj_create(info);
    lv_obj_set_pos(content, 0, 0);
    lv_obj_set_size(content, width - 16, height - 72);
    lv_obj_t **buttons[]={&back_button,&home_button,&next_button};
    const char *titles[]={"Back","Home","Next"};
    for(unsigned n=0;n<3;n++) {
        *buttons[n]=lv_button_create(screen);
        lv_obj_set_style_radius(*buttons[n],12,0);
        lv_obj_set_style_pad_all(*buttons[n],2,0);
        lv_obj_set_style_bg_color(*buttons[n],lv_color_hex(0x29292c),0);
        lv_obj_set_style_shadow_width(*buttons[n],0,0);
        lv_obj_t *label=lv_label_create(*buttons[n]);lv_label_set_text(label,titles[n]);lv_obj_center(label);
        if(n==1) {
            lv_obj_align(label,LV_ALIGN_TOP_MID,0,1);
            page_counter=lv_label_create(*buttons[n]);lv_label_set_text(page_counter,"1 / 1");
#if LV_FONT_MONTSERRAT_12
            lv_obj_set_style_text_font(page_counter,&lv_font_montserrat_12,0);
#endif
            lv_obj_align(page_counter,LV_ALIGN_BOTTOM_MID,0,-1);
            lv_obj_add_event_cb(*buttons[n],go_home,LV_EVENT_CLICKED,NULL);
        } else lv_obj_add_event_cb(*buttons[n],navigation_click,LV_EVENT_CLICKED,(void *)(intptr_t)(n==0?-1:1));
    }
    navigation_layout();
    cyd_settings_init(screen, width, height);
#ifdef CONFIG_CYD_PAIRED_SYSTEM
    cyd_pair_ui_init(lv_screen_active(),width,height-88);
#endif
    cyd_connections_init(screen, width, height);
#if !defined(CONFIG_CYD_PAIRED_SYSTEM) || !CYD_PAIR_MANAGER
    /* Paired C5 is the manager screen. App folders run only on the S3 host;
     * do not spend an 8 KiB internal task stack on an unreachable C5 view. */
    esp_err_t folders = cyd_projects_ui_init(screen, width, height);
    if (folders)
        return ui_failure("Apps UI", folders);
#endif
    refresh(NULL);
    lv_timer_create(refresh, 2000, NULL);
    navigation_update(NULL);
    lv_timer_create(navigation_update,200,NULL);
    if (xTaskCreate(action_task, "shell_actions", 12288, NULL, 3, NULL) != pdPASS)
        return ui_failure("Shell task stack", ESP_ERR_NO_MEM);
    if (xTaskCreate(task, "ui", 8192, NULL, 5, NULL) != pdPASS)
        return ui_failure("UI task stack", ESP_ERR_NO_MEM);
    cyd_log("ui", "UI initialized");
    return ESP_OK;
}
