// Swapchains on a headless surface (GPU tests, no window needed). The fixture fails on any validation warning/error.

#include "support/error_text.hpp"
#include "support/gpu_fixture.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cstdint>
#include <vector>

namespace core = bytee::core;
namespace rhi = bytee::render::rhi;
using bytee::tests::DeviceFixture;
using rhi::ResourceState;

namespace {

rhi::Swapchain make_headless(rhi::Device& device, core::Extent2D extent, uint32_t frames_in_flight = 2) {
    auto swapchain = device.create_swapchain({.window = {.platform = rhi::NativeWindow::Platform::Headless},
                                              .extent = extent,
                                              .frames_in_flight = frames_in_flight,
                                              .debug_name = "headless swapchain"});
    REQUIRE_MESSAGE(swapchain.has_value(), bytee::tests::error_text(swapchain));
    return std::move(swapchain).value();
}

// Clears the acquired image and presents it.
void render_frame(rhi::Device& device, rhi::Swapchain& swapchain, rhi::SwapchainFrame frame, rhi::ClearColor clear) {
    auto commands = device.begin_commands();
    REQUIRE(commands.has_value());
    const rhi::TextureBarrier to_attachment{
        .texture = frame.texture, .before = ResourceState::Undefined, .after = ResourceState::ColorAttachment};
    commands->barrier({}, std::span{&to_attachment, 1});
    const rhi::ColorAttachment color{.texture = frame.texture, .clear = clear};
    commands->begin_render_pass({.colors = std::span{&color, 1}});
    commands->end_render_pass();
    const rhi::TextureBarrier to_present{
        .texture = frame.texture, .before = ResourceState::ColorAttachment, .after = ResourceState::Present};
    commands->barrier({}, std::span{&to_present, 1});
    auto presented = device.present(swapchain, std::move(*commands));
    REQUIRE_MESSAGE(presented.has_value(), bytee::tests::error_text(presented));
}

} // namespace

TEST_CASE_FIXTURE(DeviceFixture, "A headless swapchain presents many frames") {
    rhi::Swapchain swapchain = make_headless(device, {.width = 64, .height = 64});
    CHECK(swapchain.extent() == core::Extent2D{.width = 64, .height = 64});
    CHECK(swapchain.image_count() >= 2);
    CHECK(rhi::is_srgb_format(swapchain.format())); // default preference: an sRGB format

    for (int frame_number = 0; frame_number < 30; ++frame_number) {
        auto frame = swapchain.acquire();
        REQUIRE_MESSAGE(frame.has_value(), bytee::tests::error_text(frame));
        REQUIRE(frame->is_valid());
        const rhi::TextureInfo& info = device.info(frame->texture);
        CHECK(info.extent == core::Extent3D{.width = 64, .height = 64});
        CHECK(info.usage.has(rhi::TextureUsage::ColorAttachment));
        render_frame(device, swapchain, *frame, {0.0f, 0.5f, 1.0f, 1.0f});
    }
    REQUIRE(device.wait_idle().has_value());
}

TEST_CASE_FIXTURE(DeviceFixture, "Swapchain images can be read back before presenting") {
    rhi::Swapchain swapchain = make_headless(device, {.width = 16, .height = 16});
    auto frame = swapchain.acquire();
    REQUIRE(frame.has_value());
    if (!device.info(frame->texture).usage.has(rhi::TextureUsage::CopySrc)) {
        MESSAGE("skipped: this surface does not allow copies from its images");
        render_frame(device, swapchain, *frame, {});
        return;
    }
    rhi::Buffer readback =
        device
            .create_buffer(
                {.size = 16 * 16 * 4, .usage = rhi::BufferUsage::CopyDst, .memory = rhi::MemoryLocation::Readback})
            .value();
    auto commands = device.begin_commands();
    REQUIRE(commands.has_value());
    const rhi::TextureBarrier to_attachment{
        .texture = frame->texture, .before = ResourceState::Undefined, .after = ResourceState::ColorAttachment};
    commands->barrier({}, std::span{&to_attachment, 1});
    const rhi::ColorAttachment color{.texture = frame->texture, .clear = {1.0f, 0.0f, 0.0f, 1.0f}};
    commands->begin_render_pass({.colors = std::span{&color, 1}});
    commands->end_render_pass();
    const rhi::TextureBarrier to_copy{
        .texture = frame->texture, .before = ResourceState::ColorAttachment, .after = ResourceState::CopySrc};
    commands->barrier({}, std::span{&to_copy, 1});
    commands->copy_texture_to_buffer(frame->texture, {}, readback, 0);
    const rhi::TextureBarrier to_present{
        .texture = frame->texture, .before = ResourceState::CopySrc, .after = ResourceState::Present};
    commands->barrier({}, std::span{&to_present, 1});
    auto presented = device.present(swapchain, std::move(*commands));
    REQUIRE(presented.has_value());
    REQUIRE(device.wait(*presented).has_value());

    std::array<uint8_t, 4> first_pixel{};
    REQUIRE(device.read(readback, 0, std::as_writable_bytes(std::span{first_pixel})).has_value());
    // Red in BGRA or RGBA order: one channel at 255, the others at 0, alpha 255.
    const bool bgra = rhi::to_string(swapchain.format()).starts_with("BGRA");
    CHECK(first_pixel[bgra ? 2 : 0] == 255);
    CHECK(first_pixel[1] == 0);
    CHECK(first_pixel[3] == 255);
}

