set(pair_sources "disabled.c")
if(CONFIG_CYD_PAIRED_SYSTEM)
    set(pair_sources "rpc.c" "json.c" "services.c" "files.c" "bundle.c" "update.c" "network.c" "proxy_io.c" "host_network.c" "ble.c" "manager.c")
endif()
idf_component_register(SRCS ${pair_sources} INCLUDE_DIRS "include"
    REQUIRES cyd_common device_link device_settings ota_manager app_manager app_services esp_timer esp_hw_support esp_system mbedtls bt nvs_flash esp_psram)
