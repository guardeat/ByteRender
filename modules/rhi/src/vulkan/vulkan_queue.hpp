#pragma once

#include "vulkan/vulkan_common.hpp"

#include <byte/core/result.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <span>
#include <string_view>

namespace byte::render::rhi::vulkan {

// A VkQueue plus the timeline semaphore that tracks its progress. Every submission signals the next
// timeline value; completed() reports how far the GPU has got. Thread-safe.
//
// Not movable (it owns a mutex and atomics); the Device holds queues by unique_ptr and must destroy them
// before the VkDevice.
class Queue {
public:
    ~Queue();
    Queue(const Queue&) = delete;
    Queue& operator=(const Queue&) = delete;

    [[nodiscard]] VkQueue handle() const noexcept { return m_queue; }
    [[nodiscard]] uint32_t family() const noexcept { return m_family; }
    [[nodiscard]] VkSemaphore timeline() const noexcept { return m_timeline; }

    // Value signalled by the most recent successful submission (0 before the first one).
    [[nodiscard]] uint64_t last_submitted() const noexcept { return m_last_submitted.load(std::memory_order_acquire); }

    // Last completed value observed by completed()/wait(), without querying the GPU.
    [[nodiscard]] uint64_t cached_completed() const noexcept { return m_completed.load(std::memory_order_acquire); }

    // Queries the GPU for the current completed value.
    [[nodiscard]] core::Result<uint64_t> completed() const;

    // Blocks until `value` is complete or the timeout expires (core::ErrorCode::Timeout).
    // Waiting for a value that has not been submitted is an error (it could never complete).
    [[nodiscard]] core::Result<void> wait(uint64_t value, std::chrono::nanoseconds timeout) const;

    // Submits command buffers and returns the timeline value that is signalled when they complete.
    // The timeline signal is appended to `signals` automatically. An empty submission only advances the timeline.
    [[nodiscard]] core::Result<uint64_t> submit(std::span<const VkCommandBufferSubmitInfo> command_buffers,
                                                std::span<const VkSemaphoreSubmitInfo> waits = {},
                                                std::span<const VkSemaphoreSubmitInfo> signals = {});

private:
    friend core::Result<std::unique_ptr<Queue>> create_queue(VkDevice device, const VolkDeviceTable& vk,
                                                             uint32_t family, uint32_t index,
                                                             std::string_view debug_name);

    Queue(VkDevice device, const VolkDeviceTable& vk, VkQueue queue, uint32_t family) noexcept;

    void observe_completed(uint64_t value) const noexcept;

    VkDevice m_device = VK_NULL_HANDLE;
    const VolkDeviceTable* m_vk = nullptr;
    VkQueue m_queue = VK_NULL_HANDLE;
    uint32_t m_family = 0;
    VkSemaphore m_timeline = VK_NULL_HANDLE;

    std::mutex m_submit_mutex; // vkQueueSubmit2 requires external synchronization of the queue
    std::atomic<uint64_t> m_last_submitted{0};
    mutable std::atomic<uint64_t> m_completed{0};
};

// Gets queue `index` of `family` and creates its timeline semaphore.
[[nodiscard]] core::Result<std::unique_ptr<Queue>> create_queue(VkDevice device, const VolkDeviceTable& vk,
                                                                uint32_t family, uint32_t index,
                                                                std::string_view debug_name);

} // namespace byte::render::rhi::vulkan
