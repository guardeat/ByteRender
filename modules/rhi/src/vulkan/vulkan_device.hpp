#pragma once

#include "common/live_object_tracker.hpp"
#include "vulkan/vulkan_common.hpp"
#include "vulkan/vulkan_instance.hpp"

#include <bytee/core/log.hpp>
#include <bytee/render/rhi/device.hpp>

#include <vk_mem_alloc.h>

#include <atomic>

namespace bytee::render::rhi {

struct BYTEE_HIDDEN Device::Impl {
    Instance::Impl* instance = nullptr; // the creating instance; outlives the device
    core::Logger* logger = nullptr;
    detail::LiveObjectTracker::Id tracker_id = 0; // entry in instance->live_devices
    std::string debug_name;

    AdapterInfo adapter;
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VolkDeviceTable vk{}; // device-level functions for this device only: no loader dispatch, no globals
    VmaAllocator allocator = VK_NULL_HANDLE;

    uint32_t queue_family = 0;
    VkQueue queue = VK_NULL_HANDLE; // universal (graphics + compute + transfer)
    bool presentation_supported = false;

    std::atomic<bool> lost{false}; // sticky: set by the first VK_ERROR_DEVICE_LOST

    detail::LiveObjectTracker live_objects; // child resources, for leak reports

    Impl() = default;
    ~Impl();
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    // Error for a failed call; VK_ERROR_DEVICE_LOST also marks the device lost (permanently).
    [[nodiscard]] core::Error fail(VkResult result, core::ErrorFormat<> call);
    // The DeviceLost error returned by every fallible call once the device is lost.
    [[nodiscard]] core::Error lost_error() const;
};

[[nodiscard]] Device::Impl* get_impl(const Device& device) noexcept;

// Destroys a Device implementation. Live child resources are a programmer error: reported by name, assert in Debug,
// and the implementation is deliberately leaked (the resources still use it).
void destroy(Device::Impl* impl) noexcept;

} // namespace bytee::render::rhi
