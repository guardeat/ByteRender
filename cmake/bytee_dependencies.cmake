# Third-party dependencies, pinned by version and SHA-256.
# Header-only use: SOURCE_SUBDIR points at a non-existent directory so the projects' own CMake files never run;
# we define minimal INTERFACE targets ourselves (with SYSTEM includes, so their headers never trigger our warnings).

include(FetchContent)

function(bytee_fetch_doctest)
    if(TARGET bytee_doctest)
        return()
    endif()
    FetchContent_Declare(doctest
        URL https://github.com/doctest/doctest/archive/refs/tags/v2.4.12.tar.gz
        URL_HASH SHA256=73381c7aa4dee704bd935609668cf41880ea7f19fa0504a200e13b74999c2d70
        SOURCE_SUBDIR do-not-build
    )
    FetchContent_MakeAvailable(doctest)
    add_library(bytee_doctest INTERFACE)
    target_include_directories(bytee_doctest SYSTEM INTERFACE "${doctest_SOURCE_DIR}")
endfunction()

# Vulkan backend: Vulkan-Headers + volk (headers only; volk's implementation is compiled by the backend itself).
# Private to byte::rhi: never propagated to consumers' include paths.
function(bytee_fetch_vulkan)
    if(TARGET bytee_vulkan_dependencies)
        return()
    endif()
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

    add_library(bytee_vulkan_dependencies INTERFACE)
    target_include_directories(bytee_vulkan_dependencies SYSTEM INTERFACE
        "${vulkan_headers_SOURCE_DIR}/include"
        "${volk_SOURCE_DIR}"
    )
    # volk provides every Vulkan function as a pointer; the loader is opened at runtime (no link dependency).
    target_compile_definitions(bytee_vulkan_dependencies INTERFACE VK_NO_PROTOTYPES)
    target_link_libraries(bytee_vulkan_dependencies INTERFACE ${CMAKE_DL_LIBS})
    set(bytee_volk_source_dir "${volk_SOURCE_DIR}" PARENT_SCOPE)
endfunction()
