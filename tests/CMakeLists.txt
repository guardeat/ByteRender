byte_render_fetch_doctest()

# Unit tests: no GPU needed.
add_executable(byte_render_tests
    main.cpp
    core/test_assert.cpp
    core/test_assert_disabled.cpp
    core/test_flags.cpp
    core/test_log.cpp
    core/test_result.cpp
    core/test_types.cpp
    core/test_version.cpp
)
target_include_directories(byte_render_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(byte_render_tests PRIVATE byte::core byte_render_doctest)
target_compile_definitions(byte_render_tests PRIVATE
    BYTE_RENDER_EXPECTED_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    BYTE_RENDER_EXPECTED_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    BYTE_RENDER_EXPECTED_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)
byte_render_configure_target(byte_render_tests)
add_test(NAME byte_render_tests COMMAND byte_render_tests)

# A failing assert must abort and report condition, formatted message and location.
add_executable(byte_render_assert_failure death/assert_failure.cpp)
target_link_libraries(byte_render_assert_failure PRIVATE byte::core)
byte_render_configure_target(byte_render_assert_failure)
add_test(NAME byte_render_assert_failure
    COMMAND "${CMAKE_COMMAND}"
        "-DEXECUTABLE=$<TARGET_FILE:byte_render_assert_failure>"
        "-DEXPECTED=tests/death/assert_failure.cpp:10: in 'int main\\(\\)'.*condition: index < size.*message:   index 7 out of range \\[0, 4\\)"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/death/expect_abort.cmake"
)
