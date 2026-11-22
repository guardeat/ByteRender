#pragma once

// The RGB triangle as a render graph pass: clears its target and draws the triangle undistorted whatever the target's
// aspect ratio. Needs the shaders triangle.vert and vertex_color.frag from samples/common/shaders compiled into the
// sample (bytee_add_shaders).

#include "triangle.vert.hpp"
#include "vertex_color.frag.hpp"

#include <bytee/core/result.hpp>
#include <bytee/render/graph/render_graph.hpp>

#include <array>
#include <utility>

namespace bytee::samples {

class TrianglePass {
private:
    render::rhi::GraphicsPipeline pipeline_;
    render::TextureRef target_;
    render::rhi::ClearColor clear_color_{0.02f, 0.02f, 0.03f, 1.0f};

public:
    explicit TrianglePass(render::rhi::GraphicsPipeline pipeline) noexcept
        : pipeline_(std::move(pipeline)) {}

    void set_target(render::TextureRef target) noexcept { target_ = target; }
    void set_clear_color(const render::rhi::ClearColor& color) noexcept { clear_color_ = color; }

    void setup(render::PassBuilder& builder) { builder.color_attachment({.texture = target_, .clear = clear_color_}); }
    void execute(render::PassContext& context) {
        const core::Extent3D extent = context.desc(target_).extent;
        context.commands().bind_pipeline(pipeline_);
        context.commands().push_constants(static_cast<float>(extent.height) / static_cast<float>(extent.width));
        context.commands().draw(3);
    }
};

[[nodiscard]] inline core::Result<TrianglePass> create_triangle_pass(render::rhi::Device& device,
                                                                     render::rhi::Format color_format) {
    const std::array color_formats{color_format};
    core::Result<render::rhi::GraphicsPipeline> pipeline = device.create_graphics_pipeline({
        .vertex = {.spirv = shaders::triangle_vert},
        .fragment = {.spirv = shaders::vertex_color_frag},
        .color_formats = color_formats,
        .debug_name = "triangle",
    });
    if (!pipeline) {
        return std::move(pipeline).error();
    }
    return TrianglePass{std::move(*pipeline)};
}

} // namespace bytee::samples
