#pragma once

// White-box test helpers for the Vulkan backend (static builds only).

#include "vulkan/vulkan_device.hpp"

#include <bytee/render/rhi/device.hpp>

#include <doctest/doctest.h>

#include <cstdint>

namespace bytee::tests {

// Number of live VMA allocations of a device.
[[nodiscard]] inline uint32_t live_allocations(const render::rhi::detail::DeviceImpl& impl) {
    VmaTotalStatistics statistics{};
    vmaCalculateStatistics(impl.allocator, &statistics);
    return statistics.total.statistics.allocationCount;
}

// Blocks the device's queue until open() is called, making "the GPU is still busy" deterministic. Always opens on
// destruction (a failing test must never leave the GPU blocked) and retires its semaphore through the device, since
// the blocked submission may still be using it.
class GpuGate {
private:
    render::rhi::detail::DeviceImpl& impl_;
    VkSemaphore semaphore_ = VK_NULL_HANDLE;
    render::rhi::SyncPoint blocked_;
    bool opened_ = false;

public:
    explicit GpuGate(render::rhi::detail::DeviceImpl& impl)
        : impl_(impl) {
        const VkSemaphoreTypeCreateInfo type{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
                                             .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE};
        const VkSemaphoreCreateInfo info{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext = &type};
        REQUIRE(impl_.vk.vkCreateSemaphore(impl_.device, &info, nullptr, &semaphore_) == VK_SUCCESS);
        const VkSemaphoreSubmitInfo wait{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                                         .semaphore = semaphore_,
                                         .value = 1,
                                         .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT};
        auto submitted = impl_.submit(render::rhi::QueueType::Graphics, {}, std::span{&wait, 1});
        REQUIRE(submitted.has_value());
        blocked_ = *submitted;
    }
    GpuGate(const GpuGate&) = delete;
    GpuGate& operator=(const GpuGate&) = delete;
    ~GpuGate() {
        if (!opened_) {
            open();
        }
        impl_.retire({.type = VK_OBJECT_TYPE_SEMAPHORE, .handle = reinterpret_cast<uint64_t>(semaphore_)});
    }

    // The submission that waits for the gate.
    [[nodiscard]] render::rhi::SyncPoint blocked() const noexcept { return blocked_; }

    void open() {
        const VkSemaphoreSignalInfo signal{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO, .semaphore = semaphore_, .value = 1};
        CHECK(impl_.vk.vkSignalSemaphore(impl_.device, &signal) == VK_SUCCESS);
        opened_ = true;
    }
};

} // namespace bytee::tests
