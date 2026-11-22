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
    rhi/test_format.cpp
    rhi/test_image_support.cpp
    rhi/test_live_object_tracker.cpp
    rhi/test_retire_list.cpp
)
target_include_directories(bytee_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/modules/rhi/src" # backend-agnostic internals (header-only)
)
target_link_libraries(bytee_tests PRIVATE bytee::rhi bytee_doctest) # rhi: header-only checks (formats); no GPU use
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
    graph/test_render_graph.cpp
    rhi/test_bindless.cpp
    rhi/test_commands.cpp
    rhi/test_compute.cpp
    rhi/test_device.cpp
    rhi/test_graphics.cpp
    rhi/test_instance.cpp
    rhi/test_rendering.cpp
    rhi/test_resources.cpp
    rhi/test_swapchain.cpp
    rhi/test_sync.cpp
)
target_include_directories(bytee_gpu_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
bytee_add_shaders(bytee_gpu_tests SOURCES
    shaders/color.frag
    shaders/fullscreen.vert
    shaders/fullscreen_uv.vert
    shaders/gradient.comp
    shaders/pulled.vert
    shaders/squares.comp
    shaders/textured.frag
    shaders/triangle.vert
)
target_link_libraries(bytee_gpu_tests PRIVATE bytee::render_graph bytee_doctest)
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

    add_executable(bytee_vulkan_gpu_tests
        main.cpp
        vulkan/test_vulkan_commands.cpp
        vulkan/test_vulkan_device.cpp
        vulkan/test_vulkan_queue.cpp
        vulkan/test_vulkan_resources.cpp
    )
    target_include_directories(bytee_vulkan_gpu_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${PROJECT_SOURCE_DIR}/modules/rhi/src"
    )
    target_link_libraries(bytee_vulkan_gpu_tests PRIVATE bytee::rhi bytee_vulkan_dependencies bytee_doctest)
    bytee_add_shaders(bytee_vulkan_gpu_tests SOURCES shaders/color.frag shaders/triangle.vert)
    bytee_configure_target(bytee_vulkan_gpu_tests)
    add_test(NAME bytee_vulkan_gpu_tests COMMAND bytee_vulkan_gpu_tests)
    set_tests_properties(bytee_vulkan_gpu_tests PROPERTIES LABELS gpu)
endif()

# Death tests: the program must abort with the expected report. CTest counts any crash as a failure, so they run
# through expect_abort.cmake, which requires an abnormal exit and matches the output. Regex metacharacters are written
# as bracket expressions ([(] rather than an escaped parenthesis): the pattern passes through several CMake parsing layers.
function(bytee_add_death_test name source library expected)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE ${library})
    bytee_configure_target(${name})
    add_test(NAME ${name}
        COMMAND "${CMAKE_COMMAND}" "-DEXECUTABLE=$<TARGET_FILE:${name}>" "-DEXPECTED=${expected}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/death/expect_abort.cmake"
    )
endfunction()

bytee_add_death_test(bytee_assert_failure death/assert_failure.cpp bytee::core
    "tests/death/assert_failure.cpp:10: in 'int main[(][)]'.*condition: index < size.*message:   index 7 out of range [[]0, 4[)]")
bytee_add_death_test(bytee_pool_stale_handle death/pool_stale_handle.cpp bytee::core
    "Pool::get: stale or invalid handle [(]index 0, generation 1[)]")

# Misuse of the RHI (e.g. a parent destroyed before its child): needs a GPU, and asserts compiled into the library (not just the test).
if(BYTEE_ASSERTS STREQUAL "ON" OR (BYTEE_ASSERTS STREQUAL "AUTO" AND CMAKE_BUILD_TYPE STREQUAL "Debug"))
    bytee_add_death_test(bytee_device_outlives_instance death/device_outlives_instance.cpp bytee::rhi
        "Instance destroyed while Device 'Leaky Device' is still alive.*every Device must be destroyed before its Instance")
    set_tests_properties(bytee_device_outlives_instance PROPERTIES LABELS gpu)
    bytee_add_death_test(bytee_texture_outlives_device death/texture_outlives_device.cpp bytee::rhi
        "Device 'Leaky Device' destroyed while Texture 'leaked' is still alive.*every resource must be destroyed before its Device")
    set_tests_properties(bytee_texture_outlives_device PROPERTIES LABELS gpu)
    bytee_add_death_test(bytee_command_list_outlives_device death/command_list_outlives_device.cpp bytee::rhi
        "Device 'Busy Device' destroyed while 1 CommandList[(]s[)] are alive.*every CommandList must be submitted")
    set_tests_properties(bytee_command_list_outlives_device PROPERTIES LABELS gpu)
    bytee_add_death_test(bytee_dispatch_without_pipeline death/dispatch_without_pipeline.cpp bytee::rhi
        "condition: context_->compute_pipeline_bound.*dispatch needs a compute pipeline")
    set_tests_properties(bytee_dispatch_without_pipeline PROPERTIES LABELS gpu)
    bytee_add_death_test(bytee_graph_resource_used_twice death/graph_resource_used_twice.cpp bytee::render_graph
        "pass 'Read and write': resource 0 is used more than once")
    set_tests_properties(bytee_graph_resource_used_twice PROPERTIES LABELS gpu)
endif()
