// Windows: ByteRender presents to the window's HWND (VK_KHR_win32_surface).

#include "sample_window.hpp"

#define GLFW_INCLUDE_NONE
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

namespace bytee::samples {

render::rhi::NativeWindow make_native_window(GLFWwindow* window) {
    return render::rhi::NativeWindow{
        .platform = render::rhi::NativeWindow::Platform::Win32,
        .display = GetModuleHandleW(nullptr),
        .handle = glfwGetWin32Window(window),
    };
}

} // namespace bytee::samples
