// White-box GPU tests of command contexts and upload staging (static builds, label "gpu").

#include "support/gpu_fixture.hpp"
#include "support/vulkan_gpu_gate.hpp"

#include <doctest/doctest.h>

#include <vector>

namespace rhi = bytee::render::rhi;
using bytee::tests::DeviceFixture;

using bytee::tests::GpuGate;
using bytee::tests::live_allocations;

TEST_CASE_FIXTURE(DeviceFixture, "Command contexts are recycled once their work completes") {
    rhi::detail::DeviceImpl& impl = *rhi::get_impl(device);
    for (int i = 0; i < 10; ++i) {
        auto done = device.submit(device.begin_commands().value());
        REQUIRE(done.has_value());
        REQUIRE(device.wait(*done).has_value());
    }
    CHECK(impl.contexts.all.size() == 1); // one context, reused every time

    {
        auto discarded = device.begin_commands();
        REQUIRE(discarded.has_value());
    }
    CHECK(impl.contexts.all.size() == 1); // discarded lists return their context too
    CHECK(impl.contexts.live_command_lists.load() == 0);
}

TEST_CASE_FIXTURE(DeviceFixture, "A submitted context is not reused until its work completes") {
    rhi::detail::DeviceImpl& impl = *rhi::get_impl(device);
    GpuGate gate{impl};

    auto first = device.begin_commands();
    REQUIRE(first.has_value());
    const rhi::detail::CommandContext* submitted_context = rhi::get_context(*first);
    REQUIRE(device.submit(std::move(*first)).has_value()); // queued behind the gate: still in flight

    auto second = device.begin_commands();
    REQUIRE(second.has_value());
    CHECK(rhi::get_context(*second) != submitted_context); // in flight: must not be recycled
    REQUIRE(device.submit(std::move(*second)).has_value());
    CHECK(impl.contexts.all.size() == 2);

    gate.open();
    REQUIRE(device.wait_idle().has_value());
    auto third = device.begin_commands();
    REQUIRE(third.has_value());
    CHECK(impl.contexts.all.size() == 2); // completed contexts are reused, no new one
}

TEST_CASE_FIXTURE(DeviceFixture, "Upload staging buffers are freed after the copy completes") {
    rhi::detail::DeviceImpl& impl = *rhi::get_impl(device);
    rhi::Buffer target =
        device.create_buffer({.size = 1 << 16, .usage = rhi::BufferUsage::CopyDst, .debug_name = "target"}).value();
    const uint32_t baseline = live_allocations(impl);

    const std::vector<std::byte> data(1 << 16, std::byte{0x5A});
    auto done = device.upload(target, 0, data);
    REQUIRE(done.has_value());
    REQUIRE(device.wait(*done).has_value()); // wait() collects the retired staging buffer
    CHECK(live_allocations(impl) == baseline);
    CHECK(impl.live_objects.count() == 1); // only `target`
}
