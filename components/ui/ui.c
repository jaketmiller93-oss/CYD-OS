#include "cyd.h"
#include "connections.h"
#include "projects.h"
#include "settings.h"
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
static SemaphoreHandle_t lock;
static lv_obj_t *home, *app, *status, *details, *content, *clock_text, *home_button, *info;
static lv_display_t *display;
static uint16_t *rotated;
static unsigned native_width, native_height;
static void home_grid(void);
static QueueHandle_t actions;
static unsigned width, height;
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
    lv_obj_add_flag(info, LV_OBJ_FLAG_HIDDEN);
    cyd_connections_close();
    cyd_projects_ui_close();
    lv_obj_scroll_to_y(home, 0, LV_ANIM_OFF);
}
static void menu(lv_event_t *e) {
    action_t a = {.kind = (int)(intptr_t)lv_event_get_user_data(e)};
    xQueueSend(actions, &a, 0);
}
static void action_task(void *arg) {
    (void)arg;
    action_t a;
    while (xQueueReceive(actions, &a, portMAX_DELAY)) {
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
        cyd_ui_lock();
        lv_obj_remove_flag(info, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(info);
        lv_obj_clean(content);
        details = lv_label_create(content);
        lv_obj_set_width(details, width - 80);
        if (a.kind == 2) {
            char b[256];
            snprintf(b, sizeof(b),
                     "CYD OS %s\nHeap: %u bytes; largest block: %u bytes\nPSRAM detected: %u "
                     "bytes\nSD card: %s\nPress Home to stop any running Lua app.",
                     cyd_system_version(), (unsigned)esp_get_free_heap_size(),
                     (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
                     (unsigned)esp_psram_get_size(), cyd_storage_ready() ? "ready" : "unavailable");
            lv_label_set_text(details, b);
        }
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
        lv_obj_scroll_to_view_recursive(details, LV_ANIM_OFF);
        cyd_ui_unlock();
        cJSON_Delete(data);
    }
}
static void refresh(lv_timer_t *t) {
    (void)t;
    char ip[32];
    cyd_network_address(ip, sizeof(ip));
    lv_label_set_text(status, ip);
    cyd_device_status_t state;
    cyd_device_snapshot(&state);
    lv_label_set_text(clock_text, state.clock_valid ? state.clock + 11 : "CYD OS");
}
static void flush(lv_display_t *d, const lv_area_t *a, uint8_t *pixels) {
    unsigned r = lv_display_get_rotation(d);
    if (!r) {
        cyd_board_flush(d, a, pixels);
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
}
static void home_grid(void) {
    lv_obj_clean(home);
    const char *names[] = {"Settings", "Apps", "Diagnostics", "Logs"};
    const char *icons[] = {LV_SYMBOL_SETTINGS, LV_SYMBOL_DIRECTORY, LV_SYMBOL_EYE_OPEN,
                           LV_SYMBOL_LIST};
    const uint32_t colors[] = {0x777c86, 0xff9500, 0x30d158, 0x0a84ff};
    int cell_w = width / 2, cell_h = (height - 88) / 2;
    int diameter = cell_h - 23;
    if (diameter > 96)
        diameter = 96;
    if (diameter > cell_w - 24)
        diameter = cell_w - 24;
    for (int n = 0; n < 4; n++) {
        lv_obj_t *b = lv_button_create(home);
        lv_obj_set_size(b, diameter, diameter);
        lv_obj_set_pos(b, (n % 2) * cell_w + (cell_w - diameter) / 2, (n / 2) * cell_h + 2);
        lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(b, lv_color_hex(colors[n]), 0);
        lv_obj_set_style_shadow_width(b, 0, 0);
        lv_obj_t *l = lv_label_create(b);
        lv_label_set_text(l, icons[n]);
#if LV_FONT_MONTSERRAT_28
        lv_obj_set_style_text_font(l, &lv_font_montserrat_28, 0);
#endif
        lv_obj_center(l);
        lv_obj_add_event_cb(b, menu, LV_EVENT_CLICKED, (void *)(intptr_t)n);
        l = lv_label_create(home);
        lv_label_set_text(l, names[n]);
        lv_obj_set_width(l, cell_w);
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(l, (n % 2) * cell_w, (n / 2) * cell_h + diameter + 5);
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
    lv_obj_set_pos(home_button, (width - 200) / 2, height - 50);
    home_grid();
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
        uint32_t ms = lv_timer_handler();
        cyd_ota_health_heartbeat(CYD_HEALTH_UI);
        cyd_ui_unlock();
        if (ms < 5)
            ms = 5;
        if (ms > 20)
            ms = 20;
        vTaskDelay(pdMS_TO_TICKS(ms));
    }
}
esp_err_t cyd_ui_init(void) {
    lock = xSemaphoreCreateRecursiveMutex();
    if (!lock)
        return ESP_ERR_NO_MEM;
    native_width = cyd_board_width();
    native_height = cyd_board_height();
    width = native_width;
    height = native_height;
    lv_init();
    lv_tick_set_cb(tick);
    lv_display_t *d = lv_display_create(width, height);
    display = d;
    if (!d)
        return ESP_ERR_NO_MEM;
    void *buf = heap_caps_malloc(native_width * 24 * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    rotated = heap_caps_malloc(native_width * 24 * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    if (!buf || !rotated)
        return ESP_ERR_NO_MEM;
    lv_display_set_color_format(d, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(d, buf, NULL, native_width * 24 * 2, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(d, flush);
    cyd_device_status_t state;
    cyd_device_snapshot(&state);
    lv_display_set_rotation(d, state.rotation);
    width = lv_display_get_horizontal_resolution(d);
    height = lv_display_get_vertical_resolution(d);
    if (cyd_board_set_brightness(state.brightness))
        return ESP_FAIL;
    lv_indev_t *i = lv_indev_create();
    if (!i)
        return ESP_ERR_NO_MEM;
    lv_indev_set_type(i, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(i, cyd_board_touch);
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
        return ESP_ERR_NO_MEM;
    home_grid();
    app = lv_obj_create(screen);
    lv_obj_set_pos(app, 0, 34);
    lv_obj_set_size(app, width, height - 88);
    lv_obj_set_style_pad_all(app, 12, 0);
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
    home_button = lv_button_create(screen);
    lv_obj_set_pos(home_button, (width - 200) / 2, height - 50);
    lv_obj_set_size(home_button, 200, 44);
    lv_obj_set_style_radius(home_button, 22, 0);
    lv_obj_set_style_bg_color(home_button, lv_color_hex(0x29292c), 0);
    lv_obj_t *l = lv_label_create(home_button);
    lv_label_set_text(l, "Home / Stop app");
    lv_obj_center(l);
    lv_obj_add_event_cb(home_button, go_home, LV_EVENT_CLICKED, NULL);
    cyd_settings_init(screen, width, height);
    cyd_connections_init(screen, width, height);
    if (cyd_projects_ui_init(screen, width, height))
        return ESP_ERR_NO_MEM;
    refresh(NULL);
    lv_timer_create(refresh, 2000, NULL);
    if (xTaskCreate(action_task, "shell_actions", 12288, NULL, 3, NULL) != pdPASS)
        return ESP_ERR_NO_MEM;
    return xTaskCreate(task, "ui", 8192, NULL, 5, NULL) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
