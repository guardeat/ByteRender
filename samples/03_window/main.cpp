// The RGB triangle in a window: swapchain, frames in flight, resizing.
//   03_window [--frames N]   (exit after N frames, e.g. for automated runs)

#include "sample_window.hpp"
#include "triangle.frag.hpp"
#include "triangle.vert.hpp"

#include <bytee/core/log.hpp>
#include <bytee/render/rhi/instance_factory.hpp>

#include <array>
#include <charconv>
#include <cstdint>
#include <optional>
#include <string_view>

namespace core = bytee::core;
namespace rhi = bytee::render::rhi;

namespace {

std::optional<uint64_t> frame_limit(int argc, char** argv) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::string_view{argv[i]} == "--frames") {
            uint64_t value = 0;
            const std::string_view text{argv[i + 1]};
            if (std::from_chars(text.data(), text.data() + text.size(), value).ec == std::errc{}) {
                return value;
            }
        }
    }
    return std::nullopt;
}

core::Result<void> record_frame(rhi::Device& device, rhi::Swapchain& swapchain, rhi::GraphicsPipelineHandle pipeline,
                                rhi::SwapchainFrame frame) {
    core::Result<rhi::CommandList> commands = device.begin_commands();
    if (!commands) {
        return std::move(commands).error();
    }
    const rhi::TextureBarrier to_attachment{.texture = frame.texture,
                                            .before = rhi::ResourceState::Undefined,
                                            .after = rhi::ResourceState::ColorAttachment};
    commands->barrier({}, std::span{&to_attachment, 1});
    const rhi::ColorAttachment color{.texture = frame.texture, .clear = {0.02f, 0.02f, 0.03f, 1.0f}};
    commands->begin_render_pass({.colors = std::span{&color, 1}});
    commands->bind_pipeline(pipeline);
    const core::Extent2D extent = swapchain.extent();
    commands->push_constants(static_cast<float>(extent.height) / static_cast<float>(extent.width)); // keep the shape
    commands->draw(3);
    commands->end_render_pass();
    const rhi::TextureBarrier to_present{
        .texture = frame.texture, .before = rhi::ResourceState::ColorAttachment, .after = rhi::ResourceState::Present};
    commands->barrier({}, std::span{&to_present, 1});
    if (auto presented = device.present(swapchain, std::move(*commands)); !presented) {
        return std::move(presented).error();
    }
    return {};
}

} // namespace

int main(int argc, char** argv) {
    core::ConsoleLogSink sink;
    core::Logger logger{sink, core::LogLevel::Info};
    const std::optional<uint64_t> max_frames = frame_limit(argc, argv);

    core::Result<bytee::samples::SampleWindow> window =
        bytee::samples::create_sample_window("Bytee — 03_window", {.width = 1280, .height = 720});
    if (!window) {
        logger.error("{}", window.error().message());
        return 1;
    }
    const rhi::InstanceFactory factory{logger};
    core::Result<rhi::Instance> instance =
        factory.create_instance({.app_name = "03_window", .enable_validation = true});
    if (!instance) {
        logger.error("cannot create an instance: {}", instance.error().message());
        return 1;
    }
    core::Result<rhi::Device> device = instance->create_device({.debug_name = "Window Device"});
    if (!device) {
        logger.error("cannot create a device: {}", device.error().message());
        return 1;
    }
    core::Result<rhi::Swapchain> swapchain =
        device->create_swapchain({.window = window->native(), .extent = window->framebuffer_extent()});
    if (!swapchain) {
        logger.error("cannot create a swapchain: {}", swapchain.error().message());
        return 1;
    }
    const std::array color_formats{swapchain->format()};
    core::Result<rhi::GraphicsPipeline> pipeline = device->create_graphics_pipeline({
        .vertex = {.spirv = shaders::triangle_vert},
        .fragment = {.spirv = shaders::triangle_frag},
        .color_formats = color_formats,
        .debug_name = "triangle",
    });
    if (!pipeline) {
        logger.error("cannot create the pipeline: {}", pipeline.error().message());
        return 1;
    }
    logger.info("rendering on {} at {}x{} ({})", device->adapter().name, swapchain->extent().width,
                swapchain->extent().height, rhi::to_string(swapchain->format()));

    uint64_t frames = 0;
    while (!window->should_close() && (!max_frames || frames < *max_frames)) {
        window->poll_events();
        swapchain->resize(window->framebuffer_extent()); // no-op unless the size changed
        core::Result<rhi::SwapchainFrame> frame = swapchain->acquire();
        if (!frame) {
            logger.error("acquire failed: {}", frame.error().message());
            return 1;
        }
        if (!frame->is_valid()) {
            continue; // minimized
        }
        if (auto recorded = record_frame(*device, *swapchain, *pipeline, *frame); !recorded) {
            logger.error("frame failed: {}", recorded.error().message());
            return 1;
        }
        ++frames;
    }
    if (auto idle = device->wait_idle(); !idle) {
        logger.error("{}", idle.error().message());
        return 1;
    }
    logger.info("presented {} frames", frames);
    return 0;
}
