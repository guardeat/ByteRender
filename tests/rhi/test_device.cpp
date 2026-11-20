// GPU tests: need a Vulkan 1.3 capable GPU and driver. Every test also enforces the zero-noise policy:
// nothing (engine, loader, validation layer, driver) may log a warning or an error.

#include "support/capture_log_sink.hpp"

#include <byte/render/rhi/instance_factory.hpp>

#include <doctest/doctest.h>

namespace core = byte::core;
namespace rhi = byte::render::rhi;

namespace {

struct GpuFixture {
    byte::tests::CaptureLogSink sink;
    core::Logger logger{sink, core::LogLevel::Trace};

    core::Result<rhi::Instance> create_instance() {
        const rhi::InstanceFactory factory{logger};
        return factory.create_instance({.app_name = "byte_render_gpu_tests", .enable_validation = true});
    }

    ~GpuFixture() { CHECK_MESSAGE(sink.problems().empty(), sink.problems()); }
};

} // namespace

TEST_CASE_FIXTURE(GpuFixture, "Instance reports a supported adapter with validation enabled") {
    auto instance = create_instance();
    REQUIRE_MESSAGE(instance.has_value(), instance.error().message);
    CHECK(instance->is_validation_enabled());

    bool any_supported = false;
    for (const rhi::AdapterInfo& adapter : instance->adapters()) {
        CHECK_FALSE(adapter.name.empty());
        any_supported = any_supported || adapter.is_supported;
    }
    CHECK(any_supported);
}

TEST_CASE_FIXTURE(GpuFixture, "Only one Instance may be alive") {
    auto first = create_instance();
    REQUIRE(first.has_value());

    auto second = create_instance();
    REQUIRE_FALSE(second.has_value());
    CHECK(second.error().code == core::ErrorCode::NotSupported);

    first = core::Result<rhi::Instance>{rhi::Instance{}}; // destroy the first one
    auto third = create_instance();
    CHECK(third.has_value());
}

TEST_CASE_FIXTURE(GpuFixture, "Device is created on the best adapter and can wait idle") {
    auto instance = create_instance();
    REQUIRE(instance.has_value());

    auto device = instance->create_device({.debug_name = "Test Device"});
    REQUIRE_MESSAGE(device.has_value(), device.error().message);
    CHECK(device->is_valid());
    CHECK(device->adapter().is_supported);

    auto idle = device->wait_idle();
    CHECK_MESSAGE(idle.has_value(), idle.error().message);
}

TEST_CASE_FIXTURE(GpuFixture, "Device creation with an explicit adapter index") {
    auto instance = create_instance();
    REQUIRE(instance.has_value());

    const auto adapters = instance->adapters();
    REQUIRE_FALSE(adapters.empty());

    auto out_of_range = instance->create_device({.adapter_index = static_cast<uint32_t>(adapters.size())});
    REQUIRE_FALSE(out_of_range.has_value());
    CHECK(out_of_range.error().code == core::ErrorCode::InvalidArgument);

    for (const rhi::AdapterInfo& adapter : adapters) {
        if (adapter.is_supported) {
            auto device = instance->create_device({.adapter_index = adapter.index});
            REQUIRE_MESSAGE(device.has_value(), device.error().message);
            CHECK(device->adapter().index == adapter.index);
            break;
        }
    }
}

TEST_CASE_FIXTURE(GpuFixture, "Devices are move-only values") {
    auto instance = create_instance();
    REQUIRE(instance.has_value());

    auto created = instance->create_device({});
    REQUIRE(created.has_value());

    rhi::Device device = std::move(created).value();
    CHECK(device.is_valid());

    rhi::Device moved = std::move(device);
    CHECK(moved.is_valid());
    CHECK_FALSE(device.is_valid()); // NOLINT(bugprone-use-after-move): verifying moved-from state
}
