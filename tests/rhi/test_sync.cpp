// Public SyncPoint API. GPU tests; zero-noise policy enforced by the fixture.

#include "support/gpu_fixture.hpp"

#include <doctest/doctest.h>

#include <chrono>

namespace core = byte::core;
namespace rhi = byte::render::rhi;
using byte::tests::DeviceFixture;

TEST_CASE_FIXTURE(DeviceFixture, "A default SyncPoint is always complete") {
    CHECK(device.is_complete(rhi::SyncPoint{}));
    auto waited = device.wait(rhi::SyncPoint{});
    CHECK_MESSAGE(waited.has_value(), waited.error().message);
}

TEST_CASE_FIXTURE(DeviceFixture, "Waiting for a SyncPoint that was never submitted is an error, not a deadlock") {
    const rhi::SyncPoint future{.queue = rhi::QueueType::Graphics, .value = 1};
    CHECK_FALSE(device.is_complete(future));

    auto waited = device.wait(future, std::chrono::seconds{1});
    REQUIRE_FALSE(waited.has_value());
    CHECK(waited.error().code == core::ErrorCode::InvalidArgument);
}

TEST_CASE_FIXTURE(DeviceFixture, "wait_idle succeeds on an idle device") {
    auto idle = device.wait_idle();
    CHECK_MESSAGE(idle.has_value(), idle.error().message);
}
