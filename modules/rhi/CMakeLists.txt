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
    FetchContent_Declare(vma
        URL https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/archive/refs/tags/v3.5.0.tar.gz
        URL_HASH SHA256=c5f3db848fca9cb9726b41f256cc3007773321fa92a86d1e0902e66474e8a9e3
        SOURCE_SUBDIR do-not-build
    )
    FetchContent_MakeAvailable(vulkan_headers volk vma)

    # Private to the backend: never propagated to consumers' include paths.
    add_library(byte_render_vulkan_deps INTERFACE)
    target_include_directories(byte_render_vulkan_deps SYSTEM INTERFACE
        "${vulkan_headers_SOURCE_DIR}/include"
        "${volk_SOURCE_DIR}"
        "${vma_SOURCE_DIR}/include"
    )
    target_compile_definitions(byte_render_vulkan_deps INTERFACE
        VK_NO_PROTOTYPES
        # VMA fetches Vulkan functions itself through vkGetInstanceProcAddr/vkGetDeviceProcAddr (volk).
        VMA_STATIC_VULKAN_FUNCTIONS=0
        VMA_DYNAMIC_VULKAN_FUNCTIONS=1
        VMA_VULKAN_VERSION=1003000
    )
    target_link_libraries(byte_render_vulkan_deps INTERFACE ${CMAKE_DL_LIBS})

    set(backend_sources
        src/vulkan/vulkan_backend.cpp
        src/vulkan/vulkan_common.cpp
        src/vulkan/vulkan_device.cpp
        src/vulkan/vulkan_device_requirements.cpp
        src/vulkan/vulkan_instance.cpp
        src/vulkan/vulkan_loader.cpp
        src/vulkan/vulkan_vma.cpp
        src/vulkan/vulkan_volk.cpp
    )
    set(backend_libraries byte_render_vulkan_deps)
    set(backend_definition BYTE_RENDER_RHI_BACKEND_VULKAN=1)

    # Third-party implementation files: don't apply our warning flags.
    if(MSVC)
        set(third_party_options "/W0")
    else()
        set(third_party_options "-w")
    endif()
    set_source_files_properties(src/vulkan/vulkan_volk.cpp src/vulkan/vulkan_vma.cpp
        PROPERTIES COMPILE_OPTIONS "${third_party_options}")
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
