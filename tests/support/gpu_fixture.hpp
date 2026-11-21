#pragma once

// Fixture for tests that need a GPU. Captures all log output and fails the test if anything (engine, loader,
// validation layer, driver) logged a warning or an error: the zero-noise policy, enforced.

#include "support/capture_log_sink.hpp"

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
