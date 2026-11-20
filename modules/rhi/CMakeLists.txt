# byte::rhi — API-agnostic public headers + exactly one backend compiled in.

if(BYTE_RENDERER_RHI_BACKEND STREQUAL "Vulkan")
    set(backend_sources
        src/vulkan/VulkanBackend.cpp
    )
    set(backend_definition BYTE_RENDERER_RHI_BACKEND_VULKAN=1)
endif()

add_library(byte_renderer_rhi ${BYTE_RENDERER_LIBRARY_TYPE}
    ${backend_sources}
)
add_library(byte::rhi ALIAS byte_renderer_rhi)

target_include_directories(byte_renderer_rhi
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Backend identity is private: public headers must not depend on which backend is compiled.
target_compile_definitions(byte_renderer_rhi PRIVATE ${backend_definition})

target_link_libraries(byte_renderer_rhi PUBLIC byte::core)

byte_renderer_configure_library(byte_renderer_rhi RHI)
