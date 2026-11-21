// White-box tests of the Vulkan backend's timeline queues and deferred destruction.
// Built only for static Vulkan builds (they use backend internals). GPU required; zero-noise enforced.

#include "support/gpu_fixture.hpp"
#include "vulkan/vulkan_device.hpp"

#include <doctest/doctest.h>

#include <atomic>
#include <chrono>

namespace core = byte::core;
namespace rhi = byte::render::rhi;
namespace vulkan = byte::render::rhi::vulkan;
using byte::tests::DeviceFixture;

namespace {

// A real Vulkan object to retire; validation reports it if it is leaked or destroyed while in use.
VkFence create_fence(const rhi::Device::Impl& impl) {
    const VkFenceCreateInfo info{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkFence fence = VK_NULL_HANDLE;
    REQUIRE(impl.vk.vkCreateFence(impl.device, &info, nullptr, &fence) == VK_SUCCESS);
    return fence;
}

} // namespace

TEST_CASE_FIXTURE(DeviceFixture, "Timeline starts at zero and advances with each submission") {
    vulkan::Queue& queue = rhi::get_impl(device)->queue(rhi::QueueType::Graphics);
    CHECK(queue.last_submitted() == 0);
    REQUIRE(queue.completed().has_value());
    CHECK(*queue.completed() == 0);

    auto first = queue.submit({});
    REQUIRE_MESSAGE(first.has_value(), first.error().message);
    CHECK(*first == 1);

    auto second = queue.submit({});
    REQUIRE(second.has_value());
    CHECK(*second == 2);
    CHECK(queue.last_submitted() == 2);

    auto waited = device.wait({.queue = rhi::QueueType::Graphics, .value = 2});
    REQUIRE_MESSAGE(waited.has_value(), waited.error().message);
    CHECK(device.is_complete({.queue = rhi::QueueType::Graphics, .value = 1}));
    CHECK(device.is_complete({.queue = rhi::QueueType::Graphics, .value = 2}));
    CHECK(queue.cached_completed() >= 2);
}

TEST_CASE_FIXTURE(DeviceFixture, "Retiring with no GPU work in flight destroys immediately") {
    rhi::Device::Impl& impl = *rhi::get_impl(device);
    const VkFence fence = create_fence(impl);

    bool destroyed = false;
    impl.retire([&impl, fence, &destroyed] {
        impl.vk.vkDestroyFence(impl.device, fence, nullptr);
        destroyed = true;
    });
    CHECK(destroyed);
    CHECK(impl.retire_queue.size() == 0);
}

TEST_CASE_FIXTURE(DeviceFixture, "Retired objects are destroyed once the GPU completes in-flight work") {
    rhi::Device::Impl& impl = *rhi::get_impl(device);
    vulkan::Queue& queue = impl.queue(rhi::QueueType::Graphics);

    auto submitted = queue.submit({});
    REQUIRE(submitted.has_value());

    const VkFence fence = create_fence(impl);
    std::atomic<bool> destroyed{false};
    impl.retire([&impl, fence, &destroyed] {
        impl.vk.vkDestroyFence(impl.device, fence, nullptr);
        destroyed = true;
    });

    // Either still pending, or the GPU was already done and it went straight through; never lost.
    CHECK((impl.retire_queue.size() == 1 || destroyed));

    auto waited = device.wait({.queue = rhi::QueueType::Graphics, .value = *submitted});
    REQUIRE_MESSAGE(waited.has_value(), waited.error().message);
    CHECK(destroyed);
    CHECK(impl.retire_queue.size() == 0);
}

TEST_CASE_FIXTURE(DeviceFixture, "wait_idle releases every retired object") {
    rhi::Device::Impl& impl = *rhi::get_impl(device);
    vulkan::Queue& queue = impl.queue(rhi::QueueType::Graphics);

    int destroyed = 0;
    for (int i = 0; i < 4; ++i) {
        REQUIRE(queue.submit({}).has_value());
        const VkFence fence = create_fence(impl);
        impl.retire([&impl, fence, &destroyed] {
            impl.vk.vkDestroyFence(impl.device, fence, nullptr);
            ++destroyed;
        });
    }

    auto idle = device.wait_idle();
    REQUIRE_MESSAGE(idle.has_value(), idle.error().message);
    CHECK(destroyed == 4);
    CHECK(impl.retire_queue.size() == 0);
}

TEST_CASE("Destroying a Device releases retired objects that are still pending") {
    byte::tests::GpuFixture fixture;
    auto instance = fixture.create_instance();
    REQUIRE(instance.has_value());
    auto device = instance->create_device({.debug_name = "Short-lived Device"});
    REQUIRE(device.has_value());

    rhi::Device::Impl& impl = *rhi::get_impl(*device);
    REQUIRE(impl.queue(rhi::QueueType::Graphics).submit({}).has_value());

    const VkFence fence = create_fence(impl);
    bool destroyed = false;
    impl.retire([&impl, fence, &destroyed] {
        impl.vk.vkDestroyFence(impl.device, fence, nullptr);
        destroyed = true;
    });

    device = core::Result<rhi::Device>{rhi::Device{}}; // destroy without waiting
    CHECK(destroyed);
}
