// Headless rendering (GPU tests): render passes verified by reading the results back. The fixture fails on any
// validation warning or error.

#include "support/gpu_fixture.hpp"
#include "support/image.hpp"

#include <doctest/doctest.h>

#include <array>
#include <bit>
#include <cstdint>
#include <cstring>

namespace rhi = bytee::render::rhi;
using bytee::tests::DeviceFixture;
using bytee::tests::Rgba8;
using rhi::ResourceState;
using rhi::TextureUsage;

namespace {

constexpr uint32_t k_size = 64;

rhi::Texture make_target(rhi::Device& device, std::string_view name) {
    return device
        .create_texture({.extent = {.width = k_size, .height = k_size},
                         .format = rhi::Format::RGBA8Unorm,
                         .usage = TextureUsage::ColorAttachment | TextureUsage::TransferSrc,
                         .debug_name = name})
        .value();
}

void submit_and_wait(rhi::Device& device, rhi::CommandList&& commands) {
    auto done = device.submit(std::move(commands));
    REQUIRE_MESSAGE(done.has_value(), done.error().message());
    REQUIRE(device.wait(*done).has_value());
}

} // namespace

TEST_CASE_FIXTURE(DeviceFixture, "Clearing a render target") {
    rhi::Texture target = make_target(device, "clear target");
    auto commands = device.begin_commands();
    REQUIRE(commands.has_value());

    const rhi::TextureBarrier to_attachment{
        .texture = target, .before = ResourceState::Undefined, .after = ResourceState::ColorAttachment};
    commands->barrier({}, std::span{&to_attachment, 1});
    // 0.2 / 0.4 / 0.6 are exact in 8-bit unorm (51, 102, 153): the comparison can be exact.
    const rhi::ColorAttachment color{.texture = target, .clear = {0.2f, 0.4f, 0.6f, 1.0f}};
    commands->begin_render_pass({.colors = std::span{&color, 1}});
    commands->end_render_pass();
    submit_and_wait(device, std::move(*commands));

    bytee::tests::check_image(bytee::tests::read_rgba8(device, target, ResourceState::ColorAttachment),
                              bytee::tests::solid_image(k_size, k_size, Rgba8{51, 102, 153, 255}), 0, "clear");
}

TEST_CASE_FIXTURE(DeviceFixture, "Several color attachments are cleared independently") {
    rhi::Texture first = make_target(device, "first");
    rhi::Texture second = make_target(device, "second");
    auto commands = device.begin_commands();
    REQUIRE(commands.has_value());

    const std::array to_attachment{
        rhi::TextureBarrier{
            .texture = first, .before = ResourceState::Undefined, .after = ResourceState::ColorAttachment},
        rhi::TextureBarrier{
            .texture = second, .before = ResourceState::Undefined, .after = ResourceState::ColorAttachment},
    };
    commands->barrier({}, to_attachment);
    const std::array colors{
        rhi::ColorAttachment{.texture = first, .clear = {1.0f, 0.0f, 0.0f, 1.0f}},
        rhi::ColorAttachment{.texture = second, .clear = {0.0f, 0.0f, 1.0f, 0.2f}},
    };
    commands->begin_render_pass({.colors = colors});
    commands->end_render_pass();
    submit_and_wait(device, std::move(*commands));

    bytee::tests::check_image(bytee::tests::read_rgba8(device, first, ResourceState::ColorAttachment),
                              bytee::tests::solid_image(k_size, k_size, Rgba8{255, 0, 0, 255}), 0, "mrt_first");
    bytee::tests::check_image(bytee::tests::read_rgba8(device, second, ResourceState::ColorAttachment),
                              bytee::tests::solid_image(k_size, k_size, Rgba8{0, 0, 255, 51}), 0, "mrt_second");
}

TEST_CASE_FIXTURE(DeviceFixture, "LoadOp::Load keeps previous contents") {
    rhi::Texture target = make_target(device, "load target");
    auto commands = device.begin_commands();
    REQUIRE(commands.has_value());
    const rhi::TextureBarrier to_attachment{
        .texture = target, .before = ResourceState::Undefined, .after = ResourceState::ColorAttachment};
    commands->barrier({}, std::span{&to_attachment, 1});

    const rhi::ColorAttachment clear{.texture = target, .clear = {0.0f, 1.0f, 0.0f, 1.0f}};
    commands->begin_render_pass({.colors = std::span{&clear, 1}});
    commands->end_render_pass();
    const rhi::TextureBarrier between{
        .texture = target, .before = ResourceState::ColorAttachment, .after = ResourceState::ColorAttachment};
    commands->barrier({}, std::span{&between, 1});
    const rhi::ColorAttachment load{.texture = target, .load = rhi::LoadOp::Load};
    commands->begin_render_pass({.colors = std::span{&load, 1}});
    commands->end_render_pass();
    submit_and_wait(device, std::move(*commands));

    bytee::tests::check_image(bytee::tests::read_rgba8(device, target, ResourceState::ColorAttachment),
                              bytee::tests::solid_image(k_size, k_size, Rgba8{0, 255, 0, 255}), 0, "load");
}

TEST_CASE_FIXTURE(DeviceFixture, "Depth is cleared to the requested value") {
    auto depth = device.create_texture({.extent = {.width = 16, .height = 16},
                                        .format = rhi::Format::D32Float,
                                        .usage = TextureUsage::DepthStencilAttachment | TextureUsage::TransferSrc,
                                        .debug_name = "depth"});
    REQUIRE_MESSAGE(depth.has_value(), depth.error().message());
    auto commands = device.begin_commands();
    REQUIRE(commands.has_value());
    const rhi::TextureBarrier to_depth{
        .texture = *depth, .before = ResourceState::Undefined, .after = ResourceState::DepthWrite};
    commands->barrier({}, std::span{&to_depth, 1});
    commands->begin_render_pass({.depth = rhi::DepthAttachment{.texture = *depth, .clear_depth = 0.25f}});
    commands->end_render_pass();
    submit_and_wait(device, std::move(*commands));

    std::array<float, 16 * 16> values{};
    auto downloaded = device.download(*depth, std::as_writable_bytes(std::span{values}),
                                      {.before = ResourceState::DepthWrite, .after = ResourceState::DepthWrite});
    REQUIRE_MESSAGE(downloaded.has_value(), downloaded.error().message());
    for (const float value : values) {
        CHECK(value == 0.25f);
    }
}

TEST_CASE_FIXTURE(DeviceFixture, "Download validates sizes and regions") {
    rhi::Texture target = make_target(device, "download target");
    std::array<std::byte, 16> too_small{};
    auto wrong_size = device.download(target, too_small, {.before = ResourceState::Undefined});
    REQUIRE_FALSE(wrong_size.has_value());
    CHECK(wrong_size.error().code() == bytee::core::ErrorCode::InvalidArgument);

    auto outside = device.download(target, too_small, {.region = {.mip_level = 4}, .before = ResourceState::Undefined});
    REQUIRE_FALSE(outside.has_value());
    CHECK(outside.error().code() == bytee::core::ErrorCode::InvalidArgument);
}
