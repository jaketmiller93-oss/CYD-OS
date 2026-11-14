/* RockBase NM-CYD-C5 V1.0 ST7789 + XPT2046. Sources: docs/HARDWARE-C5.md. */
#include "cyd.h"
#include "cyd_device.h"
#include "driver/ledc.h"
#include "sdkconfig.h"
#include "esp_check.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/sdspi_host.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_vfs_fat.h"
#include "esp_psram.h"
static esp_lcd_panel_handle_t panel;
static spi_device_handle_t touch;
static lv_display_t *display;
static bool bus_ready, sd_attempted;
static esp_err_t sd_result = ESP_ERR_INVALID_STATE;
unsigned cyd_board_width(void) {
    return 320;
}
unsigned cyd_board_height(void) {
    return 240;
}
const char *cyd_board_model(void) {
    return "RockBase NM-CYD-C5 V1.0 / ST7789";
}
static bool color_done(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *e, void *ctx) {
    (void)io;
    (void)e;
    (void)ctx;
    if (display)
        lv_display_flush_ready(display);
    return false;
}
esp_err_t cyd_board_sd_mount(void) {
    if (!bus_ready)
        return ESP_ERR_INVALID_STATE;
    if (sd_attempted)
        return sd_result;
    sd_attempted = true;
    sdmmc_host_t h = SDSPI_HOST_DEFAULT();
    h.slot = SPI2_HOST;
    h.max_freq_khz = 10000;
    sdspi_device_config_t dev = SDSPI_DEVICE_CONFIG_DEFAULT();
    dev.host_id = SPI2_HOST;
    dev.gpio_cs = 10;
    esp_vfs_fat_sdmmc_mount_config_t m = {
        .format_if_mount_failed = false, .max_files = 8, .allocation_unit_size = 16 * 1024};
    sdmmc_card_t *card;
    sd_result = esp_vfs_fat_sdspi_mount("/sdcard", &h, &dev, &m, &card);
    return sd_result;
}
esp_err_t cyd_board_init(void) {
#ifndef CONFIG_CYD_BOARD_CONFIRMED
    ESP_LOGE("board", "Confirm your NM-CYD-C5 V1.0 ST7789 hardware in menuconfig first");
    return ESP_ERR_NOT_SUPPORTED;
#else
    if (!esp_psram_is_initialized() || esp_psram_get_size() < 4 * 1024 * 1024)
        return ESP_ERR_NO_MEM;
    /* Deselect all three devices before touching the shared bus. */
    const int selects[] = {23, 1, 10};
    for (unsigned i = 0; i < 3; i++) {
        gpio_set_level(selects[i], 1);
        gpio_set_direction(selects[i], GPIO_MODE_OUTPUT);
    }
    gpio_set_level(25, 0);
    gpio_set_direction(25, GPIO_MODE_OUTPUT);
    spi_bus_config_t b = {.mosi_io_num = 7,
                          .miso_io_num = 2,
                          .sclk_io_num = 6,
                          .quadwp_io_num = -1,
                          .quadhd_io_num = -1,
                          .max_transfer_sz = 320 * 24 * 2};
    ESP_RETURN_ON_ERROR(spi_bus_initialize(SPI2_HOST, &b, SPI_DMA_CH_AUTO), "board", "shared SPI");
    bus_ready = true;
    /* Enter SD SPI mode before any LCD/touch traffic (IDF shared-bus requirement). */
    esp_err_t sd = cyd_board_sd_mount();
    if (sd)
        ESP_LOGW("board", "SD unavailable (%s); continuing to shell", esp_err_to_name(sd));
    spi_device_interface_config_t tp = {
        .clock_speed_hz = 2000000, .mode = 0, .spics_io_num = 1, .queue_size = 1};
    ESP_RETURN_ON_ERROR(spi_bus_add_device(SPI2_HOST, &tp, &touch), "board", "XPT2046");
    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_io_spi_config_t ic = {.dc_gpio_num = 24,
                                        .cs_gpio_num = 23,
                                        .pclk_hz = 20000000,
                                        .lcd_cmd_bits = 8,
                                        .lcd_param_bits = 8,
                                        .spi_mode = 0,
                                        .trans_queue_depth = 1,
                                        .on_color_trans_done = color_done};
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &ic, &io),
                        "board", "LCD IO");
    esp_lcd_panel_dev_config_t pc = {
        .reset_gpio_num = -1, .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB, .bits_per_pixel = 16};
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_st7789(io, &pc, &panel), "board", "ST7789");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(panel), "board", "reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(panel), "board", "LCD init");
    /* Vendor panel tuning from the pinned official MicroPython demo. */
    static const struct {
        uint8_t cmd, len, data[14];
    } init[] = {
        {0x13, 0, {0}},
        {0xb6, 2, {0x0a, 0x82}},
        {0xb2, 5, {0x0c, 0x0c, 0x00, 0x33, 0x33}},
        {0xb7, 1, {0x35}},
        {0xbb, 1, {0x28}},
        {0xc0, 1, {0x0c}},
        {0xc2, 2, {0x01, 0xff}},
        {0xc3, 1, {0x10}},
        {0xc4, 1, {0x20}},
        {0xc6, 1, {0x0f}},
        {0xd0, 2, {0xa4, 0xa1}},
        {0xe0,
         14,
         {0xd0, 0x00, 0x02, 0x07, 0x0a, 0x28, 0x32, 0x44, 0x42, 0x06, 0x0e, 0x12, 0x14, 0x17}},
        {0xe1,
         14,
         {0xd0, 0x00, 0x02, 0x07, 0x0a, 0x28, 0x31, 0x54, 0x47, 0x0e, 0x1c, 0x17, 0x1b, 0x1e}}};
    for (unsigned i = 0; i < sizeof(init) / sizeof(*init); i++)
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, init[i].cmd, init[i].data, init[i].len),
                            "board", "panel tuning");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_swap_xy(panel, true), "board", "landscape");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_mirror(panel, true, false), "board", "rotation 3");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_invert_color(panel, false), "board", "inversion");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(panel, true), "board", "display on");
    gpio_set_level(25, 1);
    return ESP_OK;
