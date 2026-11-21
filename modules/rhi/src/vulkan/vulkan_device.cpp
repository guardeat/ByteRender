#include "vulkan/vulkan_device.hpp"

#include "vulkan/vulkan_device_requirements.hpp"

#include <byte/core/assert.hpp>
#include <byte/render/rhi/instance.hpp>

#include <algorithm>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace byte::render::rhi {

namespace {

bool has_extension(const std::vector<VkExtensionProperties>& extensions, const char* name) {
    return std::ranges::any_of(extensions, [name](const VkExtensionProperties& extension) {
        return std::strcmp(extension.extensionName, name) == 0;
    });
}

int adapter_type_rank(AdapterType type) {
    switch (type) {
    case AdapterType::Discrete: return 3;
    case AdapterType::Integrated: return 2;
    case AdapterType::Virtual: return 1;
    default: return 0;
    }
}

// Best supported adapter: discrete > integrated > virtual > others, then most device-local memory.
std::optional<uint32_t> select_best_adapter(std::span<const AdapterInfo> adapters) {
    const AdapterInfo* best = nullptr;
    for (const AdapterInfo& adapter : adapters) {
        if (!adapter.is_supported) {
            continue;
        }
        if (!best || adapter_type_rank(adapter.type) > adapter_type_rank(best->type) ||
            (adapter.type == best->type && adapter.device_local_memory_bytes > best->device_local_memory_bytes)) {
            best = &adapter;
        }
    }
    return best ? std::optional<uint32_t>{best->index} : std::nullopt;
}

core::Error no_supported_adapter_error(std::span<const AdapterInfo> adapters) {
    if (adapters.empty()) {
        return core::make_error(core::ErrorCode::NotSupported, "no GPU adapters found");
    }
    std::string reasons;
    for (const AdapterInfo& adapter : adapters) {
        reasons += std::format("\n  {}: {}", adapter.name, adapter.unsupported_reason);
    }
    return core::make_error(core::ErrorCode::NotSupported, "no adapter meets ByteRender's requirements:{}",
                            reasons);
}

} // namespace

Device::Impl::~Impl() {
    if (device != VK_NULL_HANDLE) {
        // Nothing can be reported from a destructor; the GPU is idle afterwards unless the device was lost.
        [[maybe_unused]] const VkResult idle = vk.vkDeviceWaitIdle(device);
        retire_queue.drain();
    }
    if (live_objects.count() > 0) {
        live_objects.report_leaks(*logger, "Device");
        BYTE_RENDER_ASSERT(false, "all resources must be destroyed before their Device");
    }
    for (std::unique_ptr<vulkan::Queue>& queue_ptr : queues) {
        queue_ptr.reset();
    }
    if (allocator != VK_NULL_HANDLE) {
        vmaDestroyAllocator(allocator);
    }
    if (device != VK_NULL_HANDLE) {
        vk.vkDestroyDevice(device, nullptr);
    }
    if (instance && tracker_id != 0) {
        instance->live_devices.remove(tracker_id);
    }
}

void Device::Impl::retire(std::function<void()> destroy) {
    vulkan::DeviceQueueValues retire_after{};
    vulkan::DeviceQueueValues completed{};
    for (size_t index = 0; index < vulkan::k_queue_count; ++index) {
        retire_after[index] = queues[index]->last_submitted();
        // Read after last_submitted(): completed values only grow, so this never over-reports progress.
        completed[index] = queues[index]->cached_completed();
    }
    if (detail::is_reached(retire_after, completed)) {
        destroy();
        return;
    }
    retire_queue.push(retire_after, std::move(destroy));
}

core::Result<void> Device::Impl::collect() {
    vulkan::DeviceQueueValues completed{};
    for (size_t index = 0; index < vulkan::k_queue_count; ++index) {
        core::Result<uint64_t> value = queues[index]->completed();
        if (!value) {
            return std::move(value).error();
        }
        completed[index] = *value;
    }
    retire_queue.collect(completed);
    return {};
}

Device::Impl* get_impl(const Device& device) noexcept {
    return device.m_impl;
}

Device::~Device() {
    delete m_impl;
}

Device& Device::operator=(Device&& other) noexcept {
    if (this != &other) {
        delete m_impl;
        m_impl = std::exchange(other.m_impl, nullptr);
    }
    return *this;
}

const AdapterInfo& Device::adapter() const noexcept {
    BYTE_RENDER_ASSERT(is_valid(), "Device is empty");
    return m_impl->adapter;
}

