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

namespace vulkan {

uint64_t to_timeout_nanoseconds(std::chrono::nanoseconds timeout) noexcept {
    if (timeout <= std::chrono::nanoseconds::zero()) {
        return 0;
    }
    if (timeout == std::chrono::nanoseconds::max()) {
        return UINT64_MAX; // wait forever
    }
    return static_cast<uint64_t>(timeout.count());
}

} // namespace vulkan

detail::DeviceImpl::DeviceImpl(const DeviceLimits& limits)
    : buffers(limits.max_buffers)
    , textures(limits.max_textures)
    , graphics_pipelines(limits.max_graphics_pipelines) {}

detail::DeviceImpl::~DeviceImpl() {
    finish_pending_work();
    destroy_contexts();
    if (pipeline_layout != VK_NULL_HANDLE) {
        vk.vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
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
    if (instance != nullptr && tracker_id != 0) {
        instance->live_devices.remove(tracker_id);
    }
}

void detail::DeviceImpl::finish_pending_work() noexcept {
    if (device == VK_NULL_HANDLE) {
        return;
    }
    // Nothing can be reported from here; afterwards the GPU is idle unless the device was lost.
    static_cast<void>(vk.vkDeviceWaitIdle(device));
    retired_buffers.drain([this](BufferHandle buffer) { destroy_now(buffer); });
    retired_textures.drain([this](TextureHandle texture) { destroy_now(texture); });
    retired_graphics_pipelines.drain([this](GraphicsPipelineHandle pipeline) { destroy_now(pipeline); });
    retired_objects.drain([this](const vulkan::RetiredObject& object) { destroy_now(object); });
}

core::Error detail::DeviceImpl::fail(VkResult result, core::ErrorFormat<> call) {
    if (result == VK_ERROR_DEVICE_LOST && !lost.exchange(true, std::memory_order_acq_rel)) {
        logger->error("device '{}' was lost; it must be recreated", debug_name);
    }
    return vulkan::make_vk_error(result, call);
}

core::Error detail::DeviceImpl::lost_error() const {
    return core::make_error(core::ErrorCode::DeviceLost, "device '{}' was lost; it must be recreated", debug_name);
}

vulkan::DeviceQueueValues detail::DeviceImpl::last_submitted() const noexcept {
    vulkan::DeviceQueueValues values{};
    for (std::size_t index = 0; index < vulkan::k_queue_count; ++index) {
        values[index] = queues[index]->last_submitted();
    }
    return values;
}

vulkan::DeviceQueueValues detail::DeviceImpl::cached_completed() const noexcept {
    vulkan::DeviceQueueValues values{};
    for (std::size_t index = 0; index < vulkan::k_queue_count; ++index) {
        values[index] = queues[index]->cached_completed();
    }
    return values;
}

core::Result<SyncPoint> detail::DeviceImpl::submit(QueueType type,
                                                   std::span<const VkCommandBufferSubmitInfo> command_buffers,
                                                   std::span<const VkSemaphoreSubmitInfo> waits,
                                                   std::span<const VkSemaphoreSubmitInfo> signals) {
    if (lost.load(std::memory_order_acquire)) {
        return lost_error();
    }
    uint64_t value = 0;
    if (const VkResult result = queue(type).submit(command_buffers, waits, signals, value); result != VK_SUCCESS) {
        return fail(result, "vkQueueSubmit2");
    }
    BYTEE_TRY(collect());
    return SyncPoint{.queue = type, .value = value};
}

void detail::DeviceImpl::retire(vulkan::RetiredObject object) {
    const vulkan::DeviceQueueValues retire_after = last_submitted();
    // Read after last_submitted(): completed values only grow, so this never over-reports progress.
    if (detail::is_reached(retire_after, cached_completed())) {
        destroy_now(object);
        return;
    }
    retired_objects.push(retire_after, object);
}

core::Result<void> detail::DeviceImpl::collect() {
    vulkan::DeviceQueueValues completed{};
    for (std::size_t index = 0; index < vulkan::k_queue_count; ++index) {
        if (const VkResult result = queues[index]->query_completed(completed[index]); result != VK_SUCCESS) {
            return fail(result, "vkGetSemaphoreCounterValue");
        }
    }
    retired_buffers.collect(completed, [this](BufferHandle buffer) { destroy_now(buffer); });
    retired_textures.collect(completed, [this](TextureHandle texture) { destroy_now(texture); });
    retired_graphics_pipelines.collect(completed, [this](GraphicsPipelineHandle pipeline) { destroy_now(pipeline); });
    retired_objects.collect(completed, [this](const vulkan::RetiredObject& object) { destroy_now(object); });
    return {};
}

void detail::DeviceImpl::destroy_now(const vulkan::RetiredObject& object) const noexcept {
    switch (object.type) {
    case VK_OBJECT_TYPE_FENCE: vk.vkDestroyFence(device, reinterpret_cast<VkFence>(object.handle), nullptr); return;
    case VK_OBJECT_TYPE_SEMAPHORE:
        vk.vkDestroySemaphore(device, reinterpret_cast<VkSemaphore>(object.handle), nullptr);
        return;
    default: BYTEE_ASSERT(false, "retired object of unsupported type {}", static_cast<int>(object.type)); return;
    }
}

detail::DeviceImpl* get_impl(const Device& device) noexcept {
    return device.impl_;
}

void destroy(detail::DeviceImpl* impl) noexcept {
    if (impl == nullptr) {
        return;
    }
    impl->finish_pending_work(); // retired-but-pending resources are not leaks
    if (const uint32_t lists = impl->live_command_lists.load(std::memory_order_acquire); lists > 0) {
        impl->logger->error("Device '{}' destroyed while {} CommandList(s) are alive", impl->debug_name, lists);
        BYTEE_ASSERT(false, "every CommandList must be submitted or destroyed before its Device");
        return; // leaked on purpose: the command lists still use it
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

bool Device::is_complete(SyncPoint point) const {
    BYTEE_ASSERT(is_valid(), "Device is empty");
    const vulkan::Queue& queue = impl_->queue(point.queue);
    if (point.value <= queue.cached_completed()) {
        return true;
    }
    if (point.value > queue.last_submitted() || impl_->lost.load(std::memory_order_acquire)) {
        return false;
    }
    uint64_t completed = 0;
    if (const VkResult result = queue.query_completed(completed); result != VK_SUCCESS) {
        static_cast<void>(impl_->fail(result, "vkGetSemaphoreCounterValue")); // marks a lost device
        return false;
    }
    return point.value <= completed;
}

core::Result<void> Device::wait(SyncPoint point, std::chrono::nanoseconds timeout) {
    BYTEE_ASSERT(is_valid(), "Device is empty");
    if (impl_->lost.load(std::memory_order_acquire)) {
        return impl_->lost_error();
    }
    const vulkan::Queue& queue = impl_->queue(point.queue);
    if (point.value > queue.last_submitted()) {
        return core::make_error(core::ErrorCode::InvalidArgument,
                                "cannot wait for {} queue value {}: only {} submitted (it would never complete)",
                                to_string(point.queue), point.value, queue.last_submitted());
    }
    const VkResult result = queue.wait(point.value, vulkan::to_timeout_nanoseconds(timeout));
    if (result == VK_TIMEOUT) {
        return core::Error{core::ErrorCode::Timeout}; // allocation-free: callers may poll with short timeouts
    }
    if (result != VK_SUCCESS) {
        return impl_->fail(result, "vkWaitSemaphores");
    }
    return impl_->collect();
}

core::Result<void> Device::wait_idle() {
    BYTEE_ASSERT(is_valid(), "Device is empty");
    if (impl_->lost.load(std::memory_order_acquire)) {
        return impl_->lost_error();
    }
    if (const VkResult result = impl_->vk.vkDeviceWaitIdle(impl_->device); result != VK_SUCCESS) {
        return impl_->fail(result, "vkDeviceWaitIdle");
    }
    return impl_->collect();
}

core::Result<Device> Instance::create_device(const DeviceDesc& desc) {
    BYTEE_ASSERT(is_valid(), "Instance is empty");
    detail::InstanceImpl& instance = *impl_;

    if (desc.limits.max_buffers == 0 || desc.limits.max_textures == 0 || desc.limits.max_graphics_pipelines == 0) {
        return core::make_error(core::ErrorCode::InvalidArgument, "device '{}': resource limits must not be 0",
                                desc.debug_name);
    }

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

    auto impl = std::make_unique<detail::DeviceImpl>(desc.limits);
    impl->instance = &instance;
    impl->logger = instance.logger;
    impl->debug_name = desc.debug_name;
    impl->adapter = adapter;
    impl->physical_device = physical_device;
    impl->presentation_supported = presentation;

    if (const VkResult result = vkCreateDevice(physical_device, &create_info, nullptr, &impl->device);
        result != VK_SUCCESS) {
        return vulkan::make_vk_error(result, "vkCreateDevice");
    }
    volkLoadDeviceTable(&impl->vk, impl->device);
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physical_device, &properties);
    impl->device_limits = properties.limits;

    std::unique_ptr<vulkan::Queue>& universal_queue = impl->queues[static_cast<std::size_t>(QueueType::Graphics)];
    if (const VkResult result =
            vulkan::create_queue(impl->device, impl->vk, *queue_family, 0,
                                 std::format("{} universal queue", desc.debug_name), universal_queue);
        result != VK_SUCCESS) {
        return vulkan::make_vk_error(result, "vkCreateSemaphore (queue timeline)");
    }

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

    const VkPushConstantRange push_constants{
        .stageFlags = VK_SHADER_STAGE_ALL,
        .offset = 0,
        .size = k_max_push_constant_bytes,
    };
    const VkPipelineLayoutCreateInfo layout_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_constants,
    };
    if (const VkResult result =
            impl->vk.vkCreatePipelineLayout(impl->device, &layout_info, nullptr, &impl->pipeline_layout);
        result != VK_SUCCESS) {
        return vulkan::make_vk_error(result, "vkCreatePipelineLayout");
    }

    vulkan::set_debug_name(impl->device, VK_OBJECT_TYPE_DEVICE, impl->device, desc.debug_name);
    vulkan::set_debug_name(impl->device, VK_OBJECT_TYPE_PIPELINE_LAYOUT, impl->pipeline_layout,
                           std::format("{} pipeline layout", desc.debug_name));

    impl->tracker_id = instance.live_devices.add("Device", desc.debug_name);
    instance.logger->debug("device '{}' created on {} (queue family {}, presentation {})", desc.debug_name,
                           adapter.name, *queue_family, presentation ? "on" : "off");
    return Device{impl.release()};
}

} // namespace bytee::render::rhi
