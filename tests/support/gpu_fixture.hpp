#pragma once

// Fixture for tests that need a GPU. Captures all log output and fails the test if anything (engine,
// loader, validation layer, driver) logged a warning or an error — the zero-noise policy.

#include "support/capture_log_sink.hpp"

#include <byte/render/rhi/instance_factory.hpp>

#include <doctest/doctest.h>

namespace byte::tests {

struct GpuFixture {
    CaptureLogSink sink;
    core::Logger logger{sink, core::LogLevel::Trace};

    GpuFixture() = default;
    GpuFixture(const GpuFixture&) = delete;
    GpuFixture& operator=(const GpuFixture&) = delete;

    ~GpuFixture() { CHECK_MESSAGE(sink.problems().empty(), sink.problems()); }

    core::Result<render::rhi::Instance> create_instance() {
        const render::rhi::InstanceFactory factory{logger};
        return factory.create_instance({.app_name = "byte_render_tests", .enable_validation = true});
    }
};

// Instance + Device on the best adapter, created in the constructor.
struct DeviceFixture : GpuFixture {
    render::rhi::Instance instance;
    render::rhi::Device device;

    DeviceFixture() {
        auto created_instance = create_instance();
        REQUIRE_MESSAGE(created_instance.has_value(), created_instance.error().message);
        instance = std::move(created_instance).value();

        auto created_device = instance.create_device({.debug_name = "Test Device"});
        REQUIRE_MESSAGE(created_device.has_value(), created_device.error().message);
        device = std::move(created_device).value();
    }
};

} // namespace byte::tests
