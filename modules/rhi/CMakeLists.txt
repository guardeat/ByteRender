# bytee::rhi — API-agnostic public headers plus exactly one backend, selected at compile time.

if(BYTEE_RHI_BACKEND STREQUAL "Vulkan")
    bytee_fetch_vulkan()
    set(backend_sources
        src/vulkan/vulkan_backend.cpp
        src/vulkan/vulkan_common.cpp
        src/vulkan/vulkan_instance.cpp
        src/vulkan/vulkan_loader.cpp
        src/vulkan/vulkan_requirements.cpp
        src/vulkan/vulkan_volk.cpp
    )
    set(backend_libraries bytee_vulkan_dependencies)
    # volk's implementation is third-party code: compile it without Bytee's warning flags.
    if(MSVC)
        set_source_files_properties(src/vulkan/vulkan_volk.cpp PROPERTIES COMPILE_OPTIONS "/W0")
    else()
        set_source_files_properties(src/vulkan/vulkan_volk.cpp PROPERTIES COMPILE_OPTIONS "-w")
    endif()
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
