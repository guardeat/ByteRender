// White-box GPU tests of command contexts and upload staging (static builds, label "gpu").

#include "support/gpu_fixture.hpp"
#include "vulkan/vulkan_device.hpp"

#include <doctest/doctest.h>

#include <vector>

namespace rhi = bytee::render::rhi;
using bytee::tests::DeviceFixture;

namespace {

uint32_t live_allocations(const rhi::detail::DeviceImpl& impl) {
    VmaTotalStatistics statistics{};
    vmaCalculateStatistics(impl.allocator, &statistics);
    return statistics.total.statistics.allocationCount;
}

} // namespace

TEST_CASE_FIXTURE(DeviceFixture, "Command contexts are recycled once their work completes") {
    rhi::detail::DeviceImpl& impl = *rhi::get_impl(device);
    for (int i = 0; i < 10; ++i) {
        auto done = device.submit(device.begin_commands().value());
        REQUIRE(done.has_value());
        REQUIRE(device.wait(*done).has_value());
    }
    CHECK(impl.contexts.size() == 1); // one context, reused every time

    {
        auto discarded = device.begin_commands();
        REQUIRE(discarded.has_value());
    }
    CHECK(impl.contexts.size() == 1); // discarded lists return their context too
    CHECK(impl.live_command_lists.load() == 0);
}

TEST_CASE_FIXTURE(DeviceFixture, "Contexts in flight are not reused") {
    rhi::detail::DeviceImpl& impl = *rhi::get_impl(device);
    auto first = device.begin_commands();
    auto second = device.begin_commands(); // both alive at once: two contexts
    REQUIRE(first.has_value());
    REQUIRE(second.has_value());
    CHECK(rhi::get_context(*first) != rhi::get_context(*second));
    REQUIRE(device.submit(std::move(*first)).has_value());
    REQUIRE(device.submit(std::move(*second)).has_value());
    CHECK(impl.contexts.size() == 2);
    REQUIRE(device.wait_idle().has_value());
}

TEST_CASE_FIXTURE(DeviceFixture, "Upload staging buffers are freed after the copy completes") {
    rhi::detail::DeviceImpl& impl = *rhi::get_impl(device);
    rhi::Buffer target =
        device.create_buffer({.size = 1 << 16, .usage = rhi::BufferUsage::TransferDst, .debug_name = "target"}).value();
    const uint32_t baseline = live_allocations(impl);

    const std::vector<std::byte> data(1 << 16, std::byte{0x5A});
    auto done = device.upload(target, 0, data);
    REQUIRE(done.has_value());
    REQUIRE(device.wait(*done).has_value()); // wait() collects the retired staging buffer
    CHECK(live_allocations(impl) == baseline);
    CHECK(impl.live_objects.count() == 1); // only `target`
}
