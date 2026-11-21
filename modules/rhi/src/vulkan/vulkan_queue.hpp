#pragma once

#include "vulkan/vulkan_common.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <span>
#include <string_view>

namespace bytee::render::rhi::vulkan {

// A VkQueue plus the timeline semaphore that tracks its progress: every submission signals the next value
// (1, 2, 3, ...). Thread-safe. Returns raw VkResults: the owning Device turns them into errors (and notices device
// loss). Not movable (mutex, atomics): the Device holds queues by unique_ptr and destroys them before the VkDevice.
class Queue {
private:
    VkDevice device_ = VK_NULL_HANDLE;
    const VolkDeviceTable* vk_ = nullptr;
    VkQueue queue_ = VK_NULL_HANDLE;
    uint32_t family_ = 0;
    VkSemaphore timeline_ = VK_NULL_HANDLE;

    std::mutex submit_mutex_; // vkQueueSubmit2 requires external synchronization of the queue
    std::atomic<uint64_t> last_submitted_{0};
    mutable std::atomic<uint64_t> completed_{0}; // highest completed value observed so far

public:
    ~Queue();
    Queue(const Queue&) = delete;
    Queue& operator=(const Queue&) = delete;

    [[nodiscard]] VkQueue handle() const noexcept { return queue_; }
    [[nodiscard]] uint32_t family() const noexcept { return family_; }
    [[nodiscard]] VkSemaphore timeline() const noexcept { return timeline_; }

    // Value signalled by the most recent successful submission (0 before the first one).
    [[nodiscard]] uint64_t last_submitted() const noexcept { return last_submitted_.load(std::memory_order_acquire); }
    // Highest completed value observed by query_completed()/wait(); never queries the GPU.
    [[nodiscard]] uint64_t cached_completed() const noexcept { return completed_.load(std::memory_order_acquire); }

    // Queries the GPU for the completed value.
    [[nodiscard]] VkResult query_completed(uint64_t& value) const;

    // Blocks until `value` is complete or the timeout expires (VK_TIMEOUT). `value` must have been submitted.
    [[nodiscard]] VkResult wait(uint64_t value, uint64_t timeout_nanoseconds) const;

    // Submits command buffers, appending the timeline signal; on success `signalled` is the new timeline value.
    // An empty submission only advances the timeline.
    [[nodiscard]] VkResult submit(std::span<const VkCommandBufferSubmitInfo> command_buffers,
                                  std::span<const VkSemaphoreSubmitInfo> waits,
                                  std::span<const VkSemaphoreSubmitInfo> signals, uint64_t& signalled);

private:
    friend VkResult create_queue(VkDevice device, const VolkDeviceTable& vk, uint32_t family, uint32_t index,
                                 std::string_view debug_name, std::unique_ptr<Queue>& queue);

    Queue(VkDevice device, const VolkDeviceTable& vk, VkQueue queue, uint32_t family) noexcept;

    void observe_completed(uint64_t value) const noexcept;
};

// Gets queue `index` of `family` and creates its timeline semaphore.
[[nodiscard]] VkResult create_queue(VkDevice device, const VolkDeviceTable& vk, uint32_t family, uint32_t index,
                                    std::string_view debug_name, std::unique_ptr<Queue>& queue);

} // namespace bytee::render::rhi::vulkan
