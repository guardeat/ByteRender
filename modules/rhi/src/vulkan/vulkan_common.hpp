#pragma once

// Only included by Vulkan backend sources. Never include from public headers.

#include <byte/core/result.hpp>
#include <byte/core/version.hpp>

#include <volk.h>

#include <string_view>
#include <type_traits>
#include <vector>

namespace byte::render::rhi::vulkan {

[[nodiscard]] std::string_view to_string(VkResult result) noexcept;
[[nodiscard]] core::ErrorCode to_error_code(VkResult result) noexcept;

// Error for a failed Vulkan call, e.g. `return make_vk_error(result, "vkCreateInstance");`
[[nodiscard]] core::Error make_vk_error(VkResult result, std::string_view call);

// Names a Vulkan object for validation messages and GPU debuggers. No-op without VK_EXT_debug_utils.
void set_debug_name(VkDevice device, VkObjectType type, uint64_t handle, std::string_view name);

template <class Handle>
    requires std::is_pointer_v<Handle> // Vulkan handles are pointers on 64-bit targets
void set_debug_name(VkDevice device, VkObjectType type, Handle handle, std::string_view name) {
    set_debug_name(device, type, reinterpret_cast<uint64_t>(handle), name);
}

[[nodiscard]] constexpr core::Version to_version(uint32_t vk_version) noexcept {
    return core::Version{
        .major = VK_API_VERSION_MAJOR(vk_version),
        .minor = VK_API_VERSION_MINOR(vk_version),
        .patch = VK_API_VERSION_PATCH(vk_version),
    };
}

// Runs a Vulkan two-call enumeration (count, then data) and returns the items. Works for both
// VkResult-returning and void-returning enumeration functions.
//   auto layers   = enumerate<VkLayerProperties>(vkEnumerateInstanceLayerProperties);
//   auto gpus     = enumerate<VkPhysicalDevice>(vkEnumeratePhysicalDevices, instance);
//   auto families = enumerate<VkQueueFamilyProperties>(vkGetPhysicalDeviceQueueFamilyProperties, gpu);
template <class T, class Function, class... Args>
[[nodiscard]] std::vector<T> enumerate(Function function, Args... args) {
    std::vector<T> items;
    uint32_t count = 0;
    if constexpr (std::is_void_v<std::invoke_result_t<Function, Args..., uint32_t*, T*>>) {
        function(args..., &count, nullptr);
        items.resize(count);
        function(args..., &count, items.data());
    } else {
        VkResult result = VK_INCOMPLETE;
        while (result == VK_INCOMPLETE) {
            if (function(args..., &count, nullptr) != VK_SUCCESS) {
                return {};
            }
            items.resize(count);
            result = function(args..., &count, items.data());
        }
        if (result != VK_SUCCESS) {
            return {};
        }
    }
    items.resize(count);
    return items;
}

} // namespace byte::render::rhi::vulkan
