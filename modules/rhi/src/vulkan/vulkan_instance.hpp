#pragma once

#include "common/live_object_tracker.hpp"
#include "vulkan/vulkan_common.hpp"

#include <bytee/core/log.hpp>
#include <bytee/render/rhi/instance.hpp>

#include <vector>

namespace bytee::render::rhi {

struct BYTEE_HIDDEN Instance::Impl {
    core::Logger* logger = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debug_messenger = VK_NULL_HANDLE;
    bool validation_enabled = false;
    bool presentation_supported = false; // VK_KHR_surface + a platform surface extension are enabled

    // Same order and size: adapters[i] describes physical_devices[i].
    std::vector<VkPhysicalDevice> physical_devices;
    std::vector<AdapterInfo> adapters;

    detail::LiveObjectTracker live_devices;

    Impl() = default;
    ~Impl();
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
};

[[nodiscard]] Instance::Impl* get_impl(const Instance& instance) noexcept;

// Destroys an Instance implementation. If Devices are still alive this is a programmer error: it is reported by name
// and asserts in Debug; otherwise the implementation is deliberately leaked, because the devices still use it.
void destroy(Instance::Impl* impl) noexcept;

} // namespace bytee::render::rhi
