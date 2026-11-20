# byte::rhi — API-agnostic public headers + exactly one backend compiled in.

if(BYTE_RENDER_RHI_BACKEND STREQUAL "Vulkan")
    include(FetchContent)

    # Headers only: we don't run these projects' CMake, just use their sources.
    FetchContent_Declare(vulkan_headers
        URL https://github.com/KhronosGroup/Vulkan-Headers/archive/refs/tags/v1.4.363.tar.gz
        URL_HASH SHA256=cbaf687d3c59b9666fe080e7f8396b6b4f4d344a768ee6b337c59852f4526b68
        SOURCE_SUBDIR do-not-build
    )
    FetchContent_Declare(volk
        URL https://github.com/zeux/volk/archive/refs/tags/vulkan-sdk-1.4.363.0.tar.gz
        URL_HASH SHA256=1547d8d74395d4048fb3f4a6313da56db626e89b55db238d0d7e8944c8a645f3
        SOURCE_SUBDIR do-not-build
    )
    FetchContent_MakeAvailable(vulkan_headers volk)

    # Private to the backend: never propagated to consumers' include paths.
    add_library(byte_render_vulkan_deps INTERFACE)
    target_include_directories(byte_render_vulkan_deps SYSTEM INTERFACE
        "${vulkan_headers_SOURCE_DIR}/include"
        "${volk_SOURCE_DIR}"
    )
    target_compile_definitions(byte_render_vulkan_deps INTERFACE VK_NO_PROTOTYPES)
    target_link_libraries(byte_render_vulkan_deps INTERFACE ${CMAKE_DL_LIBS})

    set(backend_sources
        src/vulkan/vulkan_backend.cpp
        src/vulkan/vulkan_common.cpp
        src/vulkan/vulkan_instance.cpp
        src/vulkan/vulkan_loader.cpp
        src/vulkan/vulkan_volk.cpp
    )
    set(backend_libraries byte_render_vulkan_deps)
    set(backend_definition BYTE_RENDER_RHI_BACKEND_VULKAN=1)

    # Third-party implementation file: don't apply our warning flags.
    if(MSVC)
        set_source_files_properties(src/vulkan/vulkan_volk.cpp PROPERTIES COMPILE_OPTIONS "/W0")
    else()
        set_source_files_properties(src/vulkan/vulkan_volk.cpp PROPERTIES COMPILE_OPTIONS "-w")
    endif()
endif()

add_library(byte_render_rhi ${BYTE_RENDER_LIBRARY_TYPE}
    ${backend_sources}
)
add_library(byte::rhi ALIAS byte_render_rhi)

target_include_directories(byte_render_rhi
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Backend identity is private: public headers must not depend on which backend is compiled.
target_compile_definitions(byte_render_rhi PRIVATE ${backend_definition})

target_link_libraries(byte_render_rhi
    PUBLIC  byte::core
    PRIVATE ${backend_libraries}
)

byte_render_configure_library(byte_render_rhi RHI)
