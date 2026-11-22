#pragma once

// Draws the ground and every sphere (one instanced draw) into its target, with a depth buffer the graph provides.
// Sphere data is copied each frame into host-visible buffers, one per frame in flight (the swapchain's throttling
// keeps the GPU from reading one while it is rewritten), and read by the shader through its device address.

#include "sample_math.hpp"
#include "sphere_world.hpp"

#include <bytee/core/result.hpp>
#include <bytee/render/graph/render_graph.hpp>

#include <cstdint>
#include <span>
#include <vector>

namespace bytee::samples {

class ScenePass {
public:
    // Matches SphereInstance in shaders/scene.glsl.
    struct GpuSphere {
        Vec3 position;
        float radius = 0.0f;
        Vec3 color;
        float alpha = 1.0f;
    };

private:
    render::rhi::Device* device_ = nullptr;
    render::rhi::GraphicsPipeline sphere_pipeline_;
    render::rhi::GraphicsPipeline ground_pipeline_;
    render::rhi::Buffer mesh_positions_;
    render::rhi::Buffer mesh_indices_;
    uint32_t mesh_index_count_ = 0;
    std::vector<render::rhi::Buffer> instance_buffers_; // ring, one per frame in flight
    uint32_t frame_index_ = 0;
    std::vector<GpuSphere> staging_;

    render::TextureRef target_;
    render::TextureRef depth_;
    Mat4 view_projection_;
    Vec3 camera_position_;
    std::span<const Sphere> spheres_;

public:
    static constexpr render::rhi::ClearColor k_sky{0.55f, 0.72f, 0.92f, 1.0f}; // linear

    void set_target(render::TextureRef target) noexcept { target_ = target; }
    void set_view(const Mat4& view_projection, Vec3 camera_position) noexcept {
        view_projection_ = view_projection;
        camera_position_ = camera_position;
    }
    // Must stay valid until the graph has recorded.
    void set_spheres(std::span<const Sphere> spheres) noexcept { spheres_ = spheres; }

    void setup(render::PassBuilder& builder);
    void execute(render::PassContext& context);

private:
    friend core::Result<ScenePass> create_scene_pass(render::rhi::Device& device, render::rhi::Format color_format,
                                                     uint32_t frames_in_flight, uint32_t max_spheres);

    ScenePass() noexcept = default;

    [[nodiscard]] core::Result<void> create_sphere_mesh(uint32_t stacks, uint32_t slices);
};

[[nodiscard]] core::Result<ScenePass> create_scene_pass(render::rhi::Device& device, render::rhi::Format color_format,
                                                        uint32_t frames_in_flight, uint32_t max_spheres);

} // namespace bytee::samples
