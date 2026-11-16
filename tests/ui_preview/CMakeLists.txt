cmake_minimum_required(VERSION 3.16)
project(cyd_connection_preview C CXX ASM)
set(LV_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h" CACHE STRING "")
set(LV_CONF_BUILD_DISABLE_EXAMPLES ON CACHE BOOL "")
set(LV_CONF_BUILD_DISABLE_DEMOS ON CACHE BOOL "")
set(LV_CONF_BUILD_DISABLE_THORVG_INTERNAL ON CACHE BOOL "")
add_subdirectory("../../managed_components/lvgl__lvgl" lvgl)
add_executable(connection_preview preview.c ../../components/network/policy.c)
target_include_directories(connection_preview PRIVATE ../network_sdk ../../components/network/include)
target_link_libraries(connection_preview PRIVATE lvgl m)
target_include_directories(connection_preview PRIVATE ../../components/device_access/include ../../components/ota_manager/include)
set(CJSON_ROOT "$ENV{IDF_PATH}/components/json/cJSON")
add_executable(watch_preview watch_preview.c watch_mocks.c ../system_runtime.c
    ../../components/ui/ui.c ../../components/ui/pager.c ../../components/ui/connections.c ../../components/ui/projects.c ../../components/ui/settings.c
    ../../components/device_settings/geometry.c "${CJSON_ROOT}/cJSON.c")
target_compile_definitions(watch_preview PRIVATE CYD_UI_PREVIEW)
target_include_directories(watch_preview PRIVATE ../../components/device_link/include ../system_sdk "${CJSON_ROOT}" ../../components/ui
    ../../components/network/include ../../components/device_access/include ../../components/ota_manager/include
    ../../components/device_settings/include ../../components/app_manager/include)
target_link_libraries(watch_preview PRIVATE lvgl m)
target_sources(watch_preview PRIVATE ../../components/network/policy.c)
