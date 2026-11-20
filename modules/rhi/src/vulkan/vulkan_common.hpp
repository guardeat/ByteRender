#pragma once

// Only included by Vulkan backend sources. Never include from public headers.

#include <byte/core/result.hpp>
#include <byte/core/version.hpp>

#include <volk.h>

#include <string_view>
#include <vector>

namespace byte::render::rhi::vulkan {

[[nodiscard]] std::string_view to_string(VkResult result) noexcept;
[[nodiscard]] core::ErrorCode to_error_code(VkResult result) noexcept;

// Error for a failed Vulkan call, e.g. `return make_vk_error(result, "vkCreateInstance");`
[[nodiscard]] core::Error make_vk_error(VkResult result, std::string_view call);

[[nodiscard]] constexpr core::Version to_version(uint32_t vk_version) noexcept {
    return core::Version{
        .major = VK_API_VERSION_MAJOR(vk_version),
        .minor = VK_API_VERSION_MINOR(vk_version),
        .patch = VK_API_VERSION_PATCH(vk_version),
    };
}

// Runs a Vulkan two-call enumeration (count, then data) and returns the items.
//   auto layers = enumerate<VkLayerProperties>(vkEnumerateInstanceLayerProperties);
//   auto gpus   = enumerate<VkPhysicalDevice>(vkEnumeratePhysicalDevices, instance);
template <class T, class Function, class... Args>
[[nodiscard]] std::vector<T> enumerate(Function function, Args... args) {
    std::vector<T> items;
    uint32_t count = 0;
    VkResult result = VK_INCOMPLETE;
    while (result == VK_INCOMPLETE) {
        if (function(args..., &count, nullptr) != VK_SUCCESS) {
            return {};
        }
        items.resize(count);
        result = function(args..., &count, items.data());
    }
    items.resize(result == VK_SUCCESS ? count : 0);
    return items;
}

} // namespace byte::render::rhi::vulkan
