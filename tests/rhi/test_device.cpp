// GPU tests (label "gpu"). The fixtures fail any test that logs a warning or error.

#include "support/error_text.hpp"
#include "support/gpu_fixture.hpp"

#include <doctest/doctest.h>

#include <cstdint>

namespace core = bytee::core;
namespace rhi = bytee::render::rhi;
using bytee::tests::DeviceFixture;
using bytee::tests::GpuFixture;

TEST_CASE_FIXTURE(DeviceFixture, "A device on the best adapter is usable") {
    CHECK(device.is_valid());
    CHECK(device.adapter().is_supported);
    CHECK_FALSE(device.is_lost());
    CHECK(device.is_presentation_supported() == instance.is_presentation_supported());

    auto idle = device.wait_idle();
    CHECK_MESSAGE(idle.has_value(), bytee::tests::error_text(idle));
}

TEST_CASE_FIXTURE(GpuFixture, "Devices on explicit adapters; invalid indices are rejected") {
    auto instance = create_instance();
    REQUIRE(instance.has_value());
    const auto adapters = instance->adapters();

    auto out_of_range = instance->create_device({.adapter_index = static_cast<uint32_t>(adapters.size())});
    REQUIRE_FALSE(out_of_range.has_value());
    CHECK(out_of_range.error().code() == core::ErrorCode::InvalidArgument);

    for (const rhi::AdapterInfo& adapter : adapters) {
        auto device = instance->create_device({.adapter_index = adapter.index});
        if (adapter.is_supported) {
            REQUIRE_MESSAGE(device.has_value(), bytee::tests::error_text(device));
            CHECK(device->adapter().index == adapter.index);
        } else {
            REQUIRE_FALSE(device.has_value());
            CHECK(device.error().code() == core::ErrorCode::NotSupported);
        }
    }
}

TEST_CASE_FIXTURE(GpuFixture, "Several devices can share one instance") {
    auto instance = create_instance();
    REQUIRE(instance.has_value());
    auto first = instance->create_device({.debug_name = "First"});
    auto second = instance->create_device({.debug_name = "Second"});
    REQUIRE_MESSAGE(first.has_value(), bytee::tests::error_text(first));
    REQUIRE_MESSAGE(second.has_value(), bytee::tests::error_text(second));
    CHECK(first->wait_idle().has_value());
    CHECK(second->wait_idle().has_value());
}

TEST_CASE_FIXTURE(DeviceFixture, "Devices are move-only values") {
    rhi::Device moved = std::move(device);
    CHECK(moved.is_valid());
    CHECK_FALSE(device.is_valid()); // NOLINT(bugprone-use-after-move): checking the moved-from state

    device = std::move(moved);
    CHECK(device.is_valid());
}
