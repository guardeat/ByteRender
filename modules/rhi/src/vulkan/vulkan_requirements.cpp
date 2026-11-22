#include "vulkan/vulkan_requirements.hpp"

#include <array>
#include <format>
#include <string_view>
#include <type_traits>
#include <vector>

namespace bytee::render::rhi::vulkan {

namespace {

// A Vulkan feature struct (VkPhysicalDeviceFeatures, VkPhysicalDeviceVulkan12Features, ...).
template <class Struct_>
concept FeatureStruct = std::is_class_v<Struct_> && std::is_trivially_copyable_v<Struct_>;

template <FeatureStruct Struct_>
struct FeatureBit {
    std::string_view name;
    VkBool32 Struct_::* member;
};

#define BYTEE_VULKAN_FEATURE(Struct, field)                                                                            \
    FeatureBit<Struct> {                                                                                               \
        #field, &Struct::field                                                                                         \
    }

constexpr std::array k_core_features{
    BYTEE_VULKAN_FEATURE(VkPhysicalDeviceFeatures, samplerAnisotropy),
    // Bindless storage images are declared without a format qualifier.
    BYTEE_VULKAN_FEATURE(VkPhysicalDeviceFeatures, shaderStorageImageReadWithoutFormat),
    BYTEE_VULKAN_FEATURE(VkPhysicalDeviceFeatures, shaderStorageImageWriteWithoutFormat),
};

constexpr std::array k_vulkan12_features{
    // GPU lifetime tracking and submission synchronization.
    BYTEE_VULKAN_FEATURE(VkPhysicalDeviceVulkan12Features, timelineSemaphore),
    // Bindless descriptor heap.
    BYTEE_VULKAN_FEATURE(VkPhysicalDeviceVulkan12Features, descriptorIndexing),
    BYTEE_VULKAN_FEATURE(VkPhysicalDeviceVulkan12Features, runtimeDescriptorArray),
    BYTEE_VULKAN_FEATURE(VkPhysicalDeviceVulkan12Features, descriptorBindingPartiallyBound),
    BYTEE_VULKAN_FEATURE(VkPhysicalDeviceVulkan12Features, descriptorBindingVariableDescriptorCount),
    BYTEE_VULKAN_FEATURE(VkPhysicalDeviceVulkan12Features, descriptorBindingUpdateUnusedWhilePending),
    BYTEE_VULKAN_FEATURE(VkPhysicalDeviceVulkan12Features, descriptorBindingSampledImageUpdateAfterBind),
    BYTEE_VULKAN_FEATURE(VkPhysicalDeviceVulkan12Features, descriptorBindingStorageImageUpdateAfterBind),
    BYTEE_VULKAN_FEATURE(VkPhysicalDeviceVulkan12Features, descriptorBindingStorageBufferUpdateAfterBind),
    BYTEE_VULKAN_FEATURE(VkPhysicalDeviceVulkan12Features, shaderSampledImageArrayNonUniformIndexing),
    BYTEE_VULKAN_FEATURE(VkPhysicalDeviceVulkan12Features, shaderStorageImageArrayNonUniformIndexing),
    BYTEE_VULKAN_FEATURE(VkPhysicalDeviceVulkan12Features, shaderStorageBufferArrayNonUniformIndexing),
    // Buffers by GPU address; C-like (scalar) data layout.
    BYTEE_VULKAN_FEATURE(VkPhysicalDeviceVulkan12Features, bufferDeviceAddress),
    BYTEE_VULKAN_FEATURE(VkPhysicalDeviceVulkan12Features, scalarBlockLayout),
};

constexpr std::array k_vulkan13_features{
    BYTEE_VULKAN_FEATURE(VkPhysicalDeviceVulkan13Features, dynamicRendering),
    BYTEE_VULKAN_FEATURE(VkPhysicalDeviceVulkan13Features, synchronization2),
};

#undef BYTEE_VULKAN_FEATURE

template <FeatureStruct Struct_, size_t Count_>
void collect_missing(const Struct_& supported, const std::array<FeatureBit<Struct_>, Count_>& required,
                     std::vector<std::string_view>& missing) {
    for (const FeatureBit<Struct_>& feature : required) {
        if (supported.*feature.member != VK_TRUE) {
            missing.push_back(feature.name);
        }
    }
}

template <FeatureStruct Struct_, size_t Count_>
void enable(Struct_& features, const std::array<FeatureBit<Struct_>, Count_>& required) noexcept {
    for (const FeatureBit<Struct_>& feature : required) {
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
    const auto families = enumerate<VkQueueFamilyProperties>(vkGetPhysicalDeviceQueueFamilyProperties, physical_device);
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
    if (missing.empty()) {
        return std::nullopt;
    }
    std::string reason = "missing features:";
    for (const std::string_view name : missing) {
        reason += ' ';
        reason += name;
    }
    return reason;
}

} // namespace bytee::render::rhi::vulkan
