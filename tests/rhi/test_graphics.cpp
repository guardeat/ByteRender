// Graphics pipelines and draws (GPU tests), verified on the rendered images. The fixture fails on any validation
// warning or error.

#include "color.frag.hpp"
#include "fullscreen.vert.hpp"
#include "pulled.vert.hpp"
#include "support/error_text.hpp"
#include "support/gpu_fixture.hpp"
#include "support/image.hpp"
#include "triangle.vert.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cstdint>

namespace rhi = bytee::render::rhi;
using bytee::tests::DeviceFixture;
using bytee::tests::Rgba8;
using bytee::tests::Rgba8Image;
using rhi::ResourceState;
using rhi::TextureUsage;

namespace {

constexpr uint32_t k_size = 64;
constexpr rhi::Format k_color_format = rhi::Format::RGBA8Unorm;
constexpr Rgba8 k_background{0, 0, 0, 255};
constexpr Rgba8 k_red{255, 0, 0, 255};
constexpr Rgba8 k_green{0, 255, 0, 255};

struct TrianglePushConstants {
    float color[4] = {1.0f, 0.0f, 0.0f, 1.0f};
    float offset[2] = {0.0f, 0.0f};
    float depth = 0.5f;
    uint32_t reverse_winding = 0;
};
static_assert(sizeof(TrianglePushConstants) == 32);

struct PulledPushConstants {
    float color[4];
    uint64_t positions; // buffer device address
};
static_assert(sizeof(PulledPushConstants) == 24);

rhi::GraphicsPipelineDesc pipeline_desc(std::span<const uint32_t> vertex) {
    static constexpr std::array k_formats{k_color_format};
    return rhi::GraphicsPipelineDesc{.vertex = {.spirv = vertex},
                                     .fragment = {.spirv = shaders::color_frag},
                                     .color_formats = k_formats,
                                     .debug_name = "test pipeline"};
}

// Renders into a fresh 64x64 target cleared to black and returns the image.
template <class Record_>
Rgba8Image render(rhi::Device& device, Record_&& record, rhi::TextureHandle depth = {}) {
    rhi::Texture target = device
                              .create_texture({.extent = {.width = k_size, .height = k_size},
                                               .format = k_color_format,
                                               .usage = TextureUsage::ColorAttachment | TextureUsage::TransferSrc,
                                               .debug_name = "target"})
                              .value();
    auto commands = device.begin_commands();
    REQUIRE(commands.has_value());
    std::array barriers{
        rhi::TextureBarrier{
            .texture = target, .before = ResourceState::Undefined, .after = ResourceState::ColorAttachment},
        rhi::TextureBarrier{.texture = depth, .before = ResourceState::Undefined, .after = ResourceState::DepthWrite},
    };
    commands->barrier({}, std::span{barriers.data(), depth.is_valid() ? 2u : 1u});
    const rhi::ColorAttachment color{.texture = target, .clear = {0.0f, 0.0f, 0.0f, 1.0f}};
    std::optional<rhi::DepthAttachment> depth_attachment;
    if (depth.is_valid()) {
        depth_attachment = rhi::DepthAttachment{.texture = depth};
    }
    commands->begin_render_pass({.colors = std::span{&color, 1}, .depth = depth_attachment});
    record(*commands);
    commands->end_render_pass();
    auto done = device.submit(std::move(*commands));
    REQUIRE_MESSAGE(done.has_value(), bytee::tests::error_text(done));
    return bytee::tests::read_rgba8(device, target, ResourceState::ColorAttachment);
}

std::size_t count_pixels(const Rgba8Image& image, Rgba8 color) {
    return static_cast<std::size_t>(std::ranges::count(image.pixels, color));
}

} // namespace

