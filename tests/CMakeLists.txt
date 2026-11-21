byte_render_fetch_doctest()

# Unit tests: no GPU needed.
add_executable(byte_render_tests
    main.cpp
    core/test_assert.cpp
    core/test_assert_disabled.cpp
    core/test_flags.cpp
    core/test_handle.cpp
    core/test_log.cpp
    core/test_pool.cpp
    core/test_resource.cpp
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

# Death tests: the program must abort with the expected report. CTest counts any crash as a failure, so they run
# through expect_abort.cmake, which requires an abnormal exit and matches the output. Regex metacharacters are written
# as bracket expressions ([(] rather than an escaped parenthesis): the pattern passes through several CMake parsing layers.
function(byte_render_add_death_test name source expected)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE byte::core)
    byte_render_configure_target(${name})
    add_test(NAME ${name}
        COMMAND "${CMAKE_COMMAND}" "-DEXECUTABLE=$<TARGET_FILE:${name}>" "-DEXPECTED=${expected}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/death/expect_abort.cmake"
    )
endfunction()

byte_render_add_death_test(byte_render_assert_failure death/assert_failure.cpp
    "tests/death/assert_failure.cpp:10: in 'int main[(][)]'.*condition: index < size.*message:   index 7 out of range [[]0, 4[)]")
byte_render_add_death_test(byte_render_pool_stale_handle death/pool_stale_handle.cpp
    "Pool::get: stale or invalid handle [(]index 0, generation 1[)]")
