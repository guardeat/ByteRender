bytee_fetch_doctest()

# Unit tests: no GPU needed.
add_executable(bytee_tests
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
target_include_directories(bytee_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(bytee_tests PRIVATE bytee::core bytee_doctest)
target_compile_definitions(bytee_tests PRIVATE
    BYTEE_EXPECTED_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    BYTEE_EXPECTED_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    BYTEE_EXPECTED_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)
bytee_configure_target(bytee_tests)
add_test(NAME bytee_tests COMMAND bytee_tests)

# GPU tests through the public API (label "gpu"; skip on machines without a GPU: ctest -LE gpu).
add_executable(bytee_gpu_tests
    main.cpp
    rhi/test_instance.cpp
)
target_include_directories(bytee_gpu_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(bytee_gpu_tests PRIVATE bytee::rhi bytee_doctest)
bytee_configure_target(bytee_gpu_tests)
add_test(NAME bytee_gpu_tests COMMAND bytee_gpu_tests)
set_tests_properties(bytee_gpu_tests PROPERTIES LABELS gpu)

# White-box backend tests: use backend internals, so only for static Vulkan builds (shared builds hide them).
if(NOT BYTEE_BUILD_SHARED AND BYTEE_RHI_BACKEND STREQUAL "Vulkan")
    add_executable(bytee_vulkan_tests
        main.cpp
        vulkan/test_vulkan_common.cpp
    )
    target_include_directories(bytee_vulkan_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${PROJECT_SOURCE_DIR}/modules/rhi/src"
    )
    target_link_libraries(bytee_vulkan_tests PRIVATE bytee::rhi bytee_vulkan_dependencies bytee_doctest)
    bytee_configure_target(bytee_vulkan_tests)
    add_test(NAME bytee_vulkan_tests COMMAND bytee_vulkan_tests)
endif()

# Death tests: the program must abort with the expected report. CTest counts any crash as a failure, so they run
# through expect_abort.cmake, which requires an abnormal exit and matches the output. Regex metacharacters are written
# as bracket expressions ([(] rather than an escaped parenthesis): the pattern passes through several CMake parsing layers.
function(bytee_add_death_test name source expected)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE bytee::core)
    bytee_configure_target(${name})
    add_test(NAME ${name}
        COMMAND "${CMAKE_COMMAND}" "-DEXECUTABLE=$<TARGET_FILE:${name}>" "-DEXPECTED=${expected}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/death/expect_abort.cmake"
    )
endfunction()

bytee_add_death_test(bytee_assert_failure death/assert_failure.cpp
    "tests/death/assert_failure.cpp:10: in 'int main[(][)]'.*condition: index < size.*message:   index 7 out of range [[]0, 4[)]")
bytee_add_death_test(bytee_pool_stale_handle death/pool_stale_handle.cpp
    "Pool::get: stale or invalid handle [(]index 0, generation 1[)]")
