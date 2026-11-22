// RenderGraph::to_graphviz: the graph as a Graphviz DOT document.

#include "graph_state.hpp"

#include <bytee/render/graph/render_graph.hpp>

#include <format>
#include <iterator>
#include <string>
#include <string_view>

namespace bytee::render {

namespace {

// DOT string literal contents: quotes and backslashes escaped.
std::string dot_escape(std::string_view text) {
    std::string escaped;
    escaped.reserve(text.size());
    for (const char c : text) {
        if (c == '"' || c == '\\') {
            escaped.push_back('\\');
        }
        escaped.push_back(c);
    }
    return escaped;
}

std::string resource_node(ResourceKind kind, uint32_t index) {
    return std::format("{}{}", kind == ResourceKind::Texture ? "texture" : "buffer", index);
}

} // namespace

std::string RenderGraph::to_graphviz() const {
    const detail::GraphPasses& passes = state_->passes;
    const detail::GraphResources& resources = state_->resources;
    std::string dot = "digraph RenderGraph {\n    rankdir=LR;\n    node [fontname=\"Helvetica\"];\n"
                      "    edge [fontname=\"Helvetica\", fontsize=10];\n";
    auto out = std::back_inserter(dot);

    for (uint32_t i = 0; i < passes.count(); ++i) {
        const bool culled = state_->compiled && state_->schedule.passes[i].culled;
        std::format_to(out, "    pass{} [label=\"{}{}\", shape=box, style=\"{}\", fillcolor=\"{}\"{}];\n", i,
                       dot_escape(passes.name(passes.pass(i))), culled ? " (culled)" : "",
                       culled ? "filled,dashed" : "filled", culled ? "#eeeeee" : "#cfe2ff",
                       culled ? ", fontcolor=\"#888888\"" : "");
    }
    for (uint32_t i = 0; i < resources.texture_count(); ++i) {
        const detail::GraphResources::Texture& texture = resources.texture(i);
        std::format_to(out, "    texture{} [label=\"{}\\n{}x{} {}\", shape=ellipse{}];\n", i,
                       dot_escape(resources.name(texture)), texture.desc.extent.width, texture.desc.extent.height,
                       rhi::to_string(texture.desc.format), texture.imported ? ", style=bold" : "");
    }
    for (uint32_t i = 0; i < resources.buffer_count(); ++i) {
        const detail::GraphResources::Buffer& buffer = resources.buffer(i);
        std::format_to(out, "    buffer{} [label=\"{}\\n{} bytes\", shape=ellipse{}];\n", i,
                       dot_escape(resources.name(buffer)), buffer.desc.size, buffer.imported ? ", style=bold" : "");
    }
    // Reads point into the pass, writes out of it.
    for (uint32_t i = 0; i < passes.count(); ++i) {
        const std::string pass_node = std::format("pass{}", i);
        for (const ResourceAccess& access : passes.accesses(passes.pass(i))) {
            const std::string resource = resource_node(access.kind, access.resource);
            std::format_to(out, "    {} -> {} [label=\"{}\"];\n", access.writes ? pass_node : resource,
                           access.writes ? resource : pass_node, rhi::to_string(access.state));
        }
    }
    dot += "}\n";
    return dot;
}

} // namespace bytee::render
