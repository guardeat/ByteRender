// Linux: Bytee presents to the window's X11 window (VK_KHR_xlib_surface) or Wayland surface (VK_KHR_wayland_surface),
// whichever platform GLFW runs on.

#include "sample_window.hpp" // before the X11 headers below: their macros (None, Bool, ...) must not reach it

#define GLFW_INCLUDE_NONE
#define GLFW_EXPOSE_NATIVE_X11
#define GLFW_EXPOSE_NATIVE_WAYLAND
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

namespace bytee::samples {

render::rhi::NativeWindow make_native_window(GLFWwindow* window) {
    if (glfwGetPlatform() == GLFW_PLATFORM_WAYLAND) {
        return render::rhi::NativeWindow{
            .platform = render::rhi::NativeWindow::Platform::Wayland,
            .display = glfwGetWaylandDisplay(),
            .handle = glfwGetWaylandWindow(window),
        };
    }
    return render::rhi::NativeWindow{
        .platform = render::rhi::NativeWindow::Platform::Xlib,
        .display = glfwGetX11Display(),
        .id = static_cast<uint64_t>(glfwGetX11Window(window)),
    };
}

} // namespace bytee::samples
