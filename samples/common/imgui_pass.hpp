#pragma once

// Renders Dear ImGui with ByteRender: a render graph pass that draws ImGui's draw data over its target. The font atlas
// is a bindless texture (ImTextureID = bindless index), vertices are pulled through a buffer device address and every
// draw command gets its own scissor rectangle.
//
// Per-frame vertex and index data live in host-visible buffers, one pair per frame in flight: the swapchain's frame
// throttling guarantees the GPU has finished with a pair before it is rewritten. Needs a current ImGui context.

#include <bytee/core/result.hpp>
#include <bytee/render/graph/render_graph.hpp>

#include <cstdint>
#include <vector>

namespace bytee::samples {

class ImGuiPass {
private:
    struct FrameBuffers {
        render::rhi::Buffer vertices;
        render::rhi::Buffer indices;
    };

    render::rhi::Device* device_ = nullptr;
    render::rhi::GraphicsPipeline pipeline_;
    render::rhi::Sampler sampler_;
    render::rhi::Texture font_atlas_;
    std::vector<FrameBuffers> frames_; // ring, one entry per frame in flight
    uint32_t frame_index_ = 0;
    render::TextureRef target_;

public:
    void set_target(render::TextureRef target) noexcept { target_ = target; }

    void setup(render::PassBuilder& builder);
    void execute(render::PassContext& context);

private:
    friend core::Result<ImGuiPass> create_imgui_pass(render::rhi::Device& device, render::rhi::Format color_format,
                                                     uint32_t frames_in_flight);

    ImGuiPass() noexcept = default;

    // Grows `buffer` (recreating it) to hold at least `size` bytes.
    [[nodiscard]] core::Result<void> reserve(render::rhi::Buffer& buffer, uint64_t size,
                                             core::Flags<render::rhi::BufferUsage> usage);
};

// Creates the pipeline for `color_format` targets and uploads ImGui's font atlas.
[[nodiscard]] core::Result<ImGuiPass> create_imgui_pass(render::rhi::Device& device, render::rhi::Format color_format,
                                                        uint32_t frames_in_flight);

} // namespace bytee::samples