TEST_CASE_FIXTURE(DeviceFixture, "Resizing recreates the swapchain at the next acquire") {
    rhi::Swapchain swapchain = make_headless(device, {.width = 64, .height = 64});
    auto first = swapchain.acquire();
    REQUIRE(first.has_value());
    render_frame(device, swapchain, *first, {});

    swapchain.resize({.width = 32, .height = 24});
    auto resized = swapchain.acquire();
    REQUIRE_MESSAGE(resized.has_value(), bytee::tests::error_text(resized));
    CHECK(swapchain.extent() == core::Extent2D{.width = 32, .height = 24});
    CHECK(device.info(resized->texture).extent == core::Extent3D{.width = 32, .height = 24});
    render_frame(device, swapchain, *resized, {});
    REQUIRE(device.wait_idle().has_value());
}

TEST_CASE_FIXTURE(DeviceFixture, "A zero-sized (minimized) window produces frames to skip") {
    rhi::Swapchain swapchain = make_headless(device, {.width = 0, .height = 0});
    auto skipped = swapchain.acquire();
    REQUIRE(skipped.has_value());
    CHECK_FALSE(skipped->is_valid());

    swapchain.resize({.width = 20, .height = 20});
    auto frame = swapchain.acquire();
    REQUIRE_MESSAGE(frame.has_value(), bytee::tests::error_text(frame));
    REQUIRE(frame->is_valid());
    render_frame(device, swapchain, *frame, {});
    REQUIRE(device.wait_idle().has_value());
}

TEST_CASE_FIXTURE(DeviceFixture, "One frame in flight") {
    rhi::Swapchain swapchain = make_headless(device, {.width = 8, .height = 8}, 1);
    for (int frame_number = 0; frame_number < 5; ++frame_number) {
        auto frame = swapchain.acquire();
        REQUIRE(frame.has_value());
        render_frame(device, swapchain, *frame, {});
    }
    REQUIRE(device.wait_idle().has_value());
}

TEST_CASE_FIXTURE(DeviceFixture, "Invalid swapchain requests are rejected") {
    auto no_frames = device.create_swapchain({.extent = {.width = 8, .height = 8}, .frames_in_flight = 0});
    REQUIRE_FALSE(no_frames.has_value());
    CHECK(no_frames.error().code() == core::ErrorCode::InvalidArgument);

#if defined(__APPLE__)
    auto wrong_platform = device.create_swapchain(
        {.window = {.platform = rhi::NativeWindow::Platform::Xlib}, .extent = {.width = 8, .height = 8}});
    REQUIRE_FALSE(wrong_platform.has_value());
    CHECK(wrong_platform.error().code() == core::ErrorCode::NotSupported);
#endif
}

TEST_CASE_FIXTURE(DeviceFixture, "present() without a valid acquired frame is rejected") {
    rhi::Swapchain swapchain = make_headless(device, {.width = 0, .height = 0}); // minimized: frames are invalid
    auto frame = swapchain.acquire();
    REQUIRE(frame.has_value());
    REQUIRE_FALSE(frame->is_valid());

    auto presented = device.present(swapchain, device.begin_commands().value());
    REQUIRE_FALSE(presented.has_value());
    CHECK(presented.error().code() == core::ErrorCode::InvalidArgument);
}
