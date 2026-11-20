add_executable(hello main.cpp)
target_link_libraries(hello PRIVATE byte::render_graph)
target_compile_features(hello PRIVATE cxx_std_20)
byte_renderer_set_warnings(hello)
