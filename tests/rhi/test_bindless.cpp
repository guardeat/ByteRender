// Bindless textures and samplers (GPU tests), verified on rendered images. The fixture fails on any validation
// warning or error.

#include "fullscreen_uv.vert.hpp"
#include "support/error_text.hpp"
#include "support/gpu_fixture.hpp"
#include "support/image.hpp"
#include "textured.frag.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cstdint>
#include <vector>

namespace core = bytee::core;
namespace rhi = bytee::render::rhi;
using bytee::tests::DeviceFixture;
using bytee::tests::GpuFixture;
using bytee::tests::Rgba8;
using bytee::tests::Rgba8Image;
using rhi::ResourceState;
using rhi::TextureUsage;

namespace {

constexpr uint32_t k_size = 64;
constexpr rhi::Format k_format = rhi::Format::RGBA8Unorm;

struct SamplePushConstants {
    uint32_t texture_index = 0;
    uint32_t sampler_index = 0;
    float lod = 0.0f;
    uint32_t use_lod = 0;
};

rhi::GraphicsPipeline make_pipeline(rhi::Device& device) {
    static constexpr std::array k_formats{k_format};
    return device
        .create_graphics_pipeline({.vertex = {.spirv = shaders::fullscreen_uv_vert},
                                   .fragment = {.spirv = shaders::textured_frag},
                                   .color_formats = k_formats,
                                   .debug_name = "textured"})
        .value();
}

// A texture filled with `pixels` (width x height, mip 0) or several mips, ready for sampling (ShaderRead).
rhi::Texture make_texture(rhi::Device& device, uint32_t width, uint32_t height,
                          const std::vector<std::vector<Rgba8>>& mips) {
    rhi::Texture texture = device
                               .create_texture({.extent = {.width = width, .height = height},
                                                .mip_levels = static_cast<uint32_t>(mips.size()),
                                                .format = k_format,
                                                .usage = TextureUsage::Sampled | TextureUsage::TransferDst,
                                                .debug_name = "sampled"})
                               .value();
    for (uint32_t mip = 0; mip < mips.size(); ++mip) {
        auto uploaded = device.upload(texture, std::as_bytes(std::span{mips[mip]}),
                                      {.region = {.mip_level = mip}, .after = ResourceState::ShaderRead});
        REQUIRE_MESSAGE(uploaded.has_value(), bytee::tests::error_text(uploaded));
    }
    return texture;
}

template <class Record_>
Rgba8Image render(rhi::Device& device, Record_&& record) {
    rhi::Texture target = device
                              .create_texture({.extent = {.width = k_size, .height = k_size},
                                               .format = k_format,
                                               .usage = TextureUsage::ColorAttachment | TextureUsage::TransferSrc})
                              .value();
    auto commands = device.begin_commands();
    REQUIRE(commands.has_value());
    const rhi::TextureBarrier to_attachment{
        .texture = target, .before = ResourceState::Undefined, .after = ResourceState::ColorAttachment};
    commands->barrier({}, std::span{&to_attachment, 1});
    const rhi::ColorAttachment color{.texture = target};
    commands->begin_render_pass({.colors = std::span{&color, 1}});
    record(*commands);
    commands->end_render_pass();
    auto done = device.submit(std::move(*commands));
    REQUIRE_MESSAGE(done.has_value(), bytee::tests::error_text(done));
    return bytee::tests::read_rgba8(device, target, ResourceState::ColorAttachment);
}

void draw_sampled(rhi::CommandList& commands, rhi::GraphicsPipelineHandle pipeline, SamplePushConstants constants) {
    commands.bind_pipeline(pipeline);
    commands.push_constants(constants);
    commands.draw(3);
}

rhi::Sampler nearest_clamp(rhi::Device& device) {
    return device
        .create_sampler({.mag_filter = rhi::Filter::Nearest,
                         .min_filter = rhi::Filter::Nearest,
                         .mipmap_mode = rhi::MipmapMode::Nearest,
                         .address_u = rhi::AddressMode::ClampToEdge,
                         .address_v = rhi::AddressMode::ClampToEdge,
                         .debug_name = "nearest clamp"})
        .value();
}

} // namespace

