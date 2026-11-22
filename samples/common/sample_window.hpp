#pragma once

// Minimal GLFW window for the samples: owns GLFW, describes itself as an rhi::NativeWindow. Not part of the engine.

#include <bytee/core/result.hpp>
#include <bytee/core/types.hpp>
#include <bytee/render/rhi/native_window.hpp>

#include <string_view>
#include <utility>

struct GLFWwindow;

namespace bytee::samples {

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

private:
    friend core::Result<SampleWindow> create_sample_window(std::string_view title, core::Extent2D size);

    explicit SampleWindow(GLFWwindow* window) noexcept
        : window_(window) {}
};

// One window per process (GLFW is initialized with the first window and terminated with it).
[[nodiscard]] core::Result<SampleWindow> create_sample_window(std::string_view title, core::Extent2D size);

} // namespace bytee::samples
