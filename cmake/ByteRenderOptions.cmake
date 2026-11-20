# User-facing build options for ByteRender.

set(BYTE_RENDERER_RHI_BACKEND "Vulkan" CACHE STRING "RHI backend compiled into byte::rhi")
set_property(CACHE BYTE_RENDERER_RHI_BACKEND PROPERTY STRINGS Vulkan)
set(_byte_renderer_backends Vulkan)
if(NOT BYTE_RENDERER_RHI_BACKEND IN_LIST _byte_renderer_backends)
    message(FATAL_ERROR "Unknown BYTE_RENDERER_RHI_BACKEND '${BYTE_RENDERER_RHI_BACKEND}'. "
                        "Valid values: ${_byte_renderer_backends}")
endif()

option(BYTE_RENDERER_BUILD_SHARED "Build ByteRender as shared libraries" OFF)

set(BYTE_RENDERER_ASSERTS "AUTO" CACHE STRING "BYTE_RENDERER_ASSERT: AUTO (Debug only), ON, OFF")
set_property(CACHE BYTE_RENDERER_ASSERTS PROPERTY STRINGS AUTO ON OFF)

option(BYTE_RENDERER_WARNINGS_AS_ERRORS "Treat warnings as errors in ByteRender targets" ON)

option(BYTE_RENDERER_BUILD_SAMPLES "Build ByteRender samples" ${PROJECT_IS_TOP_LEVEL})
option(BYTE_RENDERER_BUILD_TESTS "Build ByteRender tests" ${PROJECT_IS_TOP_LEVEL})

if(BYTE_RENDERER_BUILD_SHARED)
    set(BYTE_RENDERER_LIBRARY_TYPE SHARED)
else()
    set(BYTE_RENDERER_LIBRARY_TYPE STATIC)
endif()
