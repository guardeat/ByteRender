#include "sample_window.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <string>

namespace bytee::samples {

// Platform part (native_window_<platform>): the window as Bytee sees it.
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

core::Result<SampleWindow> create_sample_window(std::string_view title, core::Extent2D size) {
    if (glfwInit() != GLFW_TRUE) {
        return core::make_error(core::ErrorCode::InitializationFailed, "GLFW could not be initialized");
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // Bytee renders through Vulkan, not an OpenGL context
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
