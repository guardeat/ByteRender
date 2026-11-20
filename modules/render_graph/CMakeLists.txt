# byte::render_graph — header-only placeholder until the graph is implemented (roadmap step 11),
# at which point it becomes a regular library configured with byte_render_configure_library().

add_library(byte_render_render_graph INTERFACE)
add_library(byte::render_graph ALIAS byte_render_render_graph)

target_include_directories(byte_render_render_graph INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(byte_render_render_graph INTERFACE byte::rhi)