TEST_CASE_FIXTURE(DeviceFixture, "A triangle is rasterized Y-up with the expected coverage") {
    rhi::GraphicsPipeline pipeline = device.create_graphics_pipeline(pipeline_desc(shaders::triangle_vert)).value();
    const Rgba8Image image = render(device, [&](rhi::CommandList& commands) {
        commands.bind_pipeline(pipeline);
        commands.push_constants(TrianglePushConstants{});
        commands.draw(3);
    });

    // Apex at clip y = +0.5 must be in the *upper* half of the image (row 0 is the top).
    CHECK(image.at(32, 18) == k_red);
    CHECK(image.at(32, 45) == k_red);
    CHECK(image.at(32, 60) == k_background); // below the base
    CHECK(image.at(32, 4) == k_background);  // above the apex
    CHECK(image.at(2, 2) == k_background);
    CHECK(image.at(61, 61) == k_background);
    // Area: half of a 0.5-wide... (base 1, height 1 in a 2x2 clip square) = 1/8 of the pixels.
    const std::size_t covered = count_pixels(image, k_red);
    CHECK(covered >= 480);
    CHECK(covered <= 544);
    if (image.at(32, 18) != k_red) {
        bytee::tests::write_ppm(image, "triangle_actual.ppm");
    }
}

TEST_CASE_FIXTURE(DeviceFixture, "Back-face culling follows counter-clockwise front faces in Y-up clip space") {
    rhi::GraphicsPipelineDesc desc = pipeline_desc(shaders::triangle_vert);
    desc.cull = rhi::CullMode::Back;
    rhi::GraphicsPipeline ccw_front = device.create_graphics_pipeline(desc).value();
    desc.front_face = rhi::FrontFace::Clockwise;
    rhi::GraphicsPipeline cw_front = device.create_graphics_pipeline(desc).value();

    const auto draw = [&](rhi::GraphicsPipelineHandle pipeline, uint32_t reverse) {
        return render(device, [&](rhi::CommandList& commands) {
            commands.bind_pipeline(pipeline);
            commands.push_constants(TrianglePushConstants{.reverse_winding = reverse});
            commands.draw(3);
        });
    };
    CHECK(count_pixels(draw(ccw_front, 0), k_red) > 0);  // CCW triangle, CCW is front: visible
    CHECK(count_pixels(draw(ccw_front, 1), k_red) == 0); // CW triangle is a back face: culled
    CHECK(count_pixels(draw(cw_front, 1), k_red) > 0);   // CW triangle, CW is front: visible
    CHECK(count_pixels(draw(cw_front, 0), k_red) == 0);
}

TEST_CASE_FIXTURE(DeviceFixture, "Reverse-Z: the nearer surface wins whatever the draw order") {
    rhi::Texture depth = device
                             .create_texture({.extent = {.width = k_size, .height = k_size},
                                              .format = rhi::Format::D32Float,
                                              .usage = TextureUsage::DepthStencilAttachment,
                                              .debug_name = "depth"})
                             .value();
    rhi::GraphicsPipelineDesc desc = pipeline_desc(shaders::fullscreen_vert);
    desc.depth_format = rhi::Format::D32Float;
    desc.depth = {.test = true, .write = true}; // compare defaults to Greater (reverse-Z)
    rhi::GraphicsPipeline pipeline = device.create_graphics_pipeline(desc).value();

    const TrianglePushConstants near_red{.color = {1, 0, 0, 1}, .depth = 0.75f};
    const TrianglePushConstants far_green{.color = {0, 1, 0, 1}, .depth = 0.25f};
    for (const bool near_first : {true, false}) {
        const Rgba8Image image = render(
            device,
            [&](rhi::CommandList& commands) {
                commands.bind_pipeline(pipeline);
                commands.push_constants(near_first ? near_red : far_green);
                commands.draw(3);
                commands.push_constants(near_first ? far_green : near_red);
                commands.draw(3);
            },
            depth);
        CHECK(count_pixels(image, k_red) == k_size * k_size);
    }
}

TEST_CASE_FIXTURE(DeviceFixture, "Alpha blending") {
    rhi::GraphicsPipelineDesc desc = pipeline_desc(shaders::fullscreen_vert);
    desc.blend = rhi::BlendMode::AlphaBlend;
    rhi::GraphicsPipeline pipeline = device.create_graphics_pipeline(desc).value();
    const Rgba8Image image = render(device, [&](rhi::CommandList& commands) {
        commands.bind_pipeline(pipeline);
        commands.push_constants(TrianglePushConstants{.color = {1.0f, 0.0f, 0.0f, 0.5f}});
        commands.draw(3);
    });
    // 1.0 * 0.5 + black * 0.5 = 0.5 -> 127 or 128 depending on rounding.
    bytee::tests::check_image(image, bytee::tests::solid_image(k_size, k_size, Rgba8{128, 0, 0, 255}), 1, "blend");
}

