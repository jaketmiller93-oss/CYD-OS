idf_component_register(SRCS "device.c" "geometry.c" INCLUDE_DIRS "include" REQUIRES cyd_common nvs_flash esp_timer esp_netif app_runtime)
