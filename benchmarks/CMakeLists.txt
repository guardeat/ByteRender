# Micro-benchmarks: built, not run as tests (timings belong in Release builds and are machine-dependent).
add_executable(bytee_pool_benchmark pool_benchmark.cpp)
target_link_libraries(bytee_pool_benchmark PRIVATE bytee::core)
bytee_configure_target(bytee_pool_benchmark)