TEST_CASE_FIXTURE(DeviceFixture, "Textured quad: texels land where uv says, uv (0, 0) is the top-left") {
    std::vector<Rgba8> texels;
    for (uint32_t y = 0; y < 4; ++y) {
        for (uint32_t x = 0; x < 4; ++x) {
            texels.push_back(Rgba8{static_cast<uint8_t>(x * 64), static_cast<uint8_t>(y * 64), 128, 255});
        }
    }
    rhi::Texture texture = make_texture(device, 4, 4, {texels});
    rhi::Sampler sampler = nearest_clamp(device);
    rhi::GraphicsPipeline pipeline = make_pipeline(device);

    const Rgba8Image image = render(device, [&](rhi::CommandList& commands) {
        draw_sampled(commands, pipeline,
                     {.texture_index = rhi::bindless_index(texture), .sampler_index = rhi::bindless_index(sampler)});
    });
    Rgba8Image expected{.width = k_size, .height = k_size};
    for (uint32_t y = 0; y < k_size; ++y) {
        for (uint32_t x = 0; x < k_size; ++x) {
            expected.pixels.push_back(texels[(y / 16) * 4 + x / 16]); // each texel covers a 16x16 block
        }
    }
    bytee::tests::check_image(image, expected, 0, "textured_quad");
}

TEST_CASE_FIXTURE(DeviceFixture, "Each draw picks its texture by index") {
    rhi::Texture red = make_texture(device, 1, 1, {{Rgba8{255, 0, 0, 255}}});
    rhi::Texture blue = make_texture(device, 1, 1, {{Rgba8{0, 0, 255, 255}}});
    rhi::Sampler sampler = nearest_clamp(device);
    rhi::GraphicsPipeline pipeline = make_pipeline(device);

    const Rgba8Image image = render(device, [&](rhi::CommandList& commands) {
        commands.set_scissor({.extent = {.width = k_size / 2, .height = k_size}});
        draw_sampled(commands, pipeline,
                     {.texture_index = rhi::bindless_index(red), .sampler_index = rhi::bindless_index(sampler)});
        commands.set_scissor(
            {.offset = {.x = static_cast<int32_t>(k_size / 2)}, .extent = {.width = k_size / 2, .height = k_size}});
        draw_sampled(commands, pipeline,
                     {.texture_index = rhi::bindless_index(blue), .sampler_index = rhi::bindless_index(sampler)});
    });
    CHECK(image.at(5, 30) == Rgba8{255, 0, 0, 255});
    CHECK(image.at(58, 30) == Rgba8{0, 0, 255, 255});
}

TEST_CASE_FIXTURE(DeviceFixture, "Linear filtering blends neighbouring texels") {
    rhi::Texture texture = make_texture(device, 2, 1, {{Rgba8{0, 0, 0, 255}, Rgba8{255, 255, 255, 255}}});
    rhi::Sampler linear = device
                              .create_sampler({.address_u = rhi::AddressMode::ClampToEdge,
                                               .address_v = rhi::AddressMode::ClampToEdge,
                                               .debug_name = "linear"})
                              .value();
    rhi::GraphicsPipeline pipeline = make_pipeline(device);
    const Rgba8Image image = render(device, [&](rhi::CommandList& commands) {
        draw_sampled(commands, pipeline,
                     {.texture_index = rhi::bindless_index(texture), .sampler_index = rhi::bindless_index(linear)});
    });
    CHECK(image.at(0, 10).r <= 8);    // left edge: the black texel
    CHECK(image.at(63, 10).r >= 247); // right edge: the white texel
    const uint8_t middle = image.at(32, 10).r;
    CHECK(middle > 100);
    CHECK(middle < 160); // blended, not nearest
}

TEST_CASE_FIXTURE(DeviceFixture, "Explicit LOD samples the requested mip level") {
    rhi::Texture texture =
        make_texture(device, 2, 2, {std::vector<Rgba8>(4, Rgba8{255, 0, 0, 255}), {Rgba8{0, 255, 0, 255}}});
    rhi::Sampler sampler = nearest_clamp(device);
    rhi::GraphicsPipeline pipeline = make_pipeline(device);
    const Rgba8Image image = render(device, [&](rhi::CommandList& commands) {
        draw_sampled(commands, pipeline,
                     {.texture_index = rhi::bindless_index(texture),
                      .sampler_index = rhi::bindless_index(sampler),
                      .lod = 1.0f,
                      .use_lod = 1});
    });
    bytee::tests::check_image(image, bytee::tests::solid_image(k_size, k_size, Rgba8{0, 255, 0, 255}), 0, "lod");
}

