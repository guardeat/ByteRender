cmake_minimum_required(VERSION 3.25)

# std::format needs macOS 13.3+ (libc++ availability). Must be set before project().
if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.3" CACHE STRING "Minimum macOS version")
endif()

project(Bytee
    VERSION 0.1.0
    DESCRIPTION "Generic, high-performance, cross-platform rendering engine"
    LANGUAGES CXX
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(bytee_options)
include(bytee_compiler)
include(bytee_dependencies)

if(PROJECT_IS_TOP_LEVEL)
    if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
    endif()
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    # Executables and shared libraries side by side, so samples/tests find DLLs on Windows.
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")
    bytee_enable_sanitizers()
    bytee_add_format_targets()
endif()

add_subdirectory(modules/core)
add_subdirectory(modules/rhi)
add_subdirectory(modules/render_graph)

if(BYTEE_BUILD_SAMPLES)
    add_subdirectory(samples)
endif()

if(BYTEE_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

if(BYTEE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
