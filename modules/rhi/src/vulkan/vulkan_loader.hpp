#pragma once

#include <bytee/core/result.hpp>

namespace bytee::render::rhi::vulkan {

// Opens the Vulkan loader and loads the global entry points (volk). Thread-safe; the work happens once per process.
[[nodiscard]] core::Result<void> load_vulkan_loader();

} // namespace bytee::render::rhi::vulkan
