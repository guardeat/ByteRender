#pragma once

// Vulkan backend internals. Only included by backend sources, never by public headers.

#include <bytee/core/result.hpp>
#include <bytee/core/version.hpp>

#include <volk.h>

#include <concepts>
#include <cstdint>
#include <span>
#include <string_view>
#include <type_traits>
#include <vector>

namespace bytee::render::rhi::vulkan {

inline constexpr uint32_t k_required_api_version = VK_API_VERSION_1_3;

[[nodiscard]] std::string_view to_string(VkResult result) noexcept;

[[nodiscard]] bool has_extension(std::span<const VkExtensionProperties> extensions, std::string_view name) noexcept;
[[nodiscard]] core::ErrorCode to_error_code(VkResult result) noexcept;

// Error for a failed Vulkan call, recording the caller's location in Debug:
//   return make_vk_error(result, "vkCreateInstance");
[[nodiscard]] core::Error make_vk_error(VkResult result, core::ErrorFormat<> call);

// Names a Vulkan object for validation messages and GPU debuggers. No-op unless VK_EXT_debug_utils is enabled
// (validation on).
void set_debug_name(VkDevice device, VkObjectType type, uint64_t handle, std::string_view name);

template <class Handle_>
    requires std::is_pointer_v<Handle_> // Vulkan handles are pointers on 64-bit targets
void set_debug_name(VkDevice device, VkObjectType type, Handle_ handle, std::string_view name) {
    set_debug_name(device, type, reinterpret_cast<uint64_t>(handle), name);
}

[[nodiscard]] constexpr core::Version to_version(uint32_t vk_version) noexcept {
    return core::Version{
        .major = VK_API_VERSION_MAJOR(vk_version),
        .minor = VK_API_VERSION_MINOR(vk_version),
        .patch = VK_API_VERSION_PATCH(vk_version),
    };
}

// Runs a Vulkan two-call enumeration (count, then data) and returns the items; empty on failure.
// Works with VkResult-returning and void-returning functions:
//   auto layers   = enumerate<VkLayerProperties>(vkEnumerateInstanceLayerProperties);
//   auto gpus     = enumerate<VkPhysicalDevice>(vkEnumeratePhysicalDevices, instance);
//   auto families = enumerate<VkQueueFamilyProperties>(vkGetPhysicalDeviceQueueFamilyProperties, gpu);
template <class Item_, class Function_, class... Args_>
    requires std::invocable<Function_&, Args_..., uint32_t*, Item_*>
[[nodiscard]] std::vector<Item_> enumerate(Function_ function, Args_... args) {
    std::vector<Item_> items;
    uint32_t count = 0;
    if constexpr (std::is_void_v<std::invoke_result_t<Function_, Args_..., uint32_t*, Item_*>>) {
        function(args..., &count, nullptr);
        items.resize(count);
        function(args..., &count, items.data());
    } else {
        VkResult result = VK_INCOMPLETE;
        while (result == VK_INCOMPLETE) { // the count may change between the two calls
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

} // namespace bytee::render::rhi::vulkan
