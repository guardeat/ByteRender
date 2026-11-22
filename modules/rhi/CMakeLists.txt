# bytee::rhi — API-agnostic public headers plus exactly one backend, selected at compile time.

if(BYTEE_RHI_BACKEND STREQUAL "Vulkan")
    bytee_fetch_vulkan()
    set(backend_sources
        src/vulkan/vulkan_backend.cpp
        src/vulkan/vulkan_commands.cpp
        src/vulkan/vulkan_common.cpp
        src/vulkan/vulkan_convert.cpp
        src/vulkan/vulkan_device.cpp
        src/vulkan/vulkan_instance.cpp
        src/vulkan/vulkan_loader.cpp
        src/vulkan/vulkan_pipelines.cpp
        src/vulkan/vulkan_queue.cpp
        src/vulkan/vulkan_requirements.cpp
        src/vulkan/vulkan_resources.cpp
        src/vulkan/vulkan_samplers.cpp
        src/vulkan/vulkan_swapchain.cpp
        src/vulkan/vulkan_vma.cpp
        src/vulkan/vulkan_volk.cpp
    )
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        list(APPEND backend_sources src/vulkan/vulkan_surface_linux.cpp)
    endif()
    set(backend_libraries bytee_vulkan_dependencies)
    # volk and VMA implementations are third-party code: compile them without Bytee's warning flags.
    if(MSVC)
        set(third_party_options "/W0")
    else()
        set(third_party_options "-w")
    endif()
    set_source_files_properties(src/vulkan/vulkan_volk.cpp src/vulkan/vulkan_vma.cpp
        PROPERTIES COMPILE_OPTIONS "${third_party_options}")
endif()

add_library(bytee_rhi ${BYTEE_LIBRARY_TYPE}
    ${backend_sources}
)
add_library(bytee::rhi ALIAS bytee_rhi)

target_include_directories(bytee_rhi
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(bytee_rhi
    PUBLIC  bytee::core
    PRIVATE ${backend_libraries}
)

bytee_configure_library(bytee_rhi RHI)
