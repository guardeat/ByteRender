# bytee::render_graph — headers only until the graph is implemented (roadmap step 14),
# then a regular library configured with bytee_configure_library().

add_library(bytee_render_graph INTERFACE)
add_library(bytee::render_graph ALIAS bytee_render_graph)

target_include_directories(bytee_render_graph INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(bytee_render_graph INTERFACE bytee::rhi)
