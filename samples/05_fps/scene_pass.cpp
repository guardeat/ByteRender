#include "scene_pass.hpp"

#include "ground.frag.hpp"
#include "ground.vert.hpp"
#include "sphere.frag.hpp"
#include "sphere.vert.hpp"

#include <array>
#include <cmath>
#include <numbers>
#include <utility>

namespace bytee::samples {

namespace rhi = render::rhi;

namespace {

constexpr rhi::Format k_depth_format = rhi::Format::D32Float;

// Matches PushConstants in shaders/scene.glsl.
struct ScenePushConstants {
    Mat4 view_projection;
    std::array<float, 4> camera_position{};
    std::array<float, 4> fog_color{};
    uint64_t mesh = 0;
    uint64_t instances = 0;
};

core::Result<rhi::GraphicsPipeline> create_pipeline(rhi::Device& device, rhi::Format color_format,
                                                    std::span<const uint32_t> vertex,
                                                    std::span<const uint32_t> fragment, rhi::CullMode cull,
                                                    std::string_view name) {
    const std::array color_formats{color_format};
    return device.create_graphics_pipeline({
        .vertex = {.spirv = vertex},
        .fragment = {.spirv = fragment},
        .color_formats = color_formats,
        .depth_format = k_depth_format,
        .cull = cull,
        .depth = {.test = true, .write = true}, // reverse-Z: GREATER
        .debug_name = name,
    });
}

} // namespace

core::Result<ScenePass> create_scene_pass(rhi::Device& device, rhi::Format color_format, uint32_t frames_in_flight,
                                          uint32_t max_spheres) {
    ScenePass pass;
    pass.device_ = &device;
    core::Result<rhi::GraphicsPipeline> spheres = create_pipeline(device, color_format, shaders::sphere_vert,
                                                                  shaders::sphere_frag, rhi::CullMode::Back, "spheres");
    if (!spheres) {
        return std::move(spheres).error();
    }
    pass.sphere_pipeline_ = std::move(*spheres);
    core::Result<rhi::GraphicsPipeline> ground = create_pipeline(device, color_format, shaders::ground_vert,
                                                                 shaders::ground_frag, rhi::CullMode::None, "ground");
    if (!ground) {
        return std::move(ground).error();
    }
    pass.ground_pipeline_ = std::move(*ground);
    BYTEE_TRY(pass.create_sphere_mesh(24, 32));

    for (uint32_t i = 0; i < frames_in_flight; ++i) {
        core::Result<rhi::Buffer> instances =
            device.create_buffer({.size = uint64_t{max_spheres} * sizeof(ScenePass::GpuSphere),
                                  .usage = rhi::BufferUsage::Storage,
                                  .memory = rhi::MemoryLocation::Upload,
                                  .debug_name = "sphere instances"});
        if (!instances) {
            return std::move(instances).error();
        }
        pass.instance_buffers_.push_back(std::move(*instances));
    }
    pass.staging_.reserve(max_spheres);
    return pass;
}

// A UV sphere of radius 1 (positions double as normals), counter-clockwise seen from outside.
core::Result<void> ScenePass::create_sphere_mesh(uint32_t stacks, uint32_t slices) {
    std::vector<Vec3> positions;
    for (uint32_t stack = 0; stack <= stacks; ++stack) {
        const float polar = std::numbers::pi_v<float> * static_cast<float>(stack) / static_cast<float>(stacks);
        for (uint32_t slice = 0; slice <= slices; ++slice) {
            const float azimuth =
                2.0f * std::numbers::pi_v<float> * static_cast<float>(slice) / static_cast<float>(slices);
            positions.push_back(
                {std::sin(polar) * std::cos(azimuth), std::cos(polar), std::sin(polar) * std::sin(azimuth)});
        }
    }
    std::vector<uint16_t> indices;
    const uint32_t row = slices + 1;
    for (uint32_t stack = 0; stack < stacks; ++stack) {
        for (uint32_t slice = 0; slice < slices; ++slice) {
            const auto a = static_cast<uint16_t>(stack * row + slice);
            const auto b = static_cast<uint16_t>(a + row);
            const auto c = static_cast<uint16_t>(b + 1);
            const auto d = static_cast<uint16_t>(a + 1);
            indices.insert(indices.end(), {a, c, b, a, d, c});
        }
    }

    core::Result<rhi::Buffer> position_buffer =
        device_->create_buffer({.size = positions.size() * sizeof(Vec3),
                                .usage = rhi::BufferUsage::Storage | rhi::BufferUsage::CopyDst,
                                .debug_name = "sphere positions"});
    core::Result<rhi::Buffer> index_buffer =
        device_->create_buffer({.size = indices.size() * sizeof(uint16_t),
                                .usage = rhi::BufferUsage::Index | rhi::BufferUsage::CopyDst,
                                .debug_name = "sphere indices"});
    if (!position_buffer || !index_buffer) {
        return (!position_buffer ? position_buffer.error() : index_buffer.error());
    }
    BYTEE_TRY(device_->upload(*position_buffer, 0, std::as_bytes(std::span{positions})));
    BYTEE_TRY(device_->upload(*index_buffer, 0, std::as_bytes(std::span{indices})));
    mesh_positions_ = std::move(*position_buffer);
    mesh_indices_ = std::move(*index_buffer);
    mesh_index_count_ = static_cast<uint32_t>(indices.size());
    return {};
}

void ScenePass::setup(render::PassBuilder& builder) {
    const core::Extent3D extent = builder.desc(target_).extent;
    depth_ = builder.create_texture("depth", {.extent = extent, .format = k_depth_format});
    builder.color_attachment({.texture = target_, .clear = k_sky});
    builder.depth_attachment({.texture = depth_, .store = rhi::StoreOp::DontCare}); // only needed during the pass
}

void ScenePass::execute(render::PassContext& context) {
    // This frame's sphere data, in the shader's layout.
    const rhi::Buffer& instances = instance_buffers_[frame_index_];
    frame_index_ = (frame_index_ + 1) % static_cast<uint32_t>(instance_buffers_.size());
    staging_.clear();
    for (const Sphere& sphere : spheres_.first(std::min(spheres_.size(), staging_.capacity()))) {
        staging_.push_back({.position = sphere.position, .radius = sphere.radius, .color = sphere.color});
    }
    const bool has_spheres = !staging_.empty() && device_->write(instances, 0, std::as_bytes(std::span{staging_}));

    const ScenePushConstants constants{
        .view_projection = view_projection_,
        .camera_position = {camera_position_.x, camera_position_.y, camera_position_.z, 1.0f},
        .fog_color = {k_sky.r, k_sky.g, k_sky.b, 1.0f},
        .mesh = device_->info(mesh_positions_).device_address,
        .instances = device_->info(instances).device_address,
    };
    rhi::CommandList& commands = context.commands();
    commands.bind_pipeline(ground_pipeline_);
    commands.push_constants(constants);
    commands.draw(6);
    if (has_spheres) {
        commands.bind_pipeline(sphere_pipeline_);
        commands.push_constants(constants);
        commands.bind_index_buffer(mesh_indices_, 0, rhi::IndexType::Uint16);
        commands.draw_indexed(mesh_index_count_, static_cast<uint32_t>(staging_.size()));
    }
}

} // namespace bytee::samples
