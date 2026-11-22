// White-box GPU tests of timeline queues and deferred destruction (static builds, label "gpu").
// Real Vulkan objects are retired, so the validation layer reports any early or missing destruction.

#include "support/error_text.hpp"
#include "support/gpu_fixture.hpp"
#include "support/vulkan_gpu_gate.hpp"

#include <doctest/doctest.h>

#include <chrono>

namespace core = bytee::core;
namespace rhi = bytee::render::rhi;
namespace vulkan = bytee::render::rhi::vulkan;
using bytee::tests::DeviceFixture;
using bytee::tests::GpuFixture;
using bytee::tests::GpuGate;

namespace {

VkFence create_fence(const rhi::detail::DeviceImpl& impl) {
    const VkFenceCreateInfo info{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkFence fence = VK_NULL_HANDLE;
    REQUIRE(impl.vk.vkCreateFence(impl.device, &info, nullptr, &fence) == VK_SUCCESS);
    return fence;
}

vulkan::RetiredObject retired(VkFence fence) {
    return {.type = VK_OBJECT_TYPE_FENCE, .handle = reinterpret_cast<uint64_t>(fence)};
}

} // namespace

TEST_CASE_FIXTURE(DeviceFixture, "The timeline advances with every submission") {
    rhi::detail::DeviceImpl& impl = *rhi::get_impl(device);
    CHECK(impl.queue(rhi::QueueType::Graphics).last_submitted() == 0);

    auto first = impl.submit(rhi::QueueType::Graphics, {});
    auto second = impl.submit(rhi::QueueType::Graphics, {});
    REQUIRE_MESSAGE(first.has_value(), bytee::tests::error_text(first));
    REQUIRE(second.has_value());
    CHECK(first->value == 1);
    CHECK(second->value == 2);

    auto waited = device.wait(*second);
    REQUIRE_MESSAGE(waited.has_value(), bytee::tests::error_text(waited));
    CHECK(device.is_complete(*first));
    CHECK(device.is_complete(*second));
}

TEST_CASE_FIXTURE(DeviceFixture, "Waiting times out while the GPU is blocked, then succeeds") {
    rhi::detail::DeviceImpl& impl = *rhi::get_impl(device);
    GpuGate gate{impl}; // the queue cannot proceed until the gate opens

    CHECK_FALSE(device.is_complete(gate.blocked()));
    auto timed_out = device.wait(gate.blocked(), std::chrono::milliseconds{20});
    REQUIRE_FALSE(timed_out.has_value());
    CHECK(timed_out.error().code() == core::ErrorCode::Timeout);
    CHECK(timed_out.error().message().empty()); // allocation-free

    gate.open();
    auto waited = device.wait(gate.blocked(), std::chrono::seconds{5});
    CHECK_MESSAGE(waited.has_value(), bytee::tests::error_text(waited));
}

TEST_CASE_FIXTURE(DeviceFixture, "Retiring with no GPU work in flight destroys immediately") {
    rhi::detail::DeviceImpl& impl = *rhi::get_impl(device);
    impl.retire(retired(create_fence(impl)));
    CHECK(impl.retired.objects.size() == 0);
}

TEST_CASE_FIXTURE(DeviceFixture, "Retired objects wait for in-flight work") {
    rhi::detail::DeviceImpl& impl = *rhi::get_impl(device);
    GpuGate gate{impl};

    impl.retire(retired(create_fence(impl))); // the GPU is provably not done: must be deferred
    CHECK(impl.retired.objects.size() == 1);
    REQUIRE(impl.collect().has_value());
    CHECK(impl.retired.objects.size() == 1);

    gate.open();
    REQUIRE(device.wait(gate.blocked()).has_value()); // wait() collects
    CHECK(impl.retired.objects.size() == 0);
}

TEST_CASE_FIXTURE(DeviceFixture, "wait_idle releases every retired object") {
    rhi::detail::DeviceImpl& impl = *rhi::get_impl(device);
    for (int i = 0; i < 4; ++i) {
        REQUIRE(impl.submit(rhi::QueueType::Graphics, {}).has_value());
        impl.retire(retired(create_fence(impl)));
    }
    REQUIRE(device.wait_idle().has_value());
    CHECK(impl.retired.objects.size() == 0);
}

TEST_CASE_FIXTURE(GpuFixture, "Destroying a device releases objects that are still retired") {
    auto instance = create_instance();
    REQUIRE(instance.has_value());
    auto device = instance->create_device({.debug_name = "Short-lived"});
    REQUIRE(device.has_value());

    rhi::detail::DeviceImpl& impl = *rhi::get_impl(*device);
    {
        GpuGate gate{impl};
        impl.retire(retired(create_fence(impl)));
        REQUIRE(impl.retired.objects.size() == 1);
    } // the gate opens and retires its own semaphore: both are still pending when the device goes away
    *device = rhi::Device{}; // validation would report leaked or in-use objects
}

TEST_CASE_FIXTURE(DeviceFixture, "A lost device rejects submissions and waits") {
    rhi::detail::DeviceImpl& impl = *rhi::get_impl(device);
    static_cast<void>(impl.fail(VK_ERROR_DEVICE_LOST, "simulated call"));
    sink.clear(); // the loss is reported once, on purpose

    auto submitted = impl.submit(rhi::QueueType::Graphics, {});
    REQUIRE_FALSE(submitted.has_value());
    CHECK(submitted.error().code() == core::ErrorCode::DeviceLost);

    auto waited = device.wait(rhi::SyncPoint{.value = 1});
    REQUIRE_FALSE(waited.has_value());
    CHECK(waited.error().code() == core::ErrorCode::DeviceLost);
    CHECK_FALSE(device.is_complete(rhi::SyncPoint{.value = 1}));

    impl.lost.store(false); // let the fixture shut the device down normally
}
