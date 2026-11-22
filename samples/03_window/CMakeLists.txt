bytee_add_sample(03_window main.cpp)
target_link_libraries(03_window PRIVATE bytee_sample_common)
bytee_add_shaders(03_window SOURCES shaders/triangle.vert ${BYTEE_SAMPLES_COMMON_DIR}/shaders/vertex_color.frag)
