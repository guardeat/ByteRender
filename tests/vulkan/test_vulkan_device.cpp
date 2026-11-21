// White-box GPU tests of the Vulkan device (static builds; label "gpu"). The fixture enforces zero noise.

#include "support/gpu_fixture.hpp"
#include "vulkan/vulkan_device.hpp"

#include <doctest/doctest.h>

namespace core = bytee::core;
namespace rhi = bytee::render::rhi;
namespace vulkan = bytee::render::rhi::vulkan;
using bytee::tests::DeviceFixture;

TEST_CASE_FIXTURE(DeviceFixture, "The device has its own function table, a queue and a memory allocator") {
    const rhi::Device::Impl& impl = *rhi::get_impl(device);
    CHECK(impl.device != VK_NULL_HANDLE);
    CHECK(impl.vk.vkQueueSubmit2 != nullptr);
    CHECK(impl.vk.vkCmdBeginRendering != nullptr);
    CHECK(impl.queue(rhi::QueueType::Graphics).handle() != VK_NULL_HANDLE);
    CHECK(impl.allocator != VK_NULL_HANDLE);

    VmaTotalStatistics statistics{};
    vmaCalculateStatistics(impl.allocator, &statistics);
    CHECK(statistics.total.statistics.allocationCount == 0);
}

TEST_CASE_FIXTURE(DeviceFixture, "Debug names are accepted by the validation layer") {
    const rhi::Device::Impl& impl = *rhi::get_impl(device);
    vulkan::set_debug_name(impl.device, VK_OBJECT_TYPE_QUEUE, impl.queue(rhi::QueueType::Graphics).handle(),
                           "renamed queue");
    vulkan::set_debug_name(impl.device, VK_OBJECT_TYPE_DEVICE, impl.device, "");
}

TEST_CASE_FIXTURE(DeviceFixture, "Device lost is sticky") {
    rhi::Device::Impl& impl = *rhi::get_impl(device);
    const core::Error error = impl.fail(VK_ERROR_DEVICE_LOST, "simulated call");
    CHECK(error.code() == core::ErrorCode::DeviceLost);
    CHECK(device.is_lost());

    auto idle = device.wait_idle(); // must not touch the GPU any more
    REQUIRE_FALSE(idle.has_value());
    CHECK(idle.error().code() == core::ErrorCode::DeviceLost);
    CHECK(idle.error().message() == "device 'Test Device' was lost; it must be recreated");

    // fail() logged the loss once (an error by design); keep the zero-noise check meaningful for everything else.
    const auto entries = sink.entries();
    REQUIRE(entries.size() >= 1);
    CHECK(entries.back().message == "device 'Test Device' was lost; it must be recreated");
    impl.lost.store(false); // let the fixture's device shut down normally
    sink.clear();
}
