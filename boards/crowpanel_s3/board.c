/* DIS01943A V1.1--V1.3 only. See docs/HARDWARE.md for pinned manufacturer sources. */
#include "cyd.h"
#include "cyd_device.h"
#include "control_boot.h"
#include "sdkconfig.h"
#include "esp_check.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "driver/sdspi_host.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_vfs_fat.h"
#include "esp_psram.h"
#include "esp_log.h"
static esp_lcd_panel_handle_t panel;
static esp_lcd_touch_handle_t touch;
static bool initialized;
static i2c_master_dev_handle_t stc;
static esp_err_t control_write(void *bus, uint8_t value) {
    (void)bus;
    return i2c_master_transmit(stc, &value, 1, 100);
}
static esp_err_t control_bus_reset(void *bus) {
    return i2c_master_bus_reset((i2c_master_bus_handle_t)bus);
}
static void control_wait(void *bus, unsigned ms) {
    (void)bus;
    vTaskDelay(pdMS_TO_TICKS(ms));
}
static void control_retry(void *bus, uint8_t value, unsigned attempt, esp_err_t err) {
    (void)bus;
    ESP_LOGW("board", "STC command %u attempt %u failed (%s); recovering I2C bus",
        (unsigned)value, attempt, esp_err_to_name(err));
}
unsigned cyd_board_width(void) {
    return 800;
}
unsigned cyd_board_height(void) {
    return 480;
}
const char *cyd_board_model(void) {
    return "CrowPanel Advance DIS01943A V1.1-V1.3";
}
esp_err_t cyd_board_init(void) {
#ifndef CONFIG_CYD_BOARD_CONFIRMED
    ESP_LOGE("board", "Confirm DIS01943A V1.1-V1.3 in menuconfig before enabling hardware.");
    return ESP_ERR_NOT_SUPPORTED;
#else
    ESP_LOGI("board", "CrowPanel PSRAM detected: %u bytes", (unsigned)esp_psram_get_size());
    if (!esp_psram_is_initialized() || esp_psram_get_size() < 4 * 1024 * 1024)
        return ESP_ERR_NO_MEM;
    ESP_LOGI("board", "Starting control/touch I2C on SDA15/SCL16");
    i2c_master_bus_handle_t bus;
    i2c_master_bus_config_t bc = {.i2c_port = I2C_NUM_0,
                                  .sda_io_num = 15,
                                  .scl_io_num = 16,
                                  .clk_source = I2C_CLK_SRC_DEFAULT,
                                  .glitch_ignore_cnt = 7,
                                  .flags.enable_internal_pullup = true};
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bc, &bus), "board", "I2C");
    i2c_device_config_t dc = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = 0x30, .scl_speed_hz = 100000};
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &dc, &stc), "board", "STC");
    ESP_LOGI("board", "Sending STC touch-enable and backlight commands");
    cyd_control_boot_io_t control = {.context = bus, .write = control_write,
        .reset_bus = control_bus_reset, .wait_ms = control_wait, .retry_note = control_retry};
    ESP_RETURN_ON_ERROR(cyd_control_boot(&control), "board", "STC startup after bus recovery");
    ESP_LOGI("board", "STC touch/backlight startup complete");
    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_io_i2c_config_t ic = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    ic.scl_speed_hz = 100000;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(bus, &ic, &io), "board", "touch IO");
    ESP_LOGI("board", "Initializing GT911 touch");
    esp_lcd_touch_config_t tc = {
        .x_max = 800, .y_max = 480, .rst_gpio_num = -1, .int_gpio_num = -1};
    ESP_RETURN_ON_ERROR(esp_lcd_touch_new_i2c_gt911(io, &tc, &touch), "board", "GT911");
    ESP_LOGI("board", "Initializing 800x480 RGB panel at 16 MHz");
    esp_lcd_rgb_panel_config_t pc = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .data_width = 16,
        .bits_per_pixel = 16,
        .num_fbs = 1,
        .bounce_buffer_size_px = 800 * 10,
        .psram_trans_align = 64,
        .hsync_gpio_num = 40,
        .vsync_gpio_num = 41,
        .de_gpio_num = 42,
        .pclk_gpio_num = 39,
        .disp_gpio_num = -1,
        .data_gpio_nums = {21, 47, 48, 45, 38, 9, 10, 11, 12, 13, 14, 7, 17, 18, 3, 46},
        .timings = {.pclk_hz = 16000000,
                    .h_res = 800,
                    .v_res = 480,
                    .hsync_pulse_width = 4,
                    .hsync_back_porch = 8,
                    .hsync_front_porch = 8,
                    .vsync_pulse_width = 4,
                    .vsync_back_porch = 8,
                    .vsync_front_porch = 8,
                    .flags.pclk_active_neg = true},
        .flags.fb_in_psram = true};
    ESP_RETURN_ON_ERROR(esp_lcd_new_rgb_panel(&pc, &panel), "board", "RGB");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(panel), "board", "reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(panel), "board", "init");
    ESP_LOGI("board", "RGB panel initialization complete");
    initialized = true;
    return ESP_OK;
#endif
}
void cyd_board_flush(lv_display_t *d, const lv_area_t *a, uint8_t *px) {
    esp_lcd_panel_draw_bitmap(panel, a->x1, a->y1, a->x2 + 1, a->y2 + 1, px);
    lv_display_flush_ready(d);
}
void cyd_board_touch(lv_indev_t *i, lv_indev_data_t *d) {
    (void)i;
    esp_lcd_touch_point_data_t point;
    uint8_t n = 0;
    bool pressed = esp_lcd_touch_read_data(touch) == ESP_OK &&
                   esp_lcd_touch_get_data(touch, &point, &n, 1) == ESP_OK && n;
    int x = 0, y = 0;
    bool consumed = cyd_device_touch(pressed ? point.x : 0, pressed ? point.y : 0, pressed, &x, &y);
    d->state = pressed && !consumed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    if (pressed && !consumed) {
        d->point.x = x;
        d->point.y = y;
    }
}
esp_err_t cyd_board_set_brightness(unsigned percent) {
    if (percent < 10 || percent > 100 || !stc)
        return ESP_ERR_INVALID_ARG;
    uint8_t value = (100 - percent) * 244 / 100;
    return i2c_master_transmit(stc, &value, 1, 100);
}
esp_err_t cyd_board_sd_mount(void) {
    if (!initialized)
        return ESP_ERR_INVALID_STATE;
    spi_bus_config_t b = {.mosi_io_num = 6,
                          .miso_io_num = 4,
                          .sclk_io_num = 5,
                          .quadwp_io_num = -1,
                          .quadhd_io_num = -1,
                          .max_transfer_sz = 8192};
    esp_err_t e = spi_bus_initialize(SPI2_HOST, &b, SPI_DMA_CH_AUTO);
    if (e)
        return e;
    sdmmc_host_t h = SDSPI_HOST_DEFAULT();
    h.slot = SPI2_HOST;
    h.max_freq_khz = 10000;
    sdspi_device_config_t dev = SDSPI_DEVICE_CONFIG_DEFAULT();
    dev.host_id = SPI2_HOST;
    dev.gpio_cs = SDSPI_SLOT_NO_CS; /* schematic: R33=10K to GND; no MCU CS */
    esp_vfs_fat_sdmmc_mount_config_t m = {
        .format_if_mount_failed = false, .max_files = 8, .allocation_unit_size = 16 * 1024};
    sdmmc_card_t *card;
    return esp_vfs_fat_sdspi_mount("/sdcard", &h, &dev, &m, &card);
}
