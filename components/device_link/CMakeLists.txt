idf_component_register(SRCS "link.c" "peer.c" INCLUDE_DIRS "include"
    REQUIRES cyd_common esp_driver_uart esp_driver_gpio esp_timer esp_hw_support)
