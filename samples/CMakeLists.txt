set(BYTEE_SAMPLES_COMMON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/common")

function(bytee_add_sample name)
    add_executable(${name} ${ARGN})
    target_include_directories(${name} PRIVATE "${BYTEE_SAMPLES_COMMON_DIR}") # header-only helpers (ppm.hpp)
    target_link_libraries(${name} PRIVATE bytee::render_graph)
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
endif()
