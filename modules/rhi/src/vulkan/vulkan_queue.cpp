#include "vulkan/vulkan_queue.hpp"

#include <byte/core/assert.hpp>

#include <format>
#include <limits>
#include <vector>

namespace byte::render::rhi::vulkan {

namespace {

uint64_t to_vulkan_timeout(std::chrono::nanoseconds timeout) noexcept {
    if (timeout <= std::chrono::nanoseconds::zero()) {
        return 0;
    }
    if (timeout == std::chrono::nanoseconds::max()) {
        return std::numeric_limits<uint64_t>::max();
    }
    return static_cast<uint64_t>(timeout.count());
}

} // namespace

Queue::Queue(VkDevice device, const VolkDeviceTable& vk, VkQueue queue, uint32_t family) noexcept
    : m_device(device)
    , m_vk(&vk)
    , m_queue(queue)
    , m_family(family) {}

Queue::~Queue() {
    if (m_timeline != VK_NULL_HANDLE) {
        m_vk->vkDestroySemaphore(m_device, m_timeline, nullptr);
    }
}

void Queue::observe_completed(uint64_t value) const noexcept {
    uint64_t current = m_completed.load(std::memory_order_relaxed);
    while (value > current &&
           !m_completed.compare_exchange_weak(current, value, std::memory_order_release, std::memory_order_relaxed)) {
    }
}

core::Result<uint64_t> Queue::completed() const {
    uint64_t value = 0;
    if (const VkResult result = m_vk->vkGetSemaphoreCounterValue(m_device, m_timeline, &value); result != VK_SUCCESS) {
        return make_vk_error(result, "vkGetSemaphoreCounterValue");
    }
    observe_completed(value);
    return value;
}

core::Result<void> Queue::wait(uint64_t value, std::chrono::nanoseconds timeout) const {
    if (value <= cached_completed()) {
        return {};
    }
    const uint64_t submitted = last_submitted();
    if (value > submitted) {
        return core::make_error(core::ErrorCode::InvalidArgument,
                                "cannot wait for timeline value {}: only {} submitted (would never complete)", value,
                                submitted);
    }

    const VkSemaphoreWaitInfo wait_info{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
        .semaphoreCount = 1,
        .pSemaphores = &m_timeline,
        .pValues = &value,
    };
    const VkResult result = m_vk->vkWaitSemaphores(m_device, &wait_info, to_vulkan_timeout(timeout));
    if (result == VK_TIMEOUT) {
        return core::make_error(core::ErrorCode::Timeout, "timed out waiting for timeline value {}", value);
    }
    if (result != VK_SUCCESS) {
        return make_vk_error(result, "vkWaitSemaphores");
    }
    observe_completed(value);
    return {};
}

core::Result<uint64_t> Queue::submit(std::span<const VkCommandBufferSubmitInfo> command_buffers,
                                     std::span<const VkSemaphoreSubmitInfo> waits,
                                     std::span<const VkSemaphoreSubmitInfo> signals) {
    const std::scoped_lock lock{m_submit_mutex};

    const uint64_t value = m_last_submitted.load(std::memory_order_relaxed) + 1;

    std::vector<VkSemaphoreSubmitInfo> all_signals;
    all_signals.reserve(signals.size() + 1);
    all_signals.assign(signals.begin(), signals.end());
    all_signals.push_back(VkSemaphoreSubmitInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = m_timeline,
        .value = value,
        .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
    });

    const VkSubmitInfo2 submit_info{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .waitSemaphoreInfoCount = static_cast<uint32_t>(waits.size()),
        .pWaitSemaphoreInfos = waits.data(),
        .commandBufferInfoCount = static_cast<uint32_t>(command_buffers.size()),
        .pCommandBufferInfos = command_buffers.data(),
        .signalSemaphoreInfoCount = static_cast<uint32_t>(all_signals.size()),
        .pSignalSemaphoreInfos = all_signals.data(),
    };
    if (const VkResult result = m_vk->vkQueueSubmit2(m_queue, 1, &submit_info, VK_NULL_HANDLE);
        result != VK_SUCCESS) {
        return make_vk_error(result, "vkQueueSubmit2");
    }

    m_last_submitted.store(value, std::memory_order_release);
    return value;
}

core::Result<std::unique_ptr<Queue>> create_queue(VkDevice device, const VolkDeviceTable& vk, uint32_t family,
                                                  uint32_t index, std::string_view debug_name) {
    VkQueue handle = VK_NULL_HANDLE;
    vk.vkGetDeviceQueue(device, family, index, &handle);
    BYTE_RENDER_ASSERT(handle != VK_NULL_HANDLE, "queue {} of family {} was not created with the device", index,
                       family);

    std::unique_ptr<Queue> queue{new Queue{device, vk, handle, family}};

    const VkSemaphoreTypeCreateInfo type_info{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
        .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
        .initialValue = 0,
    };
    const VkSemaphoreCreateInfo semaphore_info{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = &type_info,
    };
    if (const VkResult result = vk.vkCreateSemaphore(device, &semaphore_info, nullptr, &queue->m_timeline);
        result != VK_SUCCESS) {
        return make_vk_error(result, "vkCreateSemaphore (timeline)");
    }

    set_debug_name(device, VK_OBJECT_TYPE_QUEUE, handle, debug_name);
    set_debug_name(device, VK_OBJECT_TYPE_SEMAPHORE, queue->m_timeline, std::format("{} timeline", debug_name));
    return queue;
}

} // namespace byte::render::rhi::vulkan
