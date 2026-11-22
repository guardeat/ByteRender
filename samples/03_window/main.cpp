// The RGB triangle in a window: swapchain, frames in flight, resizing; each frame is a render graph.
//   03_window [--frames N]   (exit after N frames, e.g. for automated runs)

#include "frame_limit.hpp"
#include "sample_window.hpp"
#include "triangle_pass.hpp"

#include <bytee/core/log.hpp>
#include <bytee/render/graph/render_graph.hpp>
#include <bytee/render/rhi/instance_factory.hpp>

#include <cstdint>
#include <optional>
#include <utility>

namespace core = bytee::core;
namespace render = bytee::render;
namespace rhi = bytee::render::rhi;
using bytee::samples::TrianglePass;

namespace {

core::Result<void> render_frame(rhi::Device& device, rhi::Swapchain& swapchain, render::RenderGraph& graph,
                                TrianglePass& triangle, rhi::SwapchainFrame frame) {
    graph.reset();
    triangle.set_target(graph.import_texture(
        "backbuffer", frame.texture, {.before = rhi::ResourceState::Undefined, .after = rhi::ResourceState::Present}));
    graph.add_pass("Triangle", triangle);
    core::Result<rhi::CommandList> commands = graph.record();
    if (!commands) {
        return std::move(commands).error();
    }
    BYTEE_TRY(device.present(swapchain, std::move(*commands)));
    return {};
}

} // namespace

int main(int argc, char** argv) {
    core::ConsoleLogSink sink;
    core::Logger logger{sink, core::LogLevel::Info};
    const std::optional<uint64_t> max_frames = bytee::samples::frame_limit(argc, argv);

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
    core::Result<TrianglePass> triangle = bytee::samples::create_triangle_pass(*device, swapchain->format());
    if (!triangle) {
        logger.error("cannot create the triangle pass: {}", triangle.error().message());
        return 1;
    }
    render::RenderGraph graph{*device};
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
            window->wait_events(); // minimized: sleep until something happens instead of spinning
            continue;
        }
        if (auto rendered = render_frame(*device, *swapchain, graph, *triangle, *frame); !rendered) {
            logger.error("frame failed: {}", rendered.error().message());
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
