// A small first-person scene: walk with WASD, look with the mouse, jump with Space, sprint with Shift and throw
// spheres with E (hold to keep throwing). Esc releases the mouse, a click captures it again.
//   05_fps [--frames N]   (exit after N frames; automated runs also throw spheres and turn the camera)

#include "fps_camera.hpp"
#include "frame_limit.hpp"
#include "imgui_pass.hpp"
#include "sample_math.hpp"
#include "sample_window.hpp"
#include "scene_pass.hpp"
#include "sphere_world.hpp"

#include <bytee/core/log.hpp>
#include <bytee/render/graph/render_graph.hpp>
#include <bytee/render/rhi/instance_factory.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <numbers>
#include <optional>
#include <random>
#include <utility>

namespace core = bytee::core;
namespace render = bytee::render;
namespace rhi = bytee::render::rhi;
using bytee::samples::FpsCamera;
using bytee::samples::ImGuiPass;
using bytee::samples::Key;
using bytee::samples::SampleWindow;
using bytee::samples::ScenePass;
using bytee::samples::Sphere;
using bytee::samples::SphereWorld;
using bytee::samples::Vec3;

namespace {

constexpr uint32_t k_frames_in_flight = 2;
constexpr uint32_t k_max_spheres = 1024;
constexpr float k_mouse_sensitivity = 0.0025f; // radians per point
constexpr float k_walk_speed = 5.0f;           // meters per second
constexpr float k_sprint_factor = 2.2f;
constexpr float k_throw_interval = 0.1f; // seconds between spheres while E is held
constexpr float k_throw_speed = 14.0f;

// ImGui context and GLFW input backend for the lifetime of the sample.
class ImGuiSession {
public:
    explicit ImGuiSession(const SampleWindow& window) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::GetIO().IniFilename = nullptr;
        ImGui_ImplGlfw_InitForOther(window.glfw_window(), true);
    }
    ImGuiSession(const ImGuiSession&) = delete;
    ImGuiSession& operator=(const ImGuiSession&) = delete;
    ~ImGuiSession() {
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
};

// Turns keyboard and mouse into camera motion and sphere throws.
class PlayerInput {
private:
    std::optional<bytee::samples::CursorPosition> last_cursor_;
    float throw_cooldown_ = 0.0f;
    std::mt19937 random_{7};

public:
    void update(const SampleWindow& window, FpsCamera& camera, SphereWorld& world, float seconds) {
        update_cursor_capture(window);
        if (window.is_cursor_captured()) {
            const bytee::samples::CursorPosition cursor = window.cursor_position();
            if (last_cursor_) {
                camera.look(static_cast<float>(cursor.x - last_cursor_->x) * k_mouse_sensitivity,
                            -static_cast<float>(cursor.y - last_cursor_->y) * k_mouse_sensitivity);
            }
            last_cursor_ = cursor;
        } else {
            last_cursor_.reset(); // no jump when the cursor is captured again
        }

        const float speed = k_walk_speed * (window.is_key_down(Key::LeftShift) ? k_sprint_factor : 1.0f) * seconds;
        const float forward = (window.is_key_down(Key::W) ? 1.0f : 0.0f) - (window.is_key_down(Key::S) ? 1.0f : 0.0f);
        const float right = (window.is_key_down(Key::D) ? 1.0f : 0.0f) - (window.is_key_down(Key::A) ? 1.0f : 0.0f);
        const float diagonal = (forward != 0.0f && right != 0.0f) ? std::numbers::sqrt2_v<float> * 0.5f : 1.0f;
        camera.walk(forward * speed * diagonal, right * speed * diagonal);
        if (window.is_key_down(Key::Space)) {
            camera.jump();
        }
        camera.update(seconds);

        throw_cooldown_ = std::max(throw_cooldown_ - seconds, 0.0f);
        if (window.is_key_down(Key::E) && throw_cooldown_ == 0.0f) {
            throw_sphere(camera, world);
            throw_cooldown_ = k_throw_interval;
        }
    }

    void throw_sphere(const FpsCamera& camera, SphereWorld& world) {
        std::uniform_real_distribution<float> radius(0.2f, 0.55f);
        std::uniform_real_distribution<float> hue(0.0f, 1.0f);
        const Vec3 direction = camera.forward();
        const float size = radius(random_);
        world.spawn({.position = camera.position() + direction * (size + 0.6f),
                     .radius = size,
                     .velocity = direction * k_throw_speed + Vec3{0.0f, 1.5f, 0.0f},
                     .color = color_from_hue(hue(random_))});
    }

private:
    // Esc releases the mouse; a click outside the HUD captures it again.
    static void update_cursor_capture(const SampleWindow& window) {
        if (window.is_cursor_captured() && window.is_key_down(Key::Escape)) {
            window.set_cursor_captured(false);
        } else if (!window.is_cursor_captured() && window.is_mouse_button_down(bytee::samples::MouseButton::Left) &&
                   !ImGui::GetIO().WantCaptureMouse) {
            window.set_cursor_captured(true);
        }
    }

