add_library(byte_render_core ${BYTE_RENDER_LIBRARY_TYPE}
    src/assert.cpp
    src/log.cpp
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

byte_render_configure_library(byte_render_core CORE)
