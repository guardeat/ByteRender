# User-facing build options. See docs/architecture.md §16.

function(bytee_choice_option name default description)
    set(${name} "${default}" CACHE STRING "${description}")
    set_property(CACHE ${name} PROPERTY STRINGS ${ARGN})
    if(NOT "${${name}}" IN_LIST ARGN)
        message(FATAL_ERROR "${name}='${${name}}' is invalid. Valid values: ${ARGN}")
    endif()
endfunction()

bytee_choice_option(BYTEE_RHI_BACKEND "Vulkan" "RHI backend compiled into bytee::rhi" Vulkan)
bytee_choice_option(BYTEE_ASSERTS "AUTO" "BYTEE_ASSERT: AUTO (Debug only), ON, OFF" AUTO ON OFF)
bytee_choice_option(BYTEE_ERROR_LOCATIONS "AUTO" "Record where errors are created: AUTO (Debug only), ON, OFF"
    AUTO ON OFF)
bytee_choice_option(BYTEE_LOG_LEVEL "AUTO" "Compile-time minimum log level (AUTO: Debug in Debug, else Info)"
    AUTO Trace Debug Info Warning Error Off)
bytee_choice_option(BYTEE_SANITIZE "" "Sanitizer for top-level builds: address (ASan+UBSan) or thread (TSan)"
    "" address thread)

option(BYTEE_BUILD_SHARED "Build Bytee as shared libraries" OFF)
option(BYTEE_WARNINGS_AS_ERRORS "Treat warnings as errors in Bytee targets" ON)
option(BYTEE_LTO "Link-time optimization for Release builds" ${PROJECT_IS_TOP_LEVEL})
option(BYTEE_BUILD_SAMPLES "Build Bytee samples" ${PROJECT_IS_TOP_LEVEL})
option(BYTEE_BUILD_TESTS "Build Bytee tests" ${PROJECT_IS_TOP_LEVEL})
option(BYTEE_BUILD_BENCHMARKS "Build Bytee micro-benchmarks" ${PROJECT_IS_TOP_LEVEL})

if(BYTEE_BUILD_SHARED)
    set(BYTEE_LIBRARY_TYPE SHARED)
else()
    set(BYTEE_LIBRARY_TYPE STATIC)
endif()
