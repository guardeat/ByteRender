#pragma once

#include "common/live_object_tracker.hpp"
#include "vulkan/vulkan_common.hpp"

#include <byte/core/log.hpp>
#include <byte/render/rhi/instance.hpp>

#include <vector>

namespace byte::render::rhi {

struct Instance::Impl {
    Impl() = default;
    ~Impl();
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    core::Logger* logger = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debug_messenger = VK_NULL_HANDLE;
    bool validation_enabled = false;
    bool surface_supported = false; // VK_KHR_surface + a platform surface extension are enabled

    // Same order and size: adapters[i] describes physical_devices[i].
    std::vector<VkPhysicalDevice> physical_devices;
    std::vector<AdapterInfo> adapters;

    detail::LiveObjectTracker live_devices;
};

} // namespace byte::render::rhi
