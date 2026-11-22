#include "imgui_pass.hpp"

#include "imgui.frag.hpp"
#include "imgui.vert.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <imgui.h>
#include <span>
#include <utility>

namespace bytee::samples {

namespace rhi = render::rhi;

namespace {

struct ImGuiPushConstants {
    std::array<float, 2> scale{};
    std::array<float, 2> translate{};
    uint64_t vertices = 0;
    uint32_t texture_index = 0;
    uint32_t sampler_index = 0;
};

} // namespace

core::Result<ImGuiPass> create_imgui_pass(rhi::Device& device, rhi::Format color_format, uint32_t frames_in_flight) {
    ImGuiPass pass;
    pass.device_ = &device;
    pass.frames_.resize(frames_in_flight);

    const std::array color_formats{color_format};
    core::Result<rhi::GraphicsPipeline> pipeline = device.create_graphics_pipeline({
        .vertex = {.spirv = shaders::imgui_vert},
        .fragment = {.spirv = shaders::imgui_frag},
        .color_formats = color_formats,
        .blend = rhi::BlendMode::AlphaBlend,
        .debug_name = "imgui",
    });
    if (!pipeline) {
        return std::move(pipeline).error();
    }
    pass.pipeline_ = std::move(*pipeline);

    core::Result<rhi::Sampler> sampler = device.create_sampler({.address_u = rhi::AddressMode::ClampToEdge,
                                                                .address_v = rhi::AddressMode::ClampToEdge,
                                                                .debug_name = "imgui"});
    if (!sampler) {
        return std::move(sampler).error();
    }
    pass.sampler_ = std::move(*sampler);

    ImGuiIO& io = ImGui::GetIO();
    io.BackendRendererName = "bytee";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset; // draw_indexed takes a vertex offset

    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    core::Result<rhi::Texture> font_atlas = device.create_texture({
        .extent = {.width = static_cast<uint32_t>(width), .height = static_cast<uint32_t>(height)},
        .format = rhi::Format::RGBA8Unorm,
        .usage = rhi::TextureUsage::Sampled | rhi::TextureUsage::CopyDst,
        .debug_name = "imgui font atlas",
    });
    if (!font_atlas) {
        return std::move(font_atlas).error();
    }
    const std::span<const std::byte> texels{reinterpret_cast<const std::byte*>(pixels),
                                            static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4};
    BYTEE_TRY(device.upload(*font_atlas, texels, {})); // ends in ShaderRead
    pass.font_atlas_ = std::move(*font_atlas);
    io.Fonts->SetTexID(static_cast<ImTextureID>(rhi::bindless_index(pass.font_atlas_)));
    return pass;
}

void ImGuiPass::setup(render::PassBuilder& builder) {
    builder.color_attachment({.texture = target_, .load = rhi::LoadOp::Load}); // drawn over what is there
}

void ImGuiPass::execute(render::PassContext& context) {
    const ImDrawData* draw_data = ImGui::GetDrawData();
    if (draw_data == nullptr || draw_data->TotalVtxCount == 0) {
        return;
    }

    // Upload this frame's vertices and indices, one draw list after the other.
    FrameBuffers& frame = frames_[frame_index_];
    frame_index_ = (frame_index_ + 1) % static_cast<uint32_t>(frames_.size());
    const uint64_t vertex_bytes = static_cast<uint64_t>(draw_data->TotalVtxCount) * sizeof(ImDrawVert);
    const uint64_t index_bytes = static_cast<uint64_t>(draw_data->TotalIdxCount) * sizeof(ImDrawIdx);
    if (!reserve(frame.vertices, vertex_bytes, rhi::BufferUsage::Storage) ||
        !reserve(frame.indices, index_bytes, rhi::BufferUsage::Index)) {
        return; // out of memory: skip the UI this frame
    }
    uint64_t vertex_offset = 0;
    uint64_t index_offset = 0;
    for (const ImDrawList* list : draw_data->CmdLists) {
        const auto vertices =
            std::as_bytes(std::span{list->VtxBuffer.Data, static_cast<std::size_t>(list->VtxBuffer.Size)});
        const auto indices =
            std::as_bytes(std::span{list->IdxBuffer.Data, static_cast<std::size_t>(list->IdxBuffer.Size)});
        if (!device_->write(frame.vertices, vertex_offset, vertices) ||
            !device_->write(frame.indices, index_offset, indices)) {
            return;
        }
        vertex_offset += vertices.size();
        index_offset += indices.size();
    }

    // Display coordinates (points, top-left origin) -> Y-up clip space.
    const ImVec2 position = draw_data->DisplayPos;
    const ImVec2 size = draw_data->DisplaySize;
    ImGuiPushConstants constants{
        .scale = {2.0f / size.x, -2.0f / size.y},
        .translate = {-1.0f - position.x * 2.0f / size.x, 1.0f + position.y * 2.0f / size.y},
        .vertices = device_->info(frame.vertices).device_address,
        .sampler_index = rhi::bindless_index(sampler_),
    };

    rhi::CommandList& commands = context.commands();
    commands.bind_pipeline(pipeline_);
    commands.bind_index_buffer(frame.indices, 0,
                               sizeof(ImDrawIdx) == 2 ? rhi::IndexType::Uint16 : rhi::IndexType::Uint32);

    // Clip rectangles are in display coordinates; scissors in framebuffer pixels.
    const core::Extent3D target = context.desc(target_).extent;
    const ImVec2 scale = draw_data->FramebufferScale;
    uint32_t first_vertex = 0;
    uint32_t first_index = 0;
    for (const ImDrawList* list : draw_data->CmdLists) {
        for (const ImDrawCmd& command : list->CmdBuffer) {
            if (command.UserCallback != nullptr) {
                continue; // no callbacks in the samples
            }
            const float left = std::max((command.ClipRect.x - position.x) * scale.x, 0.0f);
            const float top = std::max((command.ClipRect.y - position.y) * scale.y, 0.0f);
            const float right = std::min((command.ClipRect.z - position.x) * scale.x, static_cast<float>(target.width));
            const float bottom =
                std::min((command.ClipRect.w - position.y) * scale.y, static_cast<float>(target.height));
            if (right <= left || bottom <= top) {
                continue;
            }
            commands.set_scissor({.offset = {.x = static_cast<int32_t>(left), .y = static_cast<int32_t>(top)},
                                  .extent = {.width = static_cast<uint32_t>(right - left),
                                             .height = static_cast<uint32_t>(bottom - top)}});
            constants.texture_index = static_cast<uint32_t>(command.GetTexID());
            commands.push_constants(constants);
            commands.draw_indexed(command.ElemCount, 1, first_index + command.IdxOffset,
                                  static_cast<int32_t>(first_vertex + command.VtxOffset));
        }
        first_vertex += static_cast<uint32_t>(list->VtxBuffer.Size);
        first_index += static_cast<uint32_t>(list->IdxBuffer.Size);
    }
}

core::Result<void> ImGuiPass::reserve(rhi::Buffer& buffer, uint64_t size, core::Flags<rhi::BufferUsage> usage) {
    if (buffer.is_valid() && device_->info(buffer).size >= size) {
        return {};
    }
    core::Result<rhi::Buffer> grown = device_->create_buffer({.size = std::max<uint64_t>(size * 2, 64 * 1024),
                                                              .usage = usage,
                                                              .memory = rhi::MemoryLocation::Upload,
                                                              .debug_name = "imgui geometry"});
    if (!grown) {
        return std::move(grown).error();
    }
    buffer = std::move(*grown); // the old buffer is released once the GPU is done with it
    return {};
}

} // namespace bytee::samples