TEST_CASE_FIXTURE(DeviceFixture, "Vertex pulling through a buffer device address, with an index buffer") {
    const std::array<float, 8> corners{-1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f};
    const std::array<uint16_t, 6> indices{0, 1, 2, 2, 1, 3};
    rhi::Buffer positions = device
                                .create_buffer({.size = sizeof(corners),
                                                .usage = rhi::BufferUsage::Storage | rhi::BufferUsage::TransferDst,
                                                .debug_name = "positions"})
                                .value();
    rhi::Buffer index_buffer = device
                                   .create_buffer({.size = sizeof(indices),
                                                   .usage = rhi::BufferUsage::Index | rhi::BufferUsage::TransferDst,
                                                   .debug_name = "indices"})
                                   .value();
    REQUIRE(device.upload(positions, 0, std::as_bytes(std::span{corners})).has_value());
    REQUIRE(device.upload(index_buffer, 0, std::as_bytes(std::span{indices})).has_value());

    rhi::GraphicsPipeline pipeline = device.create_graphics_pipeline(pipeline_desc(shaders::pulled_vert)).value();
    const Rgba8Image image = render(device, [&](rhi::CommandList& commands) {
        commands.bind_pipeline(pipeline);
        commands.push_constants(
            PulledPushConstants{.color = {0, 1, 0, 1}, .positions = device.info(positions).device_address});
        commands.bind_index_buffer(index_buffer, 0, rhi::IndexType::Uint16);
        commands.draw_indexed(static_cast<uint32_t>(indices.size()));
    });
    CHECK(count_pixels(image, k_green) == k_size * k_size); // the quad covers everything
}

TEST_CASE_FIXTURE(DeviceFixture, "A pipeline released right after its submission stays alive until the GPU is done") {
    auto pipeline = device.create_graphics_pipeline(pipeline_desc(shaders::triangle_vert));
    REQUIRE(pipeline.has_value());
    rhi::Texture target =
        device
            .create_texture(
                {.extent = {.width = 8, .height = 8}, .format = k_color_format, .usage = TextureUsage::ColorAttachment})
            .value();
    auto commands = device.begin_commands();
    REQUIRE(commands.has_value());
    const rhi::TextureBarrier to_attachment{
        .texture = target, .before = ResourceState::Undefined, .after = ResourceState::ColorAttachment};
    commands->barrier({}, std::span{&to_attachment, 1});
    const rhi::ColorAttachment color{.texture = target};
    commands->begin_render_pass({.colors = std::span{&color, 1}});
    commands->bind_pipeline(*pipeline);
    commands->push_constants(TrianglePushConstants{});
    commands->draw(3);
    commands->end_render_pass();
    auto done = device.submit(std::move(*commands));
    REQUIRE(done.has_value());
    *pipeline = rhi::GraphicsPipeline{}; // destroying an in-use pipeline would be reported by validation
    REQUIRE(device.wait(*done).has_value());
}

TEST_CASE_FIXTURE(DeviceFixture, "Invalid pipeline descriptions are rejected") {
    const std::array<uint32_t, 4> not_spirv{1, 2, 3, 4};
    std::vector<rhi::GraphicsPipelineDesc> invalid;
    invalid.push_back(pipeline_desc(not_spirv));
    invalid.push_back(pipeline_desc(shaders::triangle_vert));
    invalid.back().fragment = {}; // color attachments without a fragment shader
    invalid.push_back(pipeline_desc(shaders::triangle_vert));
    invalid.back().depth.test = true; // no depth format
    static constexpr std::array k_depth_as_color{rhi::Format::D32Float};
    invalid.push_back(pipeline_desc(shaders::triangle_vert));
    invalid.back().color_formats = k_depth_as_color;
    for (const rhi::GraphicsPipelineDesc& desc : invalid) {
        auto pipeline = device.create_graphics_pipeline(desc);
        REQUIRE_FALSE(pipeline.has_value());
        CHECK(pipeline.error().code() == bytee::core::ErrorCode::InvalidArgument);
    }
}
