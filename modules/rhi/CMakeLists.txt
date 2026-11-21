# byte::rhi — API-agnostic public headers plus exactly one backend, selected at compile time.

if(BYTE_RENDER_RHI_BACKEND STREQUAL "Vulkan")
    set(backend_sources
        src/vulkan/vulkan_backend.cpp
    )
endif()

add_library(byte_render_rhi ${BYTE_RENDER_LIBRARY_TYPE}
    ${backend_sources}
)
add_library(byte::rhi ALIAS byte_render_rhi)

target_include_directories(byte_render_rhi
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(byte_render_rhi PUBLIC byte::core)

byte_render_configure_library(byte_render_rhi RHI)
