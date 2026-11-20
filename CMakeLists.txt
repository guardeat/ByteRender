cmake_minimum_required(VERSION 3.24)

project(ByteRender
    VERSION 0.1.0
    DESCRIPTION "Generic cross-platform rendering engine"
    LANGUAGES CXX
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(ByteRenderOptions)
include(ByteRenderTargets)

if(PROJECT_IS_TOP_LEVEL)
    if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
    endif()
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    # Keep executables and shared libraries together so samples find DLLs on Windows.
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")
endif()

add_subdirectory(modules/core)
add_subdirectory(modules/rhi)
add_subdirectory(modules/render_graph)

if(BYTE_RENDERER_BUILD_SAMPLES)
    add_subdirectory(samples)
endif()

if(BYTE_RENDERER_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
    enable_testing()
    add_subdirectory(tests)
endif()
