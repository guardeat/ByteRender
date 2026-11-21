byte_render_fetch_doctest()

# Unit tests: no GPU needed.
add_executable(byte_render_tests
    main.cpp
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
