#pragma once

#include "vulkan/vulkan_common.hpp"

#include <optional>
#include <string>

namespace byte::render::rhi::vulkan {

inline constexpr uint32_t k_required_api_version = VK_API_VERSION_1_3;

// Features2 -> Vulkan12 -> Vulkan13 chain. Self-referential, so neither copyable nor movable.
struct DeviceFeatureChain {
    DeviceFeatureChain() noexcept {
        features2.pNext = &vulkan12;
        vulkan12.pNext = &vulkan13;
    }
    DeviceFeatureChain(const DeviceFeatureChain&) = delete;
    DeviceFeatureChain& operator=(const DeviceFeatureChain&) = delete;

    VkPhysicalDeviceFeatures2 features2{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    VkPhysicalDeviceVulkan12Features vulkan12{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    VkPhysicalDeviceVulkan13Features vulkan13{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
};

// Turns on exactly the features ByteRender requires (see ARCHITECTURE: bindless, sync2, dynamic rendering...).
void enable_required_features(DeviceFeatureChain& chain) noexcept;

// Queue family supporting graphics + compute (+ transfer, implied), if any.
[[nodiscard]] std::optional<uint32_t> find_universal_queue_family(VkPhysicalDevice physical_device);

// Empty if the adapter meets every requirement, otherwise a human-readable reason.
[[nodiscard]] std::optional<std::string> check_adapter_requirements(VkPhysicalDevice physical_device);

} // namespace byte::render::rhi::vulkan
