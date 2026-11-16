idf_component_register(SRCS "link.c" "peer.c" INCLUDE_DIRS "include"
    REQUIRES cyd_common esp_driver_uart esp_driver_gpio esp_timer esp_hw_support esp_system network storage)

if(CONFIG_CYD_PAIRED_SYSTEM)
    target_compile_definitions(${COMPONENT_LIB} PUBLIC CYD_LINK_ENABLE_RPC=1)
endif()
