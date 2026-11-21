# bytee::rhi — API-agnostic public headers plus exactly one backend, selected at compile time.

if(BYTEE_RHI_BACKEND STREQUAL "Vulkan")
    set(backend_sources
        src/vulkan/vulkan_backend.cpp
    )
endif()

add_library(bytee_rhi ${BYTEE_LIBRARY_TYPE}
    ${backend_sources}
)
add_library(bytee::rhi ALIAS bytee_rhi)

target_include_directories(bytee_rhi
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(bytee_rhi PUBLIC bytee::core)

bytee_configure_library(bytee_rhi RHI)
