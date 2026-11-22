set(BYTEE_SAMPLES_COMMON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/common")

# bytee_add_sample(name SOURCES ... [LIBRARIES ...]): the sample's own libraries are linked before the engine, so
# static libraries that depend on the engine do not repeat it on the link line.
function(bytee_add_sample name)
    cmake_parse_arguments(PARSE_ARGV 1 sample "" "" "SOURCES;LIBRARIES")
    add_executable(${name} ${sample_SOURCES})
    target_include_directories(${name} PRIVATE "${BYTEE_SAMPLES_COMMON_DIR}") # header-only helpers (ppm.hpp, ...)
    target_link_libraries(${name} PRIVATE ${sample_LIBRARIES} bytee::render_graph)
    bytee_configure_target(${name})
endfunction()

add_subdirectory(00_device_info)
add_subdirectory(01_headless_clear)
add_subdirectory(02_triangle)

# Windowed samples: GLFW + a small helper library (samples/common). Linux support: roadmap step 18.
if(APPLE OR WIN32)
    bytee_fetch_glfw()
    if(APPLE)
        set(native_window_source common/native_window_macos.mm) # OBJCXX is enabled by the top-level CMakeLists
    else()
        set(native_window_source common/native_window_win32.cpp)
    endif()
    add_library(bytee_sample_common STATIC common/sample_window.cpp ${native_window_source})
    target_include_directories(bytee_sample_common PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/common")
    target_link_libraries(bytee_sample_common PUBLIC bytee::rhi PRIVATE glfw)
    if(APPLE)
        target_link_libraries(bytee_sample_common PRIVATE "-framework QuartzCore" "-framework AppKit")
    endif()
    bytee_configure_target(bytee_sample_common)

    add_subdirectory(03_window)

    # Debug UI: Dear ImGui rendered by a render graph pass (samples/common/imgui_pass).
    bytee_fetch_imgui()
    add_library(bytee_sample_imgui STATIC common/imgui_pass.cpp)
    target_include_directories(bytee_sample_imgui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/common")
    target_link_libraries(bytee_sample_imgui PUBLIC bytee::render_graph bytee_imgui)
    bytee_add_shaders(bytee_sample_imgui SOURCES common/shaders/imgui.vert common/shaders/imgui.frag)
    bytee_configure_target(bytee_sample_imgui)

    add_subdirectory(04_imgui)
endif()
