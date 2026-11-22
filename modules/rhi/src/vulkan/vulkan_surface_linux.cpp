// Linux window-system surfaces (X11 via Xlib or XCB, Wayland).
//
// The few window-system types the Vulkan platform headers need are declared here instead of including the X11 /
// XCB / Wayland headers: those headers are not needed to build ByteRender, and X11 defines macros (None, Always, Bool,
// Status, ...) that collide with ordinary C++ names. The declarations match the system typedefs exactly.

#include "vulkan/vulkan_surface_linux.hpp"

#include <cstdint>

using Display = struct _XDisplay;
using Window = unsigned long;   // XID
using VisualID = unsigned long; // X11 VisualID
using xcb_connection_t = struct xcb_connection_t;
using xcb_window_t = uint32_t;
using xcb_visualid_t = uint32_t;
struct wl_display;
struct wl_surface;

#include <vulkan/vulkan_wayland.h>
#include <vulkan/vulkan_xcb.h>
#include <vulkan/vulkan_xlib.h>

namespace bytee::render::rhi::vulkan {

namespace {

// Platform functions are fetched on demand: volk only loads those whose platform macros are defined at build time.
template <class Function_>
Function_ instance_function(VkInstance instance, const char* name) {
    return reinterpret_cast<Function_>(vkGetInstanceProcAddr(instance, name));
}

} // namespace

VkResult create_linux_surface(VkInstance instance, const NativeWindow& window, VkSurfaceKHR& surface) {
    switch (window.platform) {
    case NativeWindow::Platform::Xlib:
        if (const auto create = instance_function<PFN_vkCreateXlibSurfaceKHR>(instance, "vkCreateXlibSurfaceKHR")) {
            const VkXlibSurfaceCreateInfoKHR info{.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
                                                  .dpy = static_cast<Display*>(window.display),
                                                  .window = static_cast<Window>(window.id)};
            return create(instance, &info, nullptr, &surface);
        }
        break;
    case NativeWindow::Platform::Xcb:
        if (const auto create = instance_function<PFN_vkCreateXcbSurfaceKHR>(instance, "vkCreateXcbSurfaceKHR")) {
            const VkXcbSurfaceCreateInfoKHR info{.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,
                                                 .connection = static_cast<xcb_connection_t*>(window.display),
                                                 .window = static_cast<xcb_window_t>(window.id)};
            return create(instance, &info, nullptr, &surface);
        }
        break;
    case NativeWindow::Platform::Wayland:
        if (const auto create =
                instance_function<PFN_vkCreateWaylandSurfaceKHR>(instance, "vkCreateWaylandSurfaceKHR")) {
            const VkWaylandSurfaceCreateInfoKHR info{.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
                                                     .display = static_cast<wl_display*>(window.display),
                                                     .surface = static_cast<wl_surface*>(window.handle)};
            return create(instance, &info, nullptr, &surface);
        }
        break;
    default: break;
    }
    return VK_ERROR_EXTENSION_NOT_PRESENT;
}

} // namespace bytee::render::rhi::vulkan