#endif
}
void cyd_board_flush(lv_display_t *d, const lv_area_t *a, uint8_t *px) {
    display = d;
    lv_draw_sw_rgb565_swap(px, (a->x2 - a->x1 + 1) * (a->y2 - a->y1 + 1));
    esp_err_t e = esp_lcd_panel_draw_bitmap(panel, a->x1, a->y1, a->x2 + 1, a->y2 + 1, px);
    if (e) {
        ESP_LOGE("board", "LCD transfer failed: %s", esp_err_to_name(e));
        lv_display_flush_ready(d);
    }
}
static int sample(uint8_t cmd) {
    spi_transaction_t t = {
        .flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA, .length = 24, .tx_data = {cmd, 0, 0}};
    if (spi_device_polling_transmit(touch, &t))
        return -1;
    return ((t.rx_data[1] << 8) | t.rx_data[2]) >> 3;
}
static int median(int *a) {
    for (int i = 1; i < 5; i++)
        for (int j = i; j > 0 && a[j] < a[j - 1]; j--) {
            int t = a[j];
            a[j] = a[j - 1];
            a[j - 1] = t;
        }
    return a[2];
}
void cyd_board_touch(lv_indev_t *i, lv_indev_data_t *d) {
    (void)i;
    int rx = 0, ry = 0, x = 0, y = 0;
    int z1 = sample(0xb0), z2 = sample(0xc0);
    bool pressed = z1 >= 0 && z2 >= 0 && z1 + 4095 - z2 >= 600;
    if (pressed) {
        int xs[5], ys[5];
        for (int k = 0; k < 5; k++) {
            xs[k] = sample(0xd0);
            ys[k] = sample(0x90);
            if (xs[k] < 0 || ys[k] < 0)
                pressed = false;
        }
        if (pressed) {
            rx = median(xs);
            ry = median(ys);
        }
    }
    bool consumed = cyd_device_touch(rx, ry, pressed, &x, &y);
    d->state = pressed && !consumed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    if (pressed && !consumed) {
        d->point.x = x;
        d->point.y = y;
    }
}
esp_err_t cyd_board_set_brightness(unsigned percent) {
    static bool ready;
    if (percent < 10 || percent > 100)
        return ESP_ERR_INVALID_ARG;
    if (!ready) {
        ledc_timer_config_t timer = {.speed_mode = LEDC_LOW_SPEED_MODE,
                                     .duty_resolution = LEDC_TIMER_10_BIT,
                                     .timer_num = LEDC_TIMER_0,
                                     .freq_hz = 5000,
                                     .clk_cfg = LEDC_AUTO_CLK};
        esp_err_t e = ledc_timer_config(&timer);
        if (e)
            return e;
        ledc_channel_config_t channel = {.gpio_num = 25,
                                         .speed_mode = LEDC_LOW_SPEED_MODE,
                                         .channel = LEDC_CHANNEL_0,
                                         .timer_sel = LEDC_TIMER_0,
                                         .duty = 1023};
        e = ledc_channel_config(&channel);
        if (e)
            return e;
        ready = true;
    }
    esp_err_t e = ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, percent * 1023 / 100);
    return e ? e : ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}
