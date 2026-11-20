include(FetchContent)

FetchContent_Declare(doctest
    URL https://github.com/doctest/doctest/archive/refs/tags/v2.4.12.tar.gz
    URL_HASH SHA256=73381c7aa4dee704bd935609668cf41880ea7f19fa0504a200e13b74999c2d70
    # Header-only: don't run doctest's own CMakeLists (it targets an old CMake version).
    SOURCE_SUBDIR do-not-build
)
FetchContent_MakeAvailable(doctest)

add_library(byte_render_doctest INTERFACE)
target_include_directories(byte_render_doctest SYSTEM INTERFACE "${doctest_SOURCE_DIR}")

# Unit tests: no GPU needed.
add_executable(byte_render_tests
    main.cpp
    core/test_flags.cpp
    core/test_log.cpp
    core/test_result.cpp
    rhi/test_live_object_tracker.cpp
)
target_include_directories(byte_render_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/modules/rhi/src" # backend-agnostic internals (header-only)
)
target_link_libraries(byte_render_tests PRIVATE byte::core byte_render_doctest)
target_compile_features(byte_render_tests PRIVATE cxx_std_20)
byte_render_set_warnings(byte_render_tests)
add_test(NAME byte_render_tests COMMAND byte_render_tests)

# GPU tests: need a Vulkan 1.3 GPU. Run only these with `ctest -L gpu`, skip them with `ctest -LE gpu`.
add_executable(byte_render_gpu_tests
    main.cpp
    rhi/test_device.cpp
)
target_include_directories(byte_render_gpu_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(byte_render_gpu_tests PRIVATE byte::rhi byte_render_doctest)
target_compile_features(byte_render_gpu_tests PRIVATE cxx_std_20)
byte_render_set_warnings(byte_render_gpu_tests)
add_test(NAME byte_render_gpu_tests COMMAND byte_render_gpu_tests)
set_tests_properties(byte_render_gpu_tests PROPERTIES LABELS gpu)
