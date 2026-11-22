#pragma once

// Linux window-system surfaces (vulkan_surface_linux.cpp; built on Linux only).

#include "vulkan/vulkan_common.hpp"

#include <bytee/render/rhi/native_window.hpp>

namespace bytee::render::rhi::vulkan {

// Creates a surface for an Xlib, XCB or Wayland window. VK_ERROR_EXTENSION_NOT_PRESENT if the instance lacks the
// platform's surface extension (or `window` is not a Linux window).
[[nodiscard]] VkResult create_linux_surface(VkInstance instance, const NativeWindow& window, VkSurfaceKHR& surface);

} // namespace bytee::render::rhi::vulkan
