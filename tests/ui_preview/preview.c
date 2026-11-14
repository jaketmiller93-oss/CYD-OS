/* Render actual production LVGL views using native network/NVS test doubles. */
#include "../network_harness.c"
#include "../../components/ui/connections.c"
void cyd_access_get_token(char *out,size_t n){snprintf(out,n,"demo-deployment-token");}
esp_err_t cyd_access_rotate(void){return ESP_OK;}
void cyd_ota_snapshot(cyd_ota_status_t *s){memset(s,0,sizeof(*s));s->boot_confirmed=true;s->stage=CYD_OS_IDLE;}
esp_err_t cyd_ota_reboot(void){return ESP_OK;}
esp_err_t cyd_ota_cancel(void){return ESP_OK;}
esp_err_t cyd_ota_rollback(void){return ESP_OK;}

static uint16_t frame[800 * 480], draw[800 * 24];
static unsigned screen_width, screen_height;
static void flush(lv_display_t *d, const lv_area_t *a, uint8_t *pixels) {
    uint16_t *src = (uint16_t *)pixels;
    for (int y = a->y1; y <= a->y2; y++)
        for (int x = a->x1; x <= a->x2; x++)
            frame[y * screen_width + x] = *src++;
    lv_display_flush_ready(d);
}
static void save(const char *name) {
    for (unsigned i = 0; i < 20; i++) {
        lv_tick_inc(20);
        lv_timer_handler();
    }
    char path[120];
    snprintf(path, sizeof(path), "%s-%u.ppm", name, screen_width);
    FILE *f = fopen(path, "wb");
    assert(f);
    fprintf(f, "P6\n%u %u\n255\n", screen_width, screen_height);
    for (unsigned i = 0; i < screen_width * screen_height; i++) {
        uint16_t p = frame[i];
        uint8_t rgb[] = {((p >> 11) & 31) * 255 / 31, ((p >> 5) & 63) * 255 / 63,
                         (p & 31) * 255 / 31};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
    lv_area_t area;
    lv_obj_get_coords(view, &area);
    assert(area.x1 == 0 && area.y1 == 0 && area.x2 < (int)screen_width &&
           area.y2 < (int)screen_height - 50);
}
int main(int argc, char **argv) {
    screen_width = argc > 1 ? (unsigned)atoi(argv[1]) : 320;
    screen_height = screen_width == 320 ? 240 : 480;
    assert(host_boot(false) == ESP_OK);
    host_tick(0);
    host_ip("Home");
    lv_init();
    lv_display_t *display = lv_display_create(screen_width, screen_height);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, draw, NULL, screen_width * 24 * 2,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);
    lv_display_set_theme(display, lv_theme_default_init(display, lv_palette_main(LV_PALETTE_BLUE),
                                                        lv_palette_main(LV_PALETTE_CYAN), true,
                                                        LV_FONT_DEFAULT));
    lv_obj_t *root = lv_screen_active();
    lv_obj_set_style_bg_color(root, lv_color_hex(0x101522), 0);
    lv_obj_t *home = lv_button_create(root);
    lv_obj_set_pos(home, (screen_width - 200) / 2, screen_height - 50);
    lv_obj_set_size(home, 200, 48);
    lv_obj_t *label = lv_label_create(home);
    lv_label_set_text(label, "Home / Stop app");
    lv_obj_center(label);
    cyd_connections_init(root, screen_width, screen_height);
    cyd_connections_open();
    save("connections");
    cyd_network_scan();
    host_pump();
    waiting_scan = false;
    render(PAGE_SCAN);
    save("networks");
    snprintf(join_ssid, 33, "Phone hotspot");
    secured = true;
    render(PAGE_JOIN);
    save("join");
    field = EDIT_PASSWORD;
    render(PAGE_EDIT);
    save("keyboard");
    assert(editor);
    lv_area_t input;
    lv_obj_get_coords(editor, &input);
    assert(input.y2 < (int)screen_height - 56);
    lv_obj_t *keys = lv_obj_get_child(view, -1);
    lv_area_t key_area;
    assert(lv_obj_check_type(keys, &lv_keyboard_class));
    lv_obj_get_coords(keys, &key_area);
    assert(key_area.x1 >= 0 && key_area.x2 < (int)screen_width);
    fprintf(stderr,"Keyboard: %d,%d..%d,%d; input bottom %d; reserved Home begins %u\n",key_area.x1,key_area.y1,key_area.x2,key_area.y2,input.y2,screen_height-56);
    assert(key_area.y1 > input.y2 && key_area.y2 < (int)screen_height - 56);
    lv_textarea_set_text(editor, "short");
    lv_obj_send_event(keys, LV_EVENT_READY, NULL);
    assert(page == PAGE_EDIT);
    lv_textarea_set_text(editor, "valid-password");
    lv_obj_send_event(keys, LV_EVENT_READY, NULL);
    assert(page == PAGE_JOIN);
    assert(!strcmp(join_password, "valid-password"));
    field = EDIT_PASSWORD;
    render(PAGE_EDIT);
    keys = lv_obj_get_child(view, -1);
    lv_textarea_set_text(editor, "cancelled-password");
    lv_obj_send_event(keys, LV_EVENT_CANCEL, NULL);
    assert(page == PAGE_JOIN);
    assert(!strcmp(join_password, "valid-password"));

    cyd_network_ap_credentials(ap_ssid, sizeof(ap_ssid), ap_password, sizeof(ap_password));
    show_ap_password = true;
    render(PAGE_AP);
    save("access-point");
    cyd_connections_close();
    assert(lv_obj_has_flag(view, LV_OBJ_FLAG_HIDDEN));
    puts("LVGL views rendered; keyboard and view stay above Home.");
    return 0;
}
