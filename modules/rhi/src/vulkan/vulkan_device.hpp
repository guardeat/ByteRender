#pragma once

#include "common/live_object_tracker.hpp"
#include "common/retire_queue.hpp"
#include "vulkan/vulkan_common.hpp"
#include "vulkan/vulkan_instance.hpp"
#include "vulkan/vulkan_queue.hpp"

#include <vk_mem_alloc.h>

#include <byte/core/log.hpp>
#include <byte/render/rhi/device.hpp>

#include <array>
#include <cstddef>
#include <functional>
#include <memory>

namespace byte::render::rhi {

namespace vulkan {
inline constexpr size_t k_queue_count = 1; // one per QueueType
using DeviceQueueValues = detail::QueueValues<k_queue_count>;
} // namespace vulkan

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

    // Indexed by QueueType. Destroyed explicitly before the VkDevice.
    std::array<std::unique_ptr<vulkan::Queue>, vulkan::k_queue_count> queues;
    bool presentation_supported = false;

    // GPU objects whose CPU owners are gone, waiting for the GPU to finish with them (ARCHITECTURE §5).
    detail::RetireQueue<vulkan::k_queue_count> retire_queue;

    // Child objects (buffers, textures, ...) for leak reports.
    detail::LiveObjectTracker live_objects;

    [[nodiscard]] vulkan::Queue& queue(QueueType type) const noexcept { return *queues[static_cast<size_t>(type)]; }

    // Destroys `destroy`'s object once all GPU work submitted so far has completed: immediately if the GPU is
    // already past it, otherwise at a later collect(). Thread-safe.
    //
    // Destroying an object that is referenced by a command list which has not been submitted yet is a
    // programmer error (the submission would use a destroyed object).
    void retire(std::function<void()> destroy);

    // Runs the destructors of retired objects whose GPU work has completed.
    [[nodiscard]] core::Result<void> collect();
};

// Implementation behind a public Device (friend of rhi::Device).
[[nodiscard]] Device::Impl* get_impl(const Device& device) noexcept;

} // namespace byte::render::rhi
