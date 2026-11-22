// Compute pipelines and dispatch (GPU tests): results are read back and compared exactly. The fixture fails on any
// validation warning or error.

#include "fullscreen_uv.vert.hpp"
#include "gradient.comp.hpp"
#include "squares.comp.hpp"
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
using bytee::tests::Rgba8;
using bytee::tests::Rgba8Image;
using rhi::ResourceState;
using rhi::TextureUsage;

namespace {

constexpr uint32_t k_width = 37; // not a multiple of the 8x8 workgroup: partial groups must be covered and clipped
constexpr uint32_t k_height = 21;
constexpr uint32_t k_group_size = 8;

struct GradientPushConstants {
    uint32_t output_index = 0;
    uint32_t width = 0;
    uint32_t height = 0;
};

struct SquaresPushConstants {
    uint64_t values = 0;
    uint32_t count = 0;
};

struct SamplePushConstants {
    uint32_t texture_index = 0;
    uint32_t sampler_index = 0;
    float lod = 0.0f;
    uint32_t use_lod = 0;
};

rhi::ComputePipeline make_gradient_pipeline(rhi::Device& device) {
    return device.create_compute_pipeline({.shader = {.spirv = shaders::gradient_comp}, .debug_name = "gradient"})
        .value();
}

// Records the gradient into `texture` (in ShaderWrite afterwards).
void record_gradient(rhi::CommandList& commands, rhi::ComputePipelineHandle pipeline, rhi::TextureHandle texture) {
    const rhi::TextureBarrier to_write{
        .texture = texture, .before = ResourceState::Undefined, .after = ResourceState::ShaderWrite};
    commands.barrier({}, std::span{&to_write, 1});
    commands.bind_pipeline(pipeline);
    commands.push_constants(
        GradientPushConstants{.output_index = rhi::bindless_index(texture), .width = k_width, .height = k_height});
    commands.dispatch(rhi::dispatch_group_count(k_width, k_group_size),
                      rhi::dispatch_group_count(k_height, k_group_size));
}

Rgba8Image expected_gradient() {
    Rgba8Image expected{.width = k_width, .height = k_height};
    for (uint32_t y = 0; y < k_height; ++y) {
        for (uint32_t x = 0; x < k_width; ++x) {
            expected.pixels.push_back(Rgba8{static_cast<uint8_t>(x * 4), static_cast<uint8_t>(y * 4), 128, 255});
        }
    }
    return expected;
}

} // namespace

TEST_CASE("dispatch_group_count rounds up") {
    CHECK(rhi::dispatch_group_count(0, 8) == 0);
    CHECK(rhi::dispatch_group_count(1, 8) == 1);
    CHECK(rhi::dispatch_group_count(8, 8) == 1);
    CHECK(rhi::dispatch_group_count(9, 8) == 2);
    CHECK(rhi::dispatch_group_count(37, 8) == 5);
    CHECK(rhi::dispatch_group_count(5, 0) == 0);
}

TEST_CASE_FIXTURE(DeviceFixture, "A compute shader writes a storage texture") {
    rhi::Texture texture = device
                               .create_texture({.extent = {.width = k_width, .height = k_height},
                                                .format = rhi::Format::RGBA8Unorm,
                                                .usage = TextureUsage::Storage | TextureUsage::CopySrc,
                                                .debug_name = "gradient"})
                               .value();
    rhi::ComputePipeline pipeline = make_gradient_pipeline(device);

    auto commands = device.begin_commands();
    REQUIRE(commands.has_value());
    record_gradient(*commands, pipeline, texture);
    auto done = device.submit(std::move(*commands));
    REQUIRE_MESSAGE(done.has_value(), bytee::tests::error_text(done));

    const Rgba8Image image = bytee::tests::read_rgba8(device, texture, ResourceState::ShaderWrite);
    bytee::tests::check_image(image, expected_gradient(), 0, "compute_gradient");
}

TEST_CASE_FIXTURE(DeviceFixture, "A compute shader writes a buffer through its device address") {
    constexpr uint32_t k_count = 1000; // not a multiple of the 64-wide workgroup
    rhi::Buffer values = device
                             .create_buffer({.size = k_count * sizeof(uint32_t),
                                             .usage = rhi::BufferUsage::Storage,
                                             .memory = rhi::MemoryLocation::Readback,
                                             .debug_name = "squares"})
                             .value();
    rhi::ComputePipeline pipeline =
        device.create_compute_pipeline({.shader = {.spirv = shaders::squares_comp}, .debug_name = "squares"}).value();

    auto commands = device.begin_commands();
    REQUIRE(commands.has_value());
    commands->bind_pipeline(pipeline);
    commands->push_constants(SquaresPushConstants{.values = device.info(values).device_address, .count = k_count});
    commands->dispatch(rhi::dispatch_group_count(k_count, 64));
    const rhi::BufferBarrier to_host{
        .buffer = values, .before = ResourceState::ShaderWrite, .after = ResourceState::HostRead};
    commands->barrier(std::span{&to_host, 1}, {});
    auto done = device.submit(std::move(*commands));
    REQUIRE(done.has_value());
    REQUIRE(device.wait(*done).has_value());

    std::vector<uint32_t> result(k_count);
    REQUIRE(device.read(values, 0, std::as_writable_bytes(std::span{result})).has_value());
    for (uint32_t i = 0; i < k_count; ++i) {
        if (result[i] != i * i) {
            FAIL_CHECK("values[" << i << "] = " << result[i] << ", expected " << i * i);
            break;
        }
    }
}

