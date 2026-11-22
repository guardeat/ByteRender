// The RGB triangle in a window: swapchain, frames in flight, resizing; each frame is a render graph.
//   03_window [--frames N]   (exit after N frames, e.g. for automated runs)

#include "sample_window.hpp"
#include "triangle.vert.hpp"
#include "vertex_color.frag.hpp"

#include <bytee/core/log.hpp>
#include <bytee/render/graph/render_graph.hpp>
#include <bytee/render/rhi/instance_factory.hpp>

#include <array>
#include <charconv>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

namespace core = bytee::core;
namespace render = bytee::render;
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

// Draws the triangle into its target, keeping its shape whatever the target's aspect ratio.
class TrianglePass {
private:
    rhi::GraphicsPipeline pipeline_;
    render::TextureRef target_;

public:
    explicit TrianglePass(rhi::GraphicsPipeline pipeline) noexcept
        : pipeline_(std::move(pipeline)) {}

    void set_target(render::TextureRef target) noexcept { target_ = target; }

    void setup(render::PassBuilder& builder) {
        builder.color_attachment({.texture = target_, .clear = {0.02f, 0.02f, 0.03f, 1.0f}});
    }
    void execute(render::PassContext& context) {
        const core::Extent3D extent = context.desc(target_).extent;
        context.commands().bind_pipeline(pipeline_);
        context.commands().push_constants(static_cast<float>(extent.height) / static_cast<float>(extent.width));
        context.commands().draw(3);
    }
};

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
        .fragment = {.spirv = shaders::vertex_color_frag},
        .color_formats = color_formats,
        .debug_name = "triangle",
    });
    if (!pipeline) {
        logger.error("cannot create the pipeline: {}", pipeline.error().message());
        return 1;
    }
    TrianglePass triangle{std::move(*pipeline)};
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
        if (auto rendered = render_frame(*device, *swapchain, graph, triangle, *frame); !rendered) {
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
