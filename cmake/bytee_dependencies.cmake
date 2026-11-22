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

# Vulkan backend: Vulkan-Headers + volk + Vulkan Memory Allocator (headers only; the volk and VMA implementations are
# compiled by the backend itself).
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
    FetchContent_Declare(vma
        URL https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/archive/refs/tags/v3.5.0.tar.gz
        URL_HASH SHA256=c5f3db848fca9cb9726b41f256cc3007773321fa92a86d1e0902e66474e8a9e3
        SOURCE_SUBDIR do-not-build
    )
    FetchContent_MakeAvailable(vulkan_headers volk vma)

    add_library(bytee_vulkan_dependencies INTERFACE)
    target_include_directories(bytee_vulkan_dependencies SYSTEM INTERFACE
        "${vulkan_headers_SOURCE_DIR}/include"
        "${volk_SOURCE_DIR}"
        "${vma_SOURCE_DIR}/include"
    )
    # volk provides every Vulkan function as a pointer; the loader is opened at runtime (no link dependency).
    target_compile_definitions(bytee_vulkan_dependencies INTERFACE
        VK_NO_PROTOTYPES
        # VMA fetches the Vulkan functions it needs through volk's vkGetInstanceProcAddr / vkGetDeviceProcAddr.
        VMA_STATIC_VULKAN_FUNCTIONS=0
        VMA_DYNAMIC_VULKAN_FUNCTIONS=1
        VMA_VULKAN_VERSION=1003000
    )
    target_link_libraries(bytee_vulkan_dependencies INTERFACE ${CMAKE_DL_LIBS})
    # Window-system surface types and functions for this platform. Linux needs none: its surfaces are created in
    # vulkan_surface_linux.cpp without the X11 / XCB / Wayland headers.
    if(APPLE)
        target_compile_definitions(bytee_vulkan_dependencies INTERFACE VK_USE_PLATFORM_METAL_EXT)
    elseif(WIN32)
        target_compile_definitions(bytee_vulkan_dependencies INTERFACE VK_USE_PLATFORM_WIN32_KHR WIN32_LEAN_AND_MEAN
                                                                       NOMINMAX)
    endif()
endfunction()

# GLFW: windowing for the samples only (the engine never owns windows). Built with its own CMake, without docs,
# tests, examples or install rules; its sources compile without warnings output (third-party code).
function(bytee_fetch_glfw)
    if(TARGET glfw)
        return()
    endif()
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(glfw
        URL https://github.com/glfw/glfw/archive/refs/tags/3.4.tar.gz
        URL_HASH SHA256=c038d34200234d071fae9345bc455e4a8f2f544ab60150765d7704e08f3dac01
        SYSTEM
    )
    FetchContent_MakeAvailable(glfw)
    if(MSVC)
        target_compile_options(glfw PRIVATE /W0)
    else()
        target_compile_options(glfw PRIVATE -w)
    endif()
endfunction()

# Dear ImGui: debug UI for the samples only, with its GLFW input backend (ByteRender renders the draw data itself, see
# samples/common/imgui_pass). Pinned to the last release before the 1.92 texture rework; third-party code compiles
# without warnings output, and its headers are SYSTEM includes.
function(bytee_fetch_imgui)
    if(TARGET bytee_imgui)
        return()
    endif()
    FetchContent_Declare(imgui
        URL https://github.com/ocornut/imgui/archive/refs/tags/v1.91.9b.tar.gz
        URL_HASH SHA256=8e1bbc76c71d74fef2fb85db7e7ca8eba13d6a86623c54992b60162db554ffdb
    )
    FetchContent_MakeAvailable(imgui)
    add_library(bytee_imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    )
    target_include_directories(bytee_imgui SYSTEM PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
    target_link_libraries(bytee_imgui PUBLIC glfw)
    if(MSVC)
        target_compile_options(bytee_imgui PRIVATE /W0)
    else()
        target_compile_options(bytee_imgui PRIVATE -w)
    endif()
endfunction()
