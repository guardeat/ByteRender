add_library(byte_render_core ${BYTE_RENDER_LIBRARY_TYPE}
    src/version.cpp
)
add_library(byte::core ALIAS byte_render_core)

target_include_directories(byte_render_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_definitions(byte_render_core PRIVATE
    BYTE_RENDER_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    BYTE_RENDER_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    BYTE_RENDER_VERSION_PATCH=${PROJECT_VERSION_PATCH}
    BYTE_RENDER_VERSION_STRING="${PROJECT_VERSION}"
)

# Engine-wide configuration. PUBLIC: every target using ByteRender headers must agree on these.
set(debug_only $<CONFIG:Debug>)

if(BYTE_RENDER_ASSERTS STREQUAL "AUTO")
    target_compile_definitions(byte_render_core PUBLIC $<${debug_only}:BYTE_RENDER_ENABLE_ASSERTS=1>)
elseif(BYTE_RENDER_ASSERTS STREQUAL "ON")
    target_compile_definitions(byte_render_core PUBLIC BYTE_RENDER_ENABLE_ASSERTS=1)
endif()

if(BYTE_RENDER_ERROR_LOCATIONS STREQUAL "AUTO")
    target_compile_definitions(byte_render_core PUBLIC $<${debug_only}:BYTE_RENDER_ENABLE_ERROR_LOCATIONS=1>)
elseif(BYTE_RENDER_ERROR_LOCATIONS STREQUAL "ON")
    target_compile_definitions(byte_render_core PUBLIC BYTE_RENDER_ENABLE_ERROR_LOCATIONS=1)
endif()

# Matches core::LogLevel: Trace=0, Debug=1, Info=2, Warning=3, Error=4, Off=5.
set(log_levels Trace Debug Info Warning Error Off)
if(BYTE_RENDER_LOG_LEVEL STREQUAL "AUTO")
    target_compile_definitions(byte_render_core PUBLIC BYTE_RENDER_LOG_LEVEL=$<IF:${debug_only},1,2>)
else()
    list(FIND log_levels "${BYTE_RENDER_LOG_LEVEL}" log_level_value)
    target_compile_definitions(byte_render_core PUBLIC BYTE_RENDER_LOG_LEVEL=${log_level_value})
endif()

byte_render_configure_library(byte_render_core CORE)
