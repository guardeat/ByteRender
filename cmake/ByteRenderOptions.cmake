# User-facing build options for ByteRender.

set(BYTE_RENDER_RHI_BACKEND "Vulkan" CACHE STRING "RHI backend compiled into byte::rhi")
set_property(CACHE BYTE_RENDER_RHI_BACKEND PROPERTY STRINGS Vulkan)
set(_byte_render_backends Vulkan)
if(NOT BYTE_RENDER_RHI_BACKEND IN_LIST _byte_render_backends)
    message(FATAL_ERROR "Unknown BYTE_RENDER_RHI_BACKEND '${BYTE_RENDER_RHI_BACKEND}'. "
                        "Valid values: ${_byte_render_backends}")
endif()

option(BYTE_RENDER_BUILD_SHARED "Build ByteRender as shared libraries" OFF)

set(BYTE_RENDER_ASSERTS "AUTO" CACHE STRING "BYTE_RENDER_ASSERT: AUTO (Debug only), ON, OFF")
set_property(CACHE BYTE_RENDER_ASSERTS PROPERTY STRINGS AUTO ON OFF)

option(BYTE_RENDER_WARNINGS_AS_ERRORS "Treat warnings as errors in ByteRender targets" ON)

option(BYTE_RENDER_BUILD_SAMPLES "Build ByteRender samples" ${PROJECT_IS_TOP_LEVEL})
option(BYTE_RENDER_BUILD_TESTS "Build ByteRender tests" ${PROJECT_IS_TOP_LEVEL})

if(BYTE_RENDER_BUILD_SHARED)
    set(BYTE_RENDER_LIBRARY_TYPE SHARED)
else()
    set(BYTE_RENDER_LIBRARY_TYPE STATIC)
endif()
