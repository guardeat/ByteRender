bytee_add_sample(03_window main.cpp)
target_link_libraries(03_window PRIVATE bytee_sample_common)
bytee_add_shaders(03_window SOURCES shaders/triangle.vert ../02_triangle/shaders/triangle.frag)
