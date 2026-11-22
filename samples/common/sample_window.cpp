#include "sample_window.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <string>

namespace bytee::samples {

// Platform part (native_window_<platform>): the window as ByteRender sees it.
render::rhi::NativeWindow make_native_window(GLFWwindow* window);

SampleWindow::~SampleWindow() {
    if (window_ != nullptr) {
        glfwDestroyWindow(window_);
        glfwTerminate();
    }
}

SampleWindow& SampleWindow::operator=(SampleWindow&& other) noexcept {
    if (this != &other) {
        if (window_ != nullptr) {
            glfwDestroyWindow(window_);
            glfwTerminate();
        }
        window_ = std::exchange(other.window_, nullptr);
    }
    return *this;
}

render::rhi::NativeWindow SampleWindow::native() const {
    return make_native_window(window_);
}

core::Extent2D SampleWindow::framebuffer_extent() const {
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window_, &width, &height);
    return {.width = static_cast<uint32_t>(width), .height = static_cast<uint32_t>(height)};
}

bool SampleWindow::should_close() const {
    return glfwWindowShouldClose(window_) != 0;
}

void SampleWindow::poll_events() const {
    glfwPollEvents();
}

void SampleWindow::wait_events() const {
    glfwWaitEvents();
}

bool SampleWindow::is_key_down(Key key) const {
    int code = GLFW_KEY_UNKNOWN;
    switch (key) {
    case Key::W: code = GLFW_KEY_W; break;
    case Key::A: code = GLFW_KEY_A; break;
    case Key::S: code = GLFW_KEY_S; break;
    case Key::D: code = GLFW_KEY_D; break;
    case Key::E: code = GLFW_KEY_E; break;
    case Key::Space: code = GLFW_KEY_SPACE; break;
    case Key::LeftShift: code = GLFW_KEY_LEFT_SHIFT; break;
    case Key::Escape: code = GLFW_KEY_ESCAPE; break;
    }
    return glfwGetKey(window_, code) == GLFW_PRESS;
}

bool SampleWindow::is_mouse_button_down(MouseButton button) const {
    const int code = button == MouseButton::Left ? GLFW_MOUSE_BUTTON_LEFT : GLFW_MOUSE_BUTTON_RIGHT;
    return glfwGetMouseButton(window_, code) == GLFW_PRESS;
}

CursorPosition SampleWindow::cursor_position() const {
    CursorPosition position;
    glfwGetCursorPos(window_, &position.x, &position.y);
    return position;
}

void SampleWindow::set_cursor_captured(bool captured) const {
    glfwSetInputMode(window_, GLFW_CURSOR, captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    if (glfwRawMouseMotionSupported() == GLFW_TRUE) {
        glfwSetInputMode(window_, GLFW_RAW_MOUSE_MOTION, captured ? GLFW_TRUE : GLFW_FALSE);
    }
}

bool SampleWindow::is_cursor_captured() const {
    return glfwGetInputMode(window_, GLFW_CURSOR) == GLFW_CURSOR_DISABLED;
}

core::Result<SampleWindow> create_sample_window(std::string_view title, core::Extent2D size) {
    if (glfwInit() != GLFW_TRUE) {
        return core::make_error(core::ErrorCode::InitializationFailed, "GLFW could not be initialized");
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // ByteRender renders through Vulkan, not an OpenGL context
    const std::string title_text{title};
    GLFWwindow* window = glfwCreateWindow(static_cast<int>(size.width), static_cast<int>(size.height),
                                          title_text.c_str(), nullptr, nullptr);
    if (window == nullptr) {
        glfwTerminate();
        return core::make_error(core::ErrorCode::InitializationFailed, "GLFW could not create a window");
    }
    return SampleWindow{window};
}

} // namespace bytee::samples
