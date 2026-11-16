# IDF_TARGET is provided in both IDF's isolated dependency scan and normal CMake.
# Custom cache variables are not available in the isolated scan.
if(IDF_TARGET STREQUAL "esp32s3")
    set(profile crowpanel_s3)
    set(board_deps esp_driver_i2c esp_lcd_touch_gt911 sdmmc)
    set(board_extra "control_boot.c" "sd_crc_boot.c")
elseif(IDF_TARGET STREQUAL "esp32c5")
    set(profile nm_cyd_c5)
    set(board_deps esp_driver_gpio esp_driver_ledc)
else()
    message(FATAL_ERROR "CYD OS supports only esp32s3 / crowpanel_s3 and esp32c5 / nm_cyd_c5")
endif()
idf_component_register(SRCS "../../boards/${profile}/board.c" ${board_extra} INCLUDE_DIRS "."
    REQUIRES cyd_common device_settings esp_lcd esp_driver_spi esp_driver_sdspi fatfs esp_psram ${board_deps})
if(IDF_TARGET STREQUAL "esp32s3")
    target_link_options(${COMPONENT_LIB} INTERFACE
        "-Wl,--wrap=sdmmc_send_cmd_crc_on_off"
        "-Wl,--undefined=__wrap_sdmmc_send_cmd_crc_on_off")
endif()
