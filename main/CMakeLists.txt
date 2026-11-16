idf_component_register(SRCS "main.c" INCLUDE_DIRS "." REQUIRES paired_system builtin_apps device_link cyd_common board ui storage network app_manager app_runtime web_api nvs_flash device_access ota_manager device_settings)

if(CONFIG_CYD_PAIRED_SYSTEM)
    idf_component_get_property(pair_lib paired_system COMPONENT_LIB)
    target_link_libraries(${COMPONENT_LIB} PRIVATE ${pair_lib})
endif()
