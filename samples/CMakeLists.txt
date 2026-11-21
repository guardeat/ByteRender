function(byte_render_add_sample name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE byte::render_graph)
    byte_render_configure_target(${name})
endfunction()

add_subdirectory(00_device_info)
