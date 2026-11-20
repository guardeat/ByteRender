add_library(byte_renderer_core ${BYTE_RENDERER_LIBRARY_TYPE}
    src/version.cpp
)
add_library(byte::core ALIAS byte_renderer_core)

target_include_directories(byte_renderer_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_definitions(byte_renderer_core PRIVATE
    BYTE_RENDERER_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    BYTE_RENDERER_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    BYTE_RENDERER_VERSION_PATCH=${PROJECT_VERSION_PATCH}
    BYTE_RENDERER_VERSION_STRING="${PROJECT_VERSION}"
)

byte_renderer_configure_library(byte_renderer_core CORE)
