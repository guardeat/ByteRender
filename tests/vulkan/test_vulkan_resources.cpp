// White-box GPU tests of resource lifetime (static builds, label "gpu"): deferred destruction is checked against
// the pools and VMA's live allocation count while the GPU is deliberately blocked.

#include "support/gpu_fixture.hpp"
#include "vulkan/vulkan_device.hpp"

#include <doctest/doctest.h>

namespace core = bytee::core;
namespace rhi = bytee::render::rhi;
using bytee::tests::DeviceFixture;
using bytee::tests::GpuFixture;

namespace {

uint32_t live_allocations(const rhi::detail::DeviceImpl& impl) {
    VmaTotalStatistics statistics{};
    vmaCalculateStatistics(impl.allocator, &statistics);
    return statistics.total.statistics.allocationCount;
}

// Blocks the queue until open() is called: makes "the GPU is still busy" deterministic. Always opens on destruction
// (a failing test must never leave the GPU blocked) and retires its semaphore through the device, since the blocked
// submission may still be using it.
class GpuGate {
private:
    rhi::detail::DeviceImpl& impl_;
    VkSemaphore semaphore_ = VK_NULL_HANDLE;
    rhi::SyncPoint blocked_;
    bool opened_ = false;

public:
    explicit GpuGate(rhi::detail::DeviceImpl& impl)
        : impl_(impl) {
        const VkSemaphoreTypeCreateInfo type{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
                                             .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE};
        const VkSemaphoreCreateInfo info{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext = &type};
        REQUIRE(impl_.vk.vkCreateSemaphore(impl_.device, &info, nullptr, &semaphore_) == VK_SUCCESS);
        const VkSemaphoreSubmitInfo wait{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                                         .semaphore = semaphore_,
                                         .value = 1,
                                         .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT};
        auto submitted = impl_.submit(rhi::QueueType::Graphics, {}, std::span{&wait, 1});
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

    [[nodiscard]] rhi::SyncPoint blocked() const noexcept { return blocked_; }
    void open() {
        const VkSemaphoreSignalInfo signal{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO, .semaphore = semaphore_, .value = 1};
        CHECK(impl_.vk.vkSignalSemaphore(impl_.device, &signal) == VK_SUCCESS);
        opened_ = true;
    }
};

} // namespace

TEST_CASE_FIXTURE(DeviceFixture, "Resources are destroyed immediately when the GPU is idle") {
    rhi::detail::DeviceImpl& impl = *rhi::get_impl(device);
    rhi::BufferHandle handle;
    {
        auto buffer = device.create_buffer({.size = 256, .usage = rhi::BufferUsage::Uniform});
        REQUIRE(buffer.has_value());
        handle = *buffer;
        CHECK(live_allocations(impl) == 1);
    }
    CHECK_FALSE(impl.buffers.contains(handle));
    CHECK(live_allocations(impl) == 0);
    CHECK(impl.live_objects.count() == 0);
}

TEST_CASE_FIXTURE(DeviceFixture, "Released resources live until the GPU has finished in-flight work") {
    rhi::detail::DeviceImpl& impl = *rhi::get_impl(device);
    GpuGate gate{impl};

    rhi::BufferHandle buffer;
    rhi::TextureHandle texture;
    {
        auto created_buffer = device.create_buffer({.size = 256, .usage = rhi::BufferUsage::Storage});
        auto created_texture = device.create_texture({.extent = {.width = 16, .height = 16},
                                                      .format = rhi::Format::RGBA8Unorm,
                                                      .usage = rhi::TextureUsage::Sampled});
        REQUIRE(created_buffer.has_value());
        REQUIRE(created_texture.has_value());
        buffer = *created_buffer;
        texture = *created_texture;
    } // both released while the GPU is provably busy

    CHECK(impl.buffers.contains(buffer));
    CHECK(impl.textures.contains(texture));
    CHECK(live_allocations(impl) == 2);
    REQUIRE(impl.collect().has_value());
    CHECK(impl.buffers.contains(buffer)); // still in flight

    gate.open();
    REQUIRE(device.wait(gate.blocked()).has_value()); // wait() collects
    CHECK_FALSE(impl.buffers.contains(buffer));
    CHECK_FALSE(impl.textures.contains(texture));
    CHECK(live_allocations(impl) == 0);
    CHECK(impl.live_objects.count() == 0);
}

TEST_CASE_FIXTURE(GpuFixture, "Destroying a device frees pending resources without reporting leaks") {
    auto instance = create_instance();
    REQUIRE(instance.has_value());
    auto device = instance->create_device({.debug_name = "Short-lived"});
    REQUIRE(device.has_value());
    rhi::detail::DeviceImpl& impl = *rhi::get_impl(*device);

    {
        GpuGate gate{impl};
        auto texture = device->create_texture({.extent = {.width = 8, .height = 8},
                                               .format = rhi::Format::RGBA8Unorm,
                                               .usage = rhi::TextureUsage::Sampled,
                                               .debug_name = "pending"});
        REQUIRE(texture.has_value());
        *texture = rhi::Texture{}; // retired, still pending
        CHECK(impl.retired_textures.size() == 1);
        gate.open();
    }
    *device = rhi::Device{}; // must not report 'pending' as a leak (the fixture fails on errors)
}
