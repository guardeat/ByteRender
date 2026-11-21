# Micro-benchmarks: built, not run as tests (timings belong in Release builds and are machine-dependent).
add_executable(byte_render_pool_benchmark pool_benchmark.cpp)
target_link_libraries(byte_render_pool_benchmark PRIVATE byte::core)
byte_render_configure_target(byte_render_pool_benchmark)
