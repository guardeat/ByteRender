add_library(bytee_core ${BYTEE_LIBRARY_TYPE}
    src/assert.cpp
    src/log.cpp
    src/version.cpp
)
add_library(bytee::core ALIAS bytee_core)

target_include_directories(bytee_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_definitions(bytee_core PRIVATE
    BYTEE_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    BYTEE_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    BYTEE_VERSION_PATCH=${PROJECT_VERSION_PATCH}
    BYTEE_VERSION_STRING="${PROJECT_VERSION}"
)

# Engine-wide configuration. PUBLIC: every target using ByteRender headers must agree on these.
set(debug_only $<CONFIG:Debug>)

if(BYTEE_ASSERTS STREQUAL "AUTO")
    target_compile_definitions(bytee_core PUBLIC $<${debug_only}:BYTEE_ENABLE_ASSERTS=1>)
elseif(BYTEE_ASSERTS STREQUAL "ON")
    target_compile_definitions(bytee_core PUBLIC BYTEE_ENABLE_ASSERTS=1)
endif()

if(BYTEE_ERROR_LOCATIONS STREQUAL "AUTO")
    target_compile_definitions(bytee_core PUBLIC $<${debug_only}:BYTEE_ENABLE_ERROR_LOCATIONS=1>)
elseif(BYTEE_ERROR_LOCATIONS STREQUAL "ON")
    target_compile_definitions(bytee_core PUBLIC BYTEE_ENABLE_ERROR_LOCATIONS=1)
endif()

# Matches core::LogLevel: Trace=0, Debug=1, Info=2, Warning=3, Error=4, Off=5.
set(log_levels Trace Debug Info Warning Error Off)
if(BYTEE_LOG_LEVEL STREQUAL "AUTO")
    target_compile_definitions(bytee_core PUBLIC BYTEE_LOG_LEVEL=$<IF:${debug_only},1,2>)
else()
    list(FIND log_levels "${BYTEE_LOG_LEVEL}" log_level_value)
    target_compile_definitions(bytee_core PUBLIC BYTEE_LOG_LEVEL=${log_level_value})
endif()

bytee_configure_library(bytee_core CORE)
