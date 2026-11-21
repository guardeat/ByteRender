#pragma once

#include "common/live_object_tracker.hpp"
#include "common/retire_list.hpp"
#include "vulkan/vulkan_common.hpp"
#include "vulkan/vulkan_instance.hpp"
#include "vulkan/vulkan_queue.hpp"

#include <bytee/core/log.hpp>
#include <bytee/render/rhi/device.hpp>

#include <vk_mem_alloc.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <memory>
#include <span>
#include <string>

namespace bytee::render::rhi {

namespace vulkan {

inline constexpr std::size_t k_queue_count = 1; // one per QueueType
using DeviceQueueValues = detail::QueueValues<k_queue_count>;

// A backend-internal Vulkan object waiting for the GPU to finish with it. Resource kinds with their own pools get
// their own typed retire lists instead (step 7).
struct RetiredObject {
    VkObjectType type = VK_OBJECT_TYPE_UNKNOWN;
    uint64_t handle = 0;
};

[[nodiscard]] uint64_t to_timeout_nanoseconds(std::chrono::nanoseconds timeout) noexcept;

} // namespace vulkan

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

    // Indexed by QueueType. Destroyed explicitly before the VkDevice.
    std::array<std::unique_ptr<vulkan::Queue>, vulkan::k_queue_count> queues;
    bool presentation_supported = false;

    std::atomic<bool> lost{false}; // sticky: set by the first VK_ERROR_DEVICE_LOST

    detail::RetireList<vulkan::RetiredObject, vulkan::k_queue_count> retired_objects;
    detail::LiveObjectTracker live_objects; // child resources, for leak reports

    Impl() = default;
    ~Impl();
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    // Error for a failed call; VK_ERROR_DEVICE_LOST also marks the device lost (permanently).
    [[nodiscard]] core::Error fail(VkResult result, core::ErrorFormat<> call);
    // The DeviceLost error returned by every fallible call once the device is lost.
    [[nodiscard]] core::Error lost_error() const;

    [[nodiscard]] vulkan::Queue& queue(QueueType type) const noexcept {
        return *queues[static_cast<std::size_t>(type)];
    }

    // Every queue's last submitted value: the tag for objects released now.
    [[nodiscard]] vulkan::DeviceQueueValues last_submitted() const noexcept;
    // Every queue's highest observed completed value (no GPU query).
    [[nodiscard]] vulkan::DeviceQueueValues cached_completed() const noexcept;

    // Submits to a queue and returns the SyncPoint that completes with it; then collects retired objects.
    [[nodiscard]] core::Result<SyncPoint> submit(QueueType type,
                                                 std::span<const VkCommandBufferSubmitInfo> command_buffers,
                                                 std::span<const VkSemaphoreSubmitInfo> waits = {},
                                                 std::span<const VkSemaphoreSubmitInfo> signals = {});

    // Destroys `object` once all GPU work submitted so far has completed: immediately if the GPU is already past it,
    // otherwise at a later collect(). Thread-safe. Retiring an object still referenced by an unsubmitted command
    // list is a programmer error.
    void retire(vulkan::RetiredObject object);

    // Queries GPU progress and destroys every retired object whose work has completed.
    [[nodiscard]] core::Result<void> collect();

    void destroy_now(const vulkan::RetiredObject& object) const noexcept;
};

[[nodiscard]] Device::Impl* get_impl(const Device& device) noexcept;

// Destroys a Device implementation. Live child resources are a programmer error: reported by name, assert in Debug,
// and the implementation is deliberately leaked (the resources still use it).
void destroy(Device::Impl* impl) noexcept;

} // namespace bytee::render::rhi
