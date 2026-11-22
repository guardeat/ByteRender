#pragma once

// Minimal GLFW window for the samples: owns GLFW, describes itself as an rhi::NativeWindow. Not part of the engine.

#include <bytee/core/result.hpp>
#include <bytee/core/types.hpp>
#include <bytee/render/rhi/native_window.hpp>

#include <cstdint>
#include <string_view>
#include <utility>

struct GLFWwindow;

namespace bytee::samples {

// Keys and buttons the samples use (polled, see SampleWindow::is_key_down).
enum class Key : uint8_t {
    W,
    A,
    S,
    D,
    E,
    Space,
    LeftShift,
    Escape,
};

enum class MouseButton : uint8_t {
    Left,
    Right,
};

struct CursorPosition {
    double x = 0.0; // screen points, top-left origin; unbounded while the cursor is captured
    double y = 0.0;
};

class SampleWindow {
private:
    GLFWwindow* window_ = nullptr;

public:
    SampleWindow() noexcept = default;
    ~SampleWindow();

    SampleWindow(SampleWindow&& other) noexcept
        : window_(std::exchange(other.window_, nullptr)) {}
    SampleWindow& operator=(SampleWindow&& other) noexcept;
    SampleWindow(const SampleWindow&) = delete;
    SampleWindow& operator=(const SampleWindow&) = delete;

    [[nodiscard]] render::rhi::NativeWindow native() const;
    [[nodiscard]] core::Extent2D framebuffer_extent() const; // pixels (differs from points on HiDPI screens)
    [[nodiscard]] bool should_close() const;
    void poll_events() const;
    void wait_events() const; // blocks until an event arrives (e.g. while minimized)
    [[nodiscard]] GLFWwindow* glfw_window() const noexcept { return window_; } // for input backends (ImGui)

    // Input, polled (state as of the last poll_events()).
    [[nodiscard]] bool is_key_down(Key key) const;
    [[nodiscard]] bool is_mouse_button_down(MouseButton button) const;
    [[nodiscard]] CursorPosition cursor_position() const;
    // A captured cursor is hidden and not limited by the window (mouse look); uses raw mouse motion when available.
    void set_cursor_captured(bool captured) const;
    [[nodiscard]] bool is_cursor_captured() const;

private:
    friend core::Result<SampleWindow> create_sample_window(std::string_view title, core::Extent2D size);

    explicit SampleWindow(GLFWwindow* window) noexcept
        : window_(window) {}
};

// One window per process (GLFW is initialized with the first window and terminated with it).
[[nodiscard]] core::Result<SampleWindow> create_sample_window(std::string_view title, core::Extent2D size);

} // namespace bytee::samples
