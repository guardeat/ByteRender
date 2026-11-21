#pragma once

#include "vulkan/vulkan_common.hpp"

#include <optional>
#include <string>

namespace bytee::render::rhi::vulkan {

// Features2 -> Vulkan12 -> Vulkan13 chain. Self-referential, so neither copyable nor movable.
struct DeviceFeatureChain {
    VkPhysicalDeviceFeatures2 features2{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    VkPhysicalDeviceVulkan12Features vulkan12{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    VkPhysicalDeviceVulkan13Features vulkan13{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};

    DeviceFeatureChain() noexcept {
        features2.pNext = &vulkan12;
        vulkan12.pNext = &vulkan13;
    }
    DeviceFeatureChain(const DeviceFeatureChain&) = delete;
    DeviceFeatureChain& operator=(const DeviceFeatureChain&) = delete;
};

// Turns on exactly the features Bytee requires (architecture §10).
void enable_required_features(DeviceFeatureChain& chain) noexcept;

// A queue family supporting graphics + compute (transfer is implied), if any.
[[nodiscard]] std::optional<uint32_t> find_universal_queue_family(VkPhysicalDevice physical_device);

// Empty if the adapter meets every requirement, otherwise a human-readable reason.
[[nodiscard]] std::optional<std::string> check_adapter_requirements(VkPhysicalDevice physical_device);

} // namespace bytee::render::rhi::vulkan
