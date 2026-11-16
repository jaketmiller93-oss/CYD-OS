idf_component_register(SRCS "runtime.c" INCLUDE_DIRS "." REQUIRES paired_system builtin_apps cyd_common lua ui storage esp_timer app_services network device_settings)

if(CONFIG_CYD_PAIRED_SYSTEM)
    idf_component_get_property(builtin_lib builtin_apps COMPONENT_LIB)
    target_link_libraries(${COMPONENT_LIB} PRIVATE ${builtin_lib})
endif()
