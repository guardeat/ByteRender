# bytee::render_graph — passes declare resource use; the graph orders them, places barriers and owns transients.

add_library(bytee_render_graph ${BYTEE_LIBRARY_TYPE}
    src/graph_compiler.cpp
    src/graphviz.cpp
    src/render_graph.cpp
)
add_library(bytee::render_graph ALIAS bytee_render_graph)

target_include_directories(bytee_render_graph
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(bytee_render_graph PUBLIC bytee::rhi)

bytee_configure_library(bytee_render_graph GRAPH)