TEST_CASE_FIXTURE(DeviceFixture, "A freed slot is reused by the next texture, with the new contents") {
    uint32_t old_index = 0;
    {
        rhi::Texture red = make_texture(device, 1, 1, {{Rgba8{255, 0, 0, 255}}});
        old_index = rhi::bindless_index(red);
        REQUIRE(device.wait_idle().has_value()); // the upload finished: releasing destroys immediately
    }
    rhi::Texture green = make_texture(device, 1, 1, {{Rgba8{0, 255, 0, 255}}});
    CHECK(rhi::bindless_index(green) == old_index); // LIFO slot reuse
    rhi::Sampler sampler = nearest_clamp(device);
    rhi::GraphicsPipeline pipeline = make_pipeline(device);
    const Rgba8Image image = render(device, [&](rhi::CommandList& commands) {
        draw_sampled(commands, pipeline,
                     {.texture_index = rhi::bindless_index(green), .sampler_index = rhi::bindless_index(sampler)});
    });
    CHECK(image.at(32, 32) == Rgba8{0, 255, 0, 255});
}

TEST_CASE_FIXTURE(GpuFixture, "Bindless capacity adapts to the GPU and is enforced") {
    auto instance = create_instance();
    REQUIRE(instance.has_value());
    auto huge = instance->create_device({.limits = {.max_textures = 1u << 30, .max_samplers = 1u << 30}});
    REQUIRE_MESSAGE(huge.has_value(), bytee::tests::error_text(huge));
    CHECK(huge->limits().max_textures >= 64);
    CHECK(huge->limits().max_textures < (1u << 30));
    CHECK(huge->limits().max_samplers >= 16);

    auto small = instance->create_device({.limits = {.max_textures = 2}});
    REQUIRE(small.has_value());
    CHECK(small->limits().max_textures == 2);
    const rhi::TextureDesc desc{
        .extent = {.width = 1, .height = 1}, .format = k_format, .usage = TextureUsage::Sampled};
    auto first = small->create_texture(desc);
    auto second = small->create_texture(desc);
    REQUIRE(first.has_value());
    REQUIRE(second.has_value());
    auto third = small->create_texture(desc);
    REQUIRE_FALSE(third.has_value());
    CHECK(third.error().code() == core::ErrorCode::LimitExceeded);
}

TEST_CASE_FIXTURE(DeviceFixture, "Sampler descriptions are validated") {
    CHECK(device.create_sampler({.max_anisotropy = 16.0f, .debug_name = "anisotropic"}).has_value());
    CHECK(device.create_sampler({.compare = rhi::CompareOp::Greater, .debug_name = "shadow"}).has_value());
    for (const rhi::SamplerDesc& desc :
         {rhi::SamplerDesc{.max_anisotropy = 0.5f}, rhi::SamplerDesc{.max_anisotropy = 1000.0f},
          rhi::SamplerDesc{.min_lod = 4.0f, .max_lod = 1.0f}}) {
        auto sampler = device.create_sampler(desc);
        REQUIRE_FALSE(sampler.has_value());
        CHECK(sampler.error().code() == core::ErrorCode::InvalidArgument);
    }
}

TEST_CASE_FIXTURE(GpuFixture, "Regression: a full texture pool frees every view of the rejected texture") {
    auto instance = create_instance();
    REQUIRE(instance.has_value());
    auto device = instance->create_device({.limits = {.max_textures = 1}});
    REQUIRE(device.has_value());
    const rhi::TextureDesc desc{.extent = {.width = 4, .height = 4},
                                .format = k_format,
                                .usage = TextureUsage::ColorAttachment | TextureUsage::Sampled};
    auto kept = device->create_texture(desc);
    REQUIRE(kept.has_value());
    auto rejected = device->create_texture(desc);
    REQUIRE_FALSE(rejected.has_value());
    CHECK(rejected.error().code() == core::ErrorCode::LimitExceeded);
    // Device destruction now: the validation layer reports any leaked image view (the fixture fails on it).
}
