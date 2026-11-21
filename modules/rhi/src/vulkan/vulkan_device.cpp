#include "vulkan/vulkan_device.hpp"

#include "vulkan/vulkan_requirements.hpp"

#include <bytee/core/assert.hpp>
#include <bytee/render/rhi/instance.hpp>

#include <algorithm>
#include <cstring>
#include <format>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace bytee::render::rhi {

namespace {

constexpr const char* k_portability_subset_extension = "VK_KHR_portability_subset"; // not in vulkan_core.h

bool has_extension(const std::vector<VkExtensionProperties>& extensions, const char* name) {
    return std::ranges::any_of(extensions, [name](const VkExtensionProperties& extension) {
        return std::strcmp(extension.extensionName, name) == 0;
    });
}

int adapter_type_rank(AdapterType type) noexcept {
    switch (type) {
    case AdapterType::Discrete: return 3;
    case AdapterType::Integrated: return 2;
    case AdapterType::Virtual: return 1;
    default: return 0;
    }
}

// Best supported adapter: discrete > integrated > virtual > others, then the most device-local memory.
std::optional<uint32_t> select_best_adapter(std::span<const AdapterInfo> adapters) {
    const AdapterInfo* best = nullptr;
    for (const AdapterInfo& adapter : adapters) {
        if (!adapter.is_supported) {
            continue;
        }
        if (best == nullptr || adapter_type_rank(adapter.type) > adapter_type_rank(best->type) ||
            (adapter.type == best->type && adapter.device_local_memory_bytes > best->device_local_memory_bytes)) {
            best = &adapter;
        }
    }
    return best != nullptr ? std::optional<uint32_t>{best->index} : std::nullopt;
}

core::Error no_supported_adapter_error(std::span<const AdapterInfo> adapters) {
    if (adapters.empty()) {
        return core::make_error(core::ErrorCode::NotSupported, "no GPU adapters found");
    }
    std::string reasons;
    for (const AdapterInfo& adapter : adapters) {
        reasons += std::format("\n  {}: {}", adapter.name, adapter.unsupported_reason);
    }
    return core::make_error(core::ErrorCode::NotSupported, "no adapter meets Bytee's requirements:{}", reasons);
}

} // namespace

Device::Impl::~Impl() {
    if (device != VK_NULL_HANDLE) {
        // A destructor cannot report failure; afterwards the GPU is idle unless the device was lost.
        static_cast<void>(vk.vkDeviceWaitIdle(device));
    }
    if (allocator != VK_NULL_HANDLE) {
        vmaDestroyAllocator(allocator);
    }
    if (device != VK_NULL_HANDLE) {
        vk.vkDestroyDevice(device, nullptr);
    }
    if (instance != nullptr && tracker_id != 0) {
        instance->live_devices.remove(tracker_id);
    }
}

core::Error Device::Impl::fail(VkResult result, core::ErrorFormat<> call) {
    if (result == VK_ERROR_DEVICE_LOST && !lost.exchange(true, std::memory_order_acq_rel)) {
        logger->error("device '{}' was lost; it must be recreated", debug_name);
    }
    return vulkan::make_vk_error(result, call);
}

core::Error Device::Impl::lost_error() const {
    return core::make_error(core::ErrorCode::DeviceLost, "device '{}' was lost; it must be recreated", debug_name);
}

Device::Impl* get_impl(const Device& device) noexcept {
    return device.impl_;
}

void destroy(Device::Impl* impl) noexcept {
    if (impl == nullptr) {
        return;
    }
    if (impl->live_objects.count() > 0) {
        impl->live_objects.report_leaks(*impl->logger, std::format("Device '{}'", impl->debug_name));
        BYTEE_ASSERT(false, "every resource must be destroyed before its Device");
        return; // leaked on purpose: a leak is recoverable, a use-after-free is not
    }
    delete impl;
}

Device::~Device() {
    destroy(impl_);
}

Device& Device::operator=(Device&& other) noexcept {
    if (this != &other) {
        destroy(impl_);
        impl_ = std::exchange(other.impl_, nullptr);
    }
    return *this;
}

const AdapterInfo& Device::adapter() const noexcept {
    BYTEE_ASSERT(is_valid(), "Device is empty");
    return impl_->adapter;
}

bool Device::is_presentation_supported() const noexcept {
    BYTEE_ASSERT(is_valid(), "Device is empty");
    return impl_->presentation_supported;
}

bool Device::is_lost() const noexcept {
    BYTEE_ASSERT(is_valid(), "Device is empty");
    return impl_->lost.load(std::memory_order_acquire);
}

