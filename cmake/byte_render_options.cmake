# User-facing build options. See docs/architecture.md §16.

function(byte_render_choice_option name default description)
    set(${name} "${default}" CACHE STRING "${description}")
    set_property(CACHE ${name} PROPERTY STRINGS ${ARGN})
    if(NOT "${${name}}" IN_LIST ARGN)
        message(FATAL_ERROR "${name}='${${name}}' is invalid. Valid values: ${ARGN}")
    endif()
endfunction()

byte_render_choice_option(BYTE_RENDER_RHI_BACKEND "Vulkan" "RHI backend compiled into byte::rhi" Vulkan)
byte_render_choice_option(BYTE_RENDER_ASSERTS "AUTO" "BYTE_RENDER_ASSERT: AUTO (Debug only), ON, OFF" AUTO ON OFF)
byte_render_choice_option(BYTE_RENDER_ERROR_LOCATIONS "AUTO" "Record where errors are created: AUTO (Debug only), ON, OFF"
    AUTO ON OFF)
byte_render_choice_option(BYTE_RENDER_LOG_LEVEL "AUTO" "Compile-time minimum log level (AUTO: Debug in Debug, else Info)"
    AUTO Trace Debug Info Warning Error Off)
byte_render_choice_option(BYTE_RENDER_SANITIZE "" "Sanitizer for top-level builds: address (ASan+UBSan) or thread (TSan)"
    "" address thread)

option(BYTE_RENDER_BUILD_SHARED "Build ByteRender as shared libraries" OFF)
option(BYTE_RENDER_WARNINGS_AS_ERRORS "Treat warnings as errors in ByteRender targets" ON)
option(BYTE_RENDER_LTO "Link-time optimization for Release builds" ${PROJECT_IS_TOP_LEVEL})
option(BYTE_RENDER_BUILD_SAMPLES "Build ByteRender samples" ${PROJECT_IS_TOP_LEVEL})
option(BYTE_RENDER_BUILD_TESTS "Build ByteRender tests" ${PROJECT_IS_TOP_LEVEL})
option(BYTE_RENDER_BUILD_BENCHMARKS "Build ByteRender micro-benchmarks" ${PROJECT_IS_TOP_LEVEL})

if(BYTE_RENDER_BUILD_SHARED)
    set(BYTE_RENDER_LIBRARY_TYPE SHARED)
else()
    set(BYTE_RENDER_LIBRARY_TYPE STATIC)
endif()
