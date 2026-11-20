#pragma once

#include <byte/core/result.hpp>

namespace byte::render::rhi::vulkan {

// Loads the Vulkan loader library and global entry points (volk). Safe to call repeatedly and from
// multiple threads; the work happens once per process.
[[nodiscard]] core::Result<void> load_vulkan_loader();

} // namespace byte::render::rhi::vulkan