    // A saturated color of the given hue, in linear RGB.
    static Vec3 color_from_hue(float hue) {
        const auto channel = [hue](float offset) {
            const float value = std::clamp(std::abs(std::fmod(hue * 6.0f + offset, 6.0f) - 3.0f) - 1.0f, 0.0f, 1.0f);
            return std::pow(0.15f + 0.8f * value, 2.2f);
        };
        return {channel(0.0f), channel(4.0f), channel(2.0f)};
    }
};

void build_hud(const SphereWorld& world, bool cursor_captured) {
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos({16, 16}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.45f);
    ImGui::Begin("HUD", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
    ImGui::Text("WASD move   Mouse look   Space jump   Shift sprint   E throw spheres");
    ImGui::Text(cursor_captured ? "Esc releases the mouse" : "Click to capture the mouse");
    ImGui::Separator();
    ImGui::Text("Spheres: %zu / %u", world.spheres().size(), world.capacity());
    ImGui::Text("%.2f ms (%.0f FPS)", static_cast<double>(1000.0f / io.Framerate), static_cast<double>(io.Framerate));
    ImGui::End();
}

core::Result<void> render_frame(rhi::Device& device, rhi::Swapchain& swapchain, render::RenderGraph& graph,
                                ScenePass& scene, ImGuiPass& hud, rhi::SwapchainFrame frame) {
    graph.reset();
    const render::TextureRef backbuffer = graph.import_texture(
        "backbuffer", frame.texture, {.before = rhi::ResourceState::Undefined, .after = rhi::ResourceState::Present});
    scene.set_target(backbuffer);
    graph.add_pass("Scene", scene);
    hud.set_target(backbuffer);
    graph.add_pass("HUD", hud);
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

    core::Result<SampleWindow> window =
        bytee::samples::create_sample_window("Bytee — 05_fps", {.width = 1280, .height = 720});
    if (!window) {
        logger.error("{}", window.error().message());
        return 1;
    }
    const rhi::InstanceFactory factory{logger};
    core::Result<rhi::Instance> instance = factory.create_instance({.app_name = "05_fps", .enable_validation = true});
    if (!instance) {
        logger.error("cannot create an instance: {}", instance.error().message());
        return 1;
    }
    core::Result<rhi::Device> device = instance->create_device({.debug_name = "FPS Device"});
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
    core::Result<ScenePass> scene =
        bytee::samples::create_scene_pass(*device, swapchain->format(), k_frames_in_flight, k_max_spheres);
    core::Result<ImGuiPass> hud = bytee::samples::create_imgui_pass(*device, swapchain->format(), k_frames_in_flight);
    if (!scene || !hud) {
        logger.error("cannot create the passes: {}", (!scene ? scene.error() : hud.error()).message());
        return 1;
    }
    render::RenderGraph graph{*device};
    FpsCamera camera;
    SphereWorld world{k_max_spheres};
    PlayerInput input;
    window->set_cursor_captured(!max_frames); // automated runs leave the mouse alone
    logger.info("rendering on {} at {}x{} ({})", device->adapter().name, swapchain->extent().width,
                swapchain->extent().height, rhi::to_string(swapchain->format()));

    using Clock = std::chrono::steady_clock;
    Clock::time_point last_time = Clock::now();
    uint64_t frames = 0;
    while (!window->should_close() && (!max_frames || frames < *max_frames)) {
        window->poll_events();
        const Clock::time_point now = Clock::now();
        const float seconds = std::min(std::chrono::duration<float>(now - last_time).count(), 0.1f);
        last_time = now;

        input.update(*window, camera, world, seconds);
        if (max_frames) { // automated run: exercise throwing, physics and camera motion
            camera.look(0.01f, 0.0f);
            if (frames % 4 == 0) {
                input.throw_sphere(camera, world);
            }
        }
        world.update(seconds);

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
        build_hud(world, window->is_cursor_captured());
        ImGui::Render();

        const core::Extent2D extent = swapchain->extent();
        const float aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
        scene->set_view(bytee::samples::perspective_reverse_z(1.15f, aspect, 0.05f) * camera.view(), camera.position());
        scene->set_spheres(world.spheres());
        if (auto rendered = render_frame(*device, *swapchain, graph, *scene, *hud, *frame); !rendered) {
            logger.error("frame failed: {}", rendered.error().message());
            return 1;
        }
        ++frames;
    }
    if (auto idle = device->wait_idle(); !idle) {
        logger.error("{}", idle.error().message());
        return 1;
    }
    logger.info("presented {} frames, {} spheres", frames, world.spheres().size());
    return 0;
}
