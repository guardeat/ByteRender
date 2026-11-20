#include "vulkan/vulkan_device_requirements.hpp"

#include <array>
#include <format>
#include <string_view>
#include <vector>

namespace byte::render::rhi::vulkan {

namespace {

template <class Struct>
struct FeatureBit {
    std::string_view name;
    VkBool32 Struct::* member;
};

#define BYTE_RENDER_VK_FEATURE(Struct, field)                                                              \
    FeatureBit<Struct> {                                                                                   \
        #field, &Struct::field                                                                             \
    }

constexpr std::array k_core_features{
    BYTE_RENDER_VK_FEATURE(VkPhysicalDeviceFeatures, samplerAnisotropy),
};

constexpr std::array k_vulkan12_features{
    // Timeline semaphores: GPU lifetime tracking and submission sync.
    BYTE_RENDER_VK_FEATURE(VkPhysicalDeviceVulkan12Features, timelineSemaphore),
    // Bindless descriptor heap.
    BYTE_RENDER_VK_FEATURE(VkPhysicalDeviceVulkan12Features, descriptorIndexing),
    BYTE_RENDER_VK_FEATURE(VkPhysicalDeviceVulkan12Features, runtimeDescriptorArray),
    BYTE_RENDER_VK_FEATURE(VkPhysicalDeviceVulkan12Features, descriptorBindingPartiallyBound),
    BYTE_RENDER_VK_FEATURE(VkPhysicalDeviceVulkan12Features, descriptorBindingVariableDescriptorCount),
    BYTE_RENDER_VK_FEATURE(VkPhysicalDeviceVulkan12Features, descriptorBindingUpdateUnusedWhilePending),
    BYTE_RENDER_VK_FEATURE(VkPhysicalDeviceVulkan12Features, descriptorBindingSampledImageUpdateAfterBind),
    BYTE_RENDER_VK_FEATURE(VkPhysicalDeviceVulkan12Features, descriptorBindingStorageImageUpdateAfterBind),
    BYTE_RENDER_VK_FEATURE(VkPhysicalDeviceVulkan12Features, descriptorBindingStorageBufferUpdateAfterBind),
    BYTE_RENDER_VK_FEATURE(VkPhysicalDeviceVulkan12Features, shaderSampledImageArrayNonUniformIndexing),
    BYTE_RENDER_VK_FEATURE(VkPhysicalDeviceVulkan12Features, shaderStorageImageArrayNonUniformIndexing),
    BYTE_RENDER_VK_FEATURE(VkPhysicalDeviceVulkan12Features, shaderStorageBufferArrayNonUniformIndexing),
    // Buffers referenced by GPU address; C-like (scalar) layout for push constants and buffers.
    BYTE_RENDER_VK_FEATURE(VkPhysicalDeviceVulkan12Features, bufferDeviceAddress),
    BYTE_RENDER_VK_FEATURE(VkPhysicalDeviceVulkan12Features, scalarBlockLayout),
};

constexpr std::array k_vulkan13_features{
    BYTE_RENDER_VK_FEATURE(VkPhysicalDeviceVulkan13Features, dynamicRendering),
    BYTE_RENDER_VK_FEATURE(VkPhysicalDeviceVulkan13Features, synchronization2),
};

#undef BYTE_RENDER_VK_FEATURE

template <class Struct, size_t N>
void collect_missing(const Struct& supported, const std::array<FeatureBit<Struct>, N>& required,
                     std::vector<std::string_view>& missing) {
    for (const FeatureBit<Struct>& feature : required) {
        if (supported.*feature.member != VK_TRUE) {
            missing.push_back(feature.name);
        }
    }
}

template <class Struct, size_t N>
void enable(Struct& features, const std::array<FeatureBit<Struct>, N>& required) noexcept {
    for (const FeatureBit<Struct>& feature : required) {
        features.*feature.member = VK_TRUE;
    }
}

} // namespace

void enable_required_features(DeviceFeatureChain& chain) noexcept {
    enable(chain.features2.features, k_core_features);
    enable(chain.vulkan12, k_vulkan12_features);
    enable(chain.vulkan13, k_vulkan13_features);
}

std::optional<uint32_t> find_universal_queue_family(VkPhysicalDevice physical_device) {
    const auto families =
        enumerate<VkQueueFamilyProperties>(vkGetPhysicalDeviceQueueFamilyProperties, physical_device);
    constexpr VkQueueFlags k_universal = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
    for (uint32_t index = 0; index < families.size(); ++index) {
        if ((families[index].queueFlags & k_universal) == k_universal && families[index].queueCount > 0) {
            return index;
        }
    }
    return std::nullopt;
}

std::optional<std::string> check_adapter_requirements(VkPhysicalDevice physical_device) {
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physical_device, &properties);
    if (properties.apiVersion < k_required_api_version) {
        const core::Version version = to_version(properties.apiVersion);
        return std::format("requires Vulkan 1.3, adapter supports {}.{}", version.major, version.minor);
    }

    if (!find_universal_queue_family(physical_device)) {
        return std::string{"no queue family supports both graphics and compute"};
    }

    DeviceFeatureChain supported;
    vkGetPhysicalDeviceFeatures2(physical_device, &supported.features2);

    std::vector<std::string_view> missing;
    collect_missing(supported.features2.features, k_core_features, missing);
    collect_missing(supported.vulkan12, k_vulkan12_features, missing);
    collect_missing(supported.vulkan13, k_vulkan13_features, missing);
    if (!missing.empty()) {
        std::string reason = "missing features:";
        for (const std::string_view name : missing) {
            reason += std::format(" {}", name);
        }
        return reason;
    }
    return std::nullopt;
}

} // namespace byte::render::rhi::vulkan
