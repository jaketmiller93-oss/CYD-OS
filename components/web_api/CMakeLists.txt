idf_component_register(SRCS "web.c" "startup.c" INCLUDE_DIRS "." EMBED_TXTFILES "dashboard.html" REQUIRES paired_system builtin_apps device_link cyd_common esp_http_server app_manager app_runtime storage network esp_psram device_access ota_manager device_settings)

if(CONFIG_CYD_PAIRED_SYSTEM)
    idf_component_get_property(pair_lib paired_system COMPONENT_LIB)
    target_link_libraries(${COMPONENT_LIB} PRIVATE ${pair_lib})
endif()
