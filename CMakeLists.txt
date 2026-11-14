cmake_minimum_required(VERSION 3.16)
set(CYD_BOARD "crowpanel_s3" CACHE STRING "Board profile: crowpanel_s3 or nm_cyd_c5")
if(NOT CYD_BOARD STREQUAL "crowpanel_s3" AND NOT CYD_BOARD STREQUAL "nm_cyd_c5")
    message(FATAL_ERROR "Unknown CYD_BOARD")
endif()
# Register only the selected HAL with a stable component name.
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/components/board")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(cyd-os VERSION 0.3.2)
if((CYD_BOARD STREQUAL "crowpanel_s3" AND NOT IDF_TARGET STREQUAL "esp32s3") OR
   (CYD_BOARD STREQUAL "nm_cyd_c5" AND NOT IDF_TARGET STREQUAL "esp32c5"))
    message(FATAL_ERROR "CYD_BOARD and IDF_TARGET disagree; use separate build directories and sdkconfig files")
endif()