bool Device::supports_presentation() const noexcept {
    BYTE_RENDER_ASSERT(is_valid(), "Device is empty");
    return m_impl->presentation_supported;
}

bool Device::is_complete(SyncPoint point) const {
    BYTE_RENDER_ASSERT(is_valid(), "Device is empty");
    const vulkan::Queue& queue = m_impl->queue(point.queue);
    if (point.value <= queue.cached_completed()) {
        return true;
    }
    const core::Result<uint64_t> completed = queue.completed();
    if (!completed) {
        m_impl->logger->error("{}", completed.error().message);
        return false;
    }
    return point.value <= *completed;
}

core::Result<void> Device::wait(SyncPoint point, std::chrono::nanoseconds timeout) {
    BYTE_RENDER_ASSERT(is_valid(), "Device is empty");
    if (auto waited = m_impl->queue(point.queue).wait(point.value, timeout); !waited) {
        return waited;
    }
    return m_impl->collect();
}

core::Result<void> Device::wait_idle() {
    BYTE_RENDER_ASSERT(is_valid(), "Device is empty");
    if (const VkResult result = m_impl->vk.vkDeviceWaitIdle(m_impl->device); result != VK_SUCCESS) {
        return vulkan::make_vk_error(result, "vkDeviceWaitIdle");
    }
    return m_impl->collect();
}

core::Result<Device> Instance::create_device(const DeviceDesc& desc) {
    BYTE_RENDER_ASSERT(is_valid(), "Instance is empty");
    Impl& instance = *m_impl;

    uint32_t adapter_index = 0;
    if (desc.adapter_index) {
        adapter_index = *desc.adapter_index;
        if (adapter_index >= instance.adapters.size()) {
            return core::make_error(core::ErrorCode::InvalidArgument, "adapter index {} out of range ({} adapters)",
                                    adapter_index, instance.adapters.size());
        }
        const AdapterInfo& adapter = instance.adapters[adapter_index];
        if (!adapter.is_supported) {
            return core::make_error(core::ErrorCode::NotSupported, "adapter '{}' is not supported: {}",
                                    adapter.name, adapter.unsupported_reason);
        }
    } else if (auto best = select_best_adapter(instance.adapters)) {
        adapter_index = *best;
    } else {
        return no_supported_adapter_error(instance.adapters);
    }

    const AdapterInfo& adapter = instance.adapters[adapter_index];
    VkPhysicalDevice physical_device = instance.physical_devices[adapter_index];

    const std::optional<uint32_t> queue_family = vulkan::find_universal_queue_family(physical_device);
    BYTE_RENDER_ASSERT(queue_family.has_value(), "supported adapter without a universal queue family");

    const auto extensions =
        vulkan::enumerate<VkExtensionProperties>(vkEnumerateDeviceExtensionProperties, physical_device, nullptr);
    std::vector<const char*> enabled_extensions;
    // Required by the spec whenever the implementation exposes it (MoltenVK does).
    if (has_extension(extensions, "VK_KHR_portability_subset")) {
        enabled_extensions.push_back("VK_KHR_portability_subset");
    }
    // Swapchains need the instance's surface extensions as well.
    const bool presentation =
        instance.surface_supported && has_extension(extensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME);
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
    impl->adapter = adapter;
    impl->physical_device = physical_device;
    impl->presentation_supported = presentation;

    if (const VkResult result = vkCreateDevice(physical_device, &create_info, nullptr, &impl->device);
        result != VK_SUCCESS) {
        return vulkan::make_vk_error(result, "vkCreateDevice");
    }
    volkLoadDeviceTable(&impl->vk, impl->device);

    core::Result<std::unique_ptr<vulkan::Queue>> graphics_queue = vulkan::create_queue(
        impl->device, impl->vk, *queue_family, 0, std::format("{} graphics queue", desc.debug_name));
    if (!graphics_queue) {
        return std::move(graphics_queue).error();
    }
    impl->queues[static_cast<size_t>(QueueType::Graphics)] = std::move(graphics_queue).value();

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

    impl->tracker_id = instance.live_devices.add("Device", desc.debug_name);

    instance.logger->debug("device '{}' created on {} (queue family {}, presentation {})", desc.debug_name,
                           adapter.name, *queue_family, presentation ? "yes" : "no");
    return Device{impl.release()};
}

} // namespace byte::render::rhi
