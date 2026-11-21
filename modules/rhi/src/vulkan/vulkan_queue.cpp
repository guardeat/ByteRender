#include "vulkan/vulkan_queue.hpp"

#include <bytee/core/assert.hpp>

#include <algorithm>
#include <array>
#include <format>
#include <vector>

namespace bytee::render::rhi::vulkan {

Queue::Queue(VkDevice device, const VolkDeviceTable& vk, VkQueue queue, uint32_t family) noexcept
    : device_(device)
    , vk_(&vk)
    , queue_(queue)
    , family_(family) {}

Queue::~Queue() {
    if (timeline_ != VK_NULL_HANDLE) {
        vk_->vkDestroySemaphore(device_, timeline_, nullptr);
    }
}

void Queue::observe_completed(uint64_t value) const noexcept {
    uint64_t current = completed_.load(std::memory_order_relaxed);
    while (value > current &&
           !completed_.compare_exchange_weak(current, value, std::memory_order_release, std::memory_order_relaxed)) {
    }
}

VkResult Queue::query_completed(uint64_t& value) const {
    const VkResult result = vk_->vkGetSemaphoreCounterValue(device_, timeline_, &value);
    if (result == VK_SUCCESS) {
        observe_completed(value);
    }
    return result;
}

VkResult Queue::wait(uint64_t value, uint64_t timeout_nanoseconds) const {
    BYTEE_ASSERT(value <= last_submitted(), "waiting for timeline value {} but only {} submitted", value,
                 last_submitted());
    if (value <= cached_completed()) {
        return VK_SUCCESS;
    }
    const VkSemaphoreWaitInfo wait_info{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
        .semaphoreCount = 1,
        .pSemaphores = &timeline_,
        .pValues = &value,
    };
    const VkResult result = vk_->vkWaitSemaphores(device_, &wait_info, timeout_nanoseconds);
    if (result == VK_SUCCESS) {
        observe_completed(value);
    }
    return result;
}

VkResult Queue::submit(std::span<const VkCommandBufferSubmitInfo> command_buffers,
                       std::span<const VkSemaphoreSubmitInfo> waits, std::span<const VkSemaphoreSubmitInfo> signals,
                       uint64_t& signalled) {
    // Submissions carry at most a handful of signals; avoid a heap allocation for the common case.
    constexpr std::size_t k_inline_signals = 8;
    const std::scoped_lock lock{submit_mutex_};
    const uint64_t value = last_submitted_.load(std::memory_order_relaxed) + 1;

    const VkSemaphoreSubmitInfo timeline_signal{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = timeline_,
        .value = value,
        .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
    };
    std::array<VkSemaphoreSubmitInfo, k_inline_signals> inline_signals{};
    std::vector<VkSemaphoreSubmitInfo> heap_signals;
    std::span<VkSemaphoreSubmitInfo> all_signals;
    if (signals.size() < k_inline_signals) {
        std::ranges::copy(signals, inline_signals.begin());
        inline_signals[signals.size()] = timeline_signal;
        all_signals = std::span{inline_signals.data(), signals.size() + 1};
    } else {
        heap_signals.assign(signals.begin(), signals.end());
        heap_signals.push_back(timeline_signal);
        all_signals = heap_signals;
    }

    const VkSubmitInfo2 submit_info{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .waitSemaphoreInfoCount = static_cast<uint32_t>(waits.size()),
        .pWaitSemaphoreInfos = waits.data(),
        .commandBufferInfoCount = static_cast<uint32_t>(command_buffers.size()),
        .pCommandBufferInfos = command_buffers.data(),
        .signalSemaphoreInfoCount = static_cast<uint32_t>(all_signals.size()),
        .pSignalSemaphoreInfos = all_signals.data(),
    };
    const VkResult result = vk_->vkQueueSubmit2(queue_, 1, &submit_info, VK_NULL_HANDLE);
    if (result == VK_SUCCESS) {
        last_submitted_.store(value, std::memory_order_release);
        signalled = value;
    }
    return result;
}

VkResult create_queue(VkDevice device, const VolkDeviceTable& vk, uint32_t family, uint32_t index,
                      std::string_view debug_name, std::unique_ptr<Queue>& queue) {
    VkQueue handle = VK_NULL_HANDLE;
    vk.vkGetDeviceQueue(device, family, index, &handle);
    BYTEE_ASSERT(handle != VK_NULL_HANDLE, "queue {} of family {} was not created with the device", index, family);

    std::unique_ptr<Queue> created{new Queue{device, vk, handle, family}}; // private constructor
    const VkSemaphoreTypeCreateInfo type_info{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
        .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
        .initialValue = 0,
    };
    const VkSemaphoreCreateInfo semaphore_info{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext = &type_info};
    if (const VkResult result = vk.vkCreateSemaphore(device, &semaphore_info, nullptr, &created->timeline_);
        result != VK_SUCCESS) {
        return result;
    }
    set_debug_name(device, VK_OBJECT_TYPE_QUEUE, handle, debug_name);
    set_debug_name(device, VK_OBJECT_TYPE_SEMAPHORE, created->timeline_, std::format("{} timeline", debug_name));
    queue = std::move(created);
    return VK_SUCCESS;
}

} // namespace bytee::render::rhi::vulkan
