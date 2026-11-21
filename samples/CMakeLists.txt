function(bytee_add_sample name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE bytee::render_graph)
    bytee_configure_target(${name})
endfunction()

add_subdirectory(00_device_info)
add_subdirectory(01_headless_clear)
add_subdirectory(02_triangle)
