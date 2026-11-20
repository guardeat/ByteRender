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

add_executable(byte_render_tests
    main.cpp
    core/test_flags.cpp
    core/test_log.cpp
    core/test_result.cpp
)
target_link_libraries(byte_render_tests PRIVATE byte::core byte_render_doctest)
target_compile_features(byte_render_tests PRIVATE cxx_std_20)
byte_render_set_warnings(byte_render_tests)

add_test(NAME byte_render_tests COMMAND byte_render_tests)
