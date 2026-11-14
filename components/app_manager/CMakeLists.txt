idf_component_register(SRCS "manager.c" "manifest.c" "catalog.c" INCLUDE_DIRS "." "include" REQUIRES cyd_common storage app_runtime esp_timer esp_hw_support)