TEST_CASE_FIXTURE(DeviceFixture, "Compute output is sampled by a later draw") {
    rhi::Texture texture = device
                               .create_texture({.extent = {.width = k_width, .height = k_height},
                                                .format = rhi::Format::RGBA8Unorm,
                                                .usage = TextureUsage::Storage | TextureUsage::Sampled,
                                                .debug_name = "compute output"})
                               .value();
    rhi::Texture target = device
                              .create_texture({.extent = {.width = k_width, .height = k_height},
                                               .format = rhi::Format::RGBA8Unorm,
                                               .usage = TextureUsage::ColorAttachment | TextureUsage::CopySrc,
                                               .debug_name = "target"})
                              .value();
    rhi::ComputePipeline gradient = make_gradient_pipeline(device);
    static constexpr std::array k_formats{rhi::Format::RGBA8Unorm};
    rhi::GraphicsPipeline textured = device
                                         .create_graphics_pipeline({.vertex = {.spirv = shaders::fullscreen_uv_vert},
                                                                    .fragment = {.spirv = shaders::textured_frag},
                                                                    .color_formats = k_formats,
                                                                    .debug_name = "textured"})
                                         .value();
    rhi::Sampler sampler = device
                               .create_sampler({.mag_filter = rhi::Filter::Nearest,
                                                .min_filter = rhi::Filter::Nearest,
                                                .mipmap_mode = rhi::MipmapMode::Nearest,
                                                .address_u = rhi::AddressMode::ClampToEdge,
                                                .address_v = rhi::AddressMode::ClampToEdge})
                               .value();

    auto commands = device.begin_commands();
    REQUIRE(commands.has_value());
    record_gradient(*commands, gradient, texture);
    const std::array barriers{
        rhi::TextureBarrier{
            .texture = texture, .before = ResourceState::ShaderWrite, .after = ResourceState::ShaderRead},
        rhi::TextureBarrier{
            .texture = target, .before = ResourceState::Undefined, .after = ResourceState::ColorAttachment},
    };
    commands->barrier({}, barriers);
    const rhi::ColorAttachment color{.texture = target};
    commands->begin_render_pass({.colors = std::span{&color, 1}});
    commands->bind_pipeline(textured);
    commands->push_constants(SamplePushConstants{.texture_index = rhi::bindless_index(texture),
                                                 .sampler_index = rhi::bindless_index(sampler)});
    commands->draw(3);
    commands->end_render_pass();
    auto done = device.submit(std::move(*commands));
    REQUIRE_MESSAGE(done.has_value(), bytee::tests::error_text(done));

    const Rgba8Image image = bytee::tests::read_rgba8(device, target, ResourceState::ColorAttachment);
    bytee::tests::check_image(image, expected_gradient(), 0, "compute_then_draw"); // same size: one texel per pixel
}

TEST_CASE_FIXTURE(DeviceFixture,
                  "A compute pipeline released right after its submission stays alive until the GPU is done") {
    auto pipeline = device.create_compute_pipeline({.shader = {.spirv = shaders::squares_comp}});
    REQUIRE(pipeline.has_value());
    rhi::Buffer values =
        device.create_buffer({.size = 64 * sizeof(uint32_t), .usage = rhi::BufferUsage::Storage}).value();
    auto commands = device.begin_commands();
    REQUIRE(commands.has_value());
    commands->bind_pipeline(*pipeline);
    commands->push_constants(SquaresPushConstants{.values = device.info(values).device_address, .count = 64});
    commands->dispatch(1);
    auto done = device.submit(std::move(*commands));
    REQUIRE(done.has_value());
    *pipeline = rhi::ComputePipeline{}; // destroying an in-use pipeline would be reported by validation
    REQUIRE(device.wait(*done).has_value());
}

TEST_CASE_FIXTURE(DeviceFixture, "Invalid compute pipeline descriptions are rejected") {
    const std::array<uint32_t, 4> not_spirv{1, 2, 3, 4};
    for (const rhi::ComputePipelineDesc& desc :
         {rhi::ComputePipelineDesc{}, rhi::ComputePipelineDesc{.shader = {.spirv = not_spirv}}}) {
        auto pipeline = device.create_compute_pipeline(desc);
        REQUIRE_FALSE(pipeline.has_value());
        CHECK(pipeline.error().code() == core::ErrorCode::InvalidArgument);
    }
}

TEST_CASE_FIXTURE(bytee::tests::GpuFixture, "The compute pipeline capacity is enforced") {
    auto instance = create_instance();
    REQUIRE(instance.has_value());
    auto device = instance->create_device({.capacities = {.max_compute_pipelines = 1}});
    REQUIRE_MESSAGE(device.has_value(), bytee::tests::error_text(device));
    auto first = device->create_compute_pipeline({.shader = {.spirv = shaders::squares_comp}});
    REQUIRE(first.has_value());
    auto second = device->create_compute_pipeline({.shader = {.spirv = shaders::squares_comp}});
    REQUIRE_FALSE(second.has_value());
    CHECK(second.error().code() == core::ErrorCode::LimitExceeded);
}
