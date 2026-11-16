set(role host)
if(CONFIG_IDF_TARGET_ESP32C5)
    set(role manager)
endif()
set(gen "${CMAKE_CURRENT_BINARY_DIR}/builtin_data.inc")
set(src "${CMAKE_CURRENT_LIST_DIR}/../../apps/essentials")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${src}/common.lua" "${src}/calculator.lua" "${src}/calendar.lua"
    "${src}/clock.lua" "${src}/notes.lua" "${src}/contacts.lua" "${src}/files.lua")
execute_process(COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/../../scripts/embed_essentials.py"
    --source "${src}" --output "${gen}" --role "${role}" COMMAND_ERROR_IS_FATAL ANY)
idf_component_register(SRCS "builtin.c" INCLUDE_DIRS "include" PRIV_INCLUDE_DIRS "${CMAKE_CURRENT_BINARY_DIR}"
    REQUIRES cyd_common)
