idf_component_register(SRCS "services.c" "esp_io.c" "credentials.c" INCLUDE_DIRS "include" REQUIRES paired_system builtin_apps cyd_common esp-tls lwip mbedtls nvs_flash)

if(NOT CONFIG_MBEDTLS_CERTIFICATE_BUNDLE OR CONFIG_ESP_TLS_INSECURE OR CONFIG_ESP_TLS_SKIP_SERVER_CERT_VERIFY)
    message(FATAL_ERROR "App services require the certificate bundle and verified TLS; apply ota_config flags and disable insecure TLS options")
endif()

if(CONFIG_CYD_PAIRED_SYSTEM)
    idf_component_get_property(pair_lib paired_system COMPONENT_LIB)
    target_link_libraries(${COMPONENT_LIB} PRIVATE ${pair_lib})
endif()
