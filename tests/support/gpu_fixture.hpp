#pragma once

// Fixture for tests that need a GPU. Captures all log output and fails the test if anything (engine, loader,
// validation layer, driver) logged a warning or an error: the zero-noise policy, enforced.

#include "support/capture_log_sink.hpp"
#include "support/error_text.hpp"

#include <bytee/render/rhi/instance_factory.hpp>

#include <doctest/doctest.h>

namespace bytee::tests {

struct GpuFixture {
    CaptureLogSink sink;
    core::Logger logger{sink, core::LogLevel::Trace};

    GpuFixture() = default;
    GpuFixture(const GpuFixture&) = delete;
    GpuFixture& operator=(const GpuFixture&) = delete;
    ~GpuFixture() { CHECK_MESSAGE(sink.problems().empty(), sink.problems()); }

    [[nodiscard]] core::Result<render::rhi::Instance> create_instance(bool enable_validation = true) {
        const render::rhi::InstanceFactory factory{logger};
        return factory.create_instance({.app_name = "bytee_tests", .enable_validation = enable_validation});
    }
};

} // namespace bytee::tests

namespace bytee::tests {

// An Instance (validation on) and a Device on the best adapter, created before each test.
struct DeviceFixture : GpuFixture {
    render::rhi::Instance instance;
    render::rhi::Device device;

    DeviceFixture() {
        auto created_instance = create_instance();
        REQUIRE_MESSAGE(created_instance.has_value(), bytee::tests::error_text(created_instance));
        instance = std::move(created_instance).value();

        auto created_device = instance.create_device({.debug_name = "Test Device"});
        REQUIRE_MESSAGE(created_device.has_value(), bytee::tests::error_text(created_device));
        device = std::move(created_device).value();
    }
};

} // namespace bytee::tests