core::Result<void> Device::wait_idle() {
    BYTEE_ASSERT(is_valid(), "Device is empty");
    if (impl_->lost.load(std::memory_order_acquire)) {
        return impl_->lost_error();
    }
    if (const VkResult result = impl_->vk.vkDeviceWaitIdle(impl_->device); result != VK_SUCCESS) {
        return impl_->fail(result, "vkDeviceWaitIdle");
    }
    return {};
}

core::Result<Device> Instance::create_device(const DeviceDesc& desc) {
    BYTEE_ASSERT(is_valid(), "Instance is empty");
    Impl& instance = *impl_;

    uint32_t adapter_index = 0;
    if (desc.adapter_index) {
        adapter_index = *desc.adapter_index;
        if (adapter_index >= instance.adapters.size()) {
            return core::make_error(core::ErrorCode::InvalidArgument, "adapter index {} is out of range ({} adapters)",
                                    adapter_index, instance.adapters.size());
        }
        const AdapterInfo& requested = instance.adapters[adapter_index];
        if (!requested.is_supported) {
            return core::make_error(core::ErrorCode::NotSupported, "adapter '{}' is not supported: {}", requested.name,
                                    requested.unsupported_reason);
        }
    } else if (const std::optional<uint32_t> best = select_best_adapter(instance.adapters)) {
        adapter_index = *best;
    } else {
        return no_supported_adapter_error(instance.adapters);
    }

    const AdapterInfo& adapter = instance.adapters[adapter_index];
    VkPhysicalDevice physical_device = instance.physical_devices[adapter_index];
    const std::optional<uint32_t> queue_family = vulkan::find_universal_queue_family(physical_device);
    BYTEE_ASSERT(queue_family.has_value(), "supported adapter '{}' has no universal queue family", adapter.name);

    const auto extensions =
        vulkan::enumerate<VkExtensionProperties>(vkEnumerateDeviceExtensionProperties, physical_device, nullptr);
    std::vector<const char*> enabled_extensions;
    // Mandatory whenever the implementation exposes it (MoltenVK does).
    if (has_extension(extensions, k_portability_subset_extension)) {
        enabled_extensions.push_back(k_portability_subset_extension);
    }
    // Swapchains also need the instance's surface extensions.
    const bool presentation =
        instance.presentation_supported && has_extension(extensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    if (presentation) {
        enabled_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }

    vulkan::DeviceFeatureChain features;
    vulkan::enable_required_features(features);

    const float queue_priority = 1.0f;
    const VkDeviceQueueCreateInfo queue_info{
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = *queue_family,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority,
    };
    const VkDeviceCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &features.features2,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_info,
        .enabledExtensionCount = static_cast<uint32_t>(enabled_extensions.size()),
        .ppEnabledExtensionNames = enabled_extensions.data(),
    };

    auto impl = std::make_unique<Device::Impl>();
    impl->instance = &instance;
    impl->logger = instance.logger;
    impl->debug_name = desc.debug_name;
    impl->adapter = adapter;
    impl->physical_device = physical_device;
    impl->queue_family = *queue_family;
    impl->presentation_supported = presentation;

    if (const VkResult result = vkCreateDevice(physical_device, &create_info, nullptr, &impl->device);
        result != VK_SUCCESS) {
        return vulkan::make_vk_error(result, "vkCreateDevice");
    }
    volkLoadDeviceTable(&impl->vk, impl->device);
    impl->vk.vkGetDeviceQueue(impl->device, impl->queue_family, 0, &impl->queue);

    const VmaVulkanFunctions vma_functions{
        .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
        .vkGetDeviceProcAddr = vkGetDeviceProcAddr,
    };
    const VmaAllocatorCreateInfo allocator_info{
        .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .physicalDevice = physical_device,
        .device = impl->device,
        .pVulkanFunctions = &vma_functions,
        .instance = instance.instance,
        .vulkanApiVersion = vulkan::k_required_api_version,
    };
    if (const VkResult result = vmaCreateAllocator(&allocator_info, &impl->allocator); result != VK_SUCCESS) {
        return vulkan::make_vk_error(result, "vmaCreateAllocator");
    }

    vulkan::set_debug_name(impl->device, VK_OBJECT_TYPE_DEVICE, impl->device, desc.debug_name);
    vulkan::set_debug_name(impl->device, VK_OBJECT_TYPE_QUEUE, impl->queue,
                           std::format("{} universal queue", desc.debug_name));

    impl->tracker_id = instance.live_devices.add("Device", desc.debug_name);
    instance.logger->debug("device '{}' created on {} (queue family {}, presentation {})", desc.debug_name,
                           adapter.name, *queue_family, presentation ? "on" : "off");
    return Device{impl.release()};
}

} // namespace bytee::render::rhi
