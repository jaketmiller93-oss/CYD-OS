idf_component_register(SRCS "network.c" "policy.c" INCLUDE_DIRS "include" REQUIRES cyd_common esp_wifi esp_netif nvs_flash mdns esp_timer ota_manager)
