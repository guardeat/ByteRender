#pragma once

#include "common/live_object_tracker.hpp"
#include "vulkan/vulkan_common.hpp"
#include "vulkan/vulkan_instance.hpp"

#include <vk_mem_alloc.h>

#include <byte/core/log.hpp>
#include <byte/render/rhi/device.hpp>

namespace byte::render::rhi {

struct Device::Impl {
    Impl() = default;
    ~Impl();
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    Instance::Impl* instance = nullptr; // the creating Instance; outlives the device
    core::Logger* logger = nullptr;
    detail::LiveObjectTracker::Id tracker_id = 0;

    AdapterInfo adapter;
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    // Device-level Vulkan functions for this device only (no process-global device function pointers).
    VolkDeviceTable vk{};
    VmaAllocator allocator = VK_NULL_HANDLE;

    uint32_t graphics_queue_family = 0;
    VkQueue graphics_queue = VK_NULL_HANDLE;
    bool presentation_supported = false;

    // Child objects (buffers, textures, ...) for leak reports.
    detail::LiveObjectTracker live_objects;
};

} // namespace byte::render::rhi
