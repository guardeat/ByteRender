// Debug UI with Dear ImGui, rendered by ByteRender: the triangle plus a window with frame statistics, a background
// color picker and the render graph of the previous frame (passes, culling, Graphviz dump to the clipboard).
//   04_imgui [--frames N]   (exit after N frames, e.g. for automated runs)

#include "frame_limit.hpp"
#include "imgui_pass.hpp"
#include "sample_window.hpp"
#include "triangle_pass.hpp"

#include <bytee/core/log.hpp>
#include <bytee/render/graph/render_graph.hpp>
#include <bytee/render/rhi/instance_factory.hpp>

#include <cstdint>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <optional>
#include <string>
#include <utility>

namespace core = bytee::core;
namespace render = bytee::render;
namespace rhi = bytee::render::rhi;
using bytee::samples::ImGuiPass;
using bytee::samples::TrianglePass;

namespace {

constexpr uint32_t k_frames_in_flight = 2;

// ImGui context and GLFW input backend for the lifetime of the sample.
class ImGuiSession {
public:
    explicit ImGuiSession(const bytee::samples::SampleWindow& window) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::GetIO().IniFilename = nullptr; // no imgui.ini next to the executable
        ImGui_ImplGlfw_InitForOther(window.glfw_window(), true);
    }
    ImGuiSession(const ImGuiSession&) = delete;
    ImGuiSession& operator=(const ImGuiSession&) = delete;
    ~ImGuiSession() {
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
};

struct UiState {
    rhi::ClearColor background{0.02f, 0.02f, 0.03f, 1.0f};
    bool show_demo = false;
};

// The debug window. `graph` still describes the previous frame (it is rebuilt after the UI).
void build_ui(UiState& state, const rhi::Device& device, const rhi::Swapchain& swapchain,
              const render::RenderGraph& graph) {
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos({16, 16}, ImGuiCond_FirstUseEver);
    ImGui::Begin("ByteRender");
    ImGui::Text("%s", device.adapter().name.c_str());
    ImGui::Text("%ux%u %s", swapchain.extent().width, swapchain.extent().height,
                std::string{rhi::to_string(swapchain.format())}.c_str());
    ImGui::Text("%.2f ms (%.0f FPS)", static_cast<double>(1000.0f / io.Framerate), static_cast<double>(io.Framerate));
    ImGui::ColorEdit3("Background", &state.background.r);

    ImGui::SeparatorText("Render graph (previous frame)");
    for (uint32_t i = 0; i < graph.pass_count(); ++i) {
        const render::PassView pass = graph.pass(i);
        ImGui::BulletText("%s%s (%zu barriers)", std::string{pass.name}.c_str(), pass.culled ? " - culled" : "",
                          pass.barriers.size());
    }
    ImGui::Text("Pooled transients: %u textures, %u buffers", graph.pooled_texture_count(),
                graph.pooled_buffer_count());
    if (ImGui::Button("Copy Graphviz dump")) {
        ImGui::SetClipboardText(graph.to_graphviz().c_str());
    }
    ImGui::Checkbox("ImGui demo window", &state.show_demo);
    ImGui::End();
    if (state.show_demo) {
        ImGui::ShowDemoWindow(&state.show_demo);
    }
}

core::Result<void> render_frame(rhi::Device& device, rhi::Swapchain& swapchain, render::RenderGraph& graph,
                                TrianglePass& triangle, ImGuiPass& imgui, rhi::SwapchainFrame frame) {
    graph.reset();
    const render::TextureRef backbuffer = graph.import_texture(
        "backbuffer", frame.texture, {.before = rhi::ResourceState::Undefined, .after = rhi::ResourceState::Present});
    triangle.set_target(backbuffer);
    graph.add_pass("Triangle", triangle);
    imgui.set_target(backbuffer);
    graph.add_pass("ImGui", imgui);
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
        bytee::samples::create_sample_window("ByteRender — 04_imgui", {.width = 1280, .height = 720});
    if (!window) {
        logger.error("{}", window.error().message());
        return 1;
    }
    const rhi::InstanceFactory factory{logger};
    core::Result<rhi::Instance> instance = factory.create_instance({.app_name = "04_imgui", .enable_validation = true});
    if (!instance) {
        logger.error("cannot create an instance: {}", instance.error().message());
        return 1;
    }
    core::Result<rhi::Device> device = instance->create_device({.debug_name = "ImGui Device"});
    if (!device) {
        logger.error("cannot create a device: {}", device.error().message());
        return 1;
    }
    core::Result<rhi::Swapchain> swapchain = device->create_swapchain(
        {.window = window->native(), .extent = window->framebuffer_extent(), .frames_in_flight = k_frames_in_flight});
    if (!swapchain) {
        logger.error("cannot create a swapchain: {}", swapchain.error().message());
        return 1;
    }

    const ImGuiSession imgui_session{*window};
    core::Result<TrianglePass> triangle = bytee::samples::create_triangle_pass(*device, swapchain->format());
    core::Result<ImGuiPass> imgui = bytee::samples::create_imgui_pass(*device, swapchain->format(), k_frames_in_flight);
    if (!triangle || !imgui) {
        logger.error("cannot create the passes: {}", (!triangle ? triangle.error() : imgui.error()).message());
        return 1;
    }
    render::RenderGraph graph{*device};
    UiState ui;
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
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        build_ui(ui, *device, *swapchain, graph);
        ImGui::Render();
        triangle->set_clear_color(ui.background);
        if (auto rendered = render_frame(*device, *swapchain, graph, *triangle, *imgui, *frame); !rendered) {
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
