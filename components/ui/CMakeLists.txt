idf_component_register(SRCS "pager.c" "ui.c" "connections.c" "projects.c" "settings.c" INCLUDE_DIRS "." REQUIRES paired_system builtin_apps device_link cyd_common board app_runtime esp_timer esp_psram storage network device_access ota_manager app_manager device_settings esp_hw_support spi_flash esp_system)

if(CONFIG_CYD_PAIRED_SYSTEM)
    target_sources(${COMPONENT_LIB} PRIVATE "pair_ui.c")
    idf_component_get_property(pair_lib paired_system COMPONENT_LIB)
    idf_component_get_property(builtin_lib builtin_apps COMPONENT_LIB)
    target_link_libraries(${COMPONENT_LIB} PRIVATE ${pair_lib} ${builtin_lib})
endif()
