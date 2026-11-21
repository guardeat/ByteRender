// GPU tests (label "gpu"): need a Vulkan 1.3 driver. The fixture fails any test that logs a warning or error.

#include "support/error_text.hpp"
#include "support/gpu_fixture.hpp"

#include <doctest/doctest.h>

namespace core = bytee::core;
namespace rhi = bytee::render::rhi;
using bytee::tests::GpuFixture;

TEST_CASE_FIXTURE(GpuFixture, "Instance with validation reports at least one supported adapter") {
    auto instance = create_instance();
    REQUIRE_MESSAGE(instance.has_value(), bytee::tests::error_text(instance));
    CHECK(instance->is_valid());
    CHECK(instance->is_validation_enabled());

    bool any_supported = false;
    for (const rhi::AdapterInfo& adapter : instance->adapters()) {
        CHECK_FALSE(adapter.name.empty());
        CHECK(adapter.index < instance->adapters().size());
        CHECK(adapter.is_supported == adapter.unsupported_reason.empty());
        any_supported = any_supported || adapter.is_supported;
    }
    CHECK(any_supported);
}

TEST_CASE_FIXTURE(GpuFixture, "Instance without validation") {
    auto instance = create_instance(false);
    REQUIRE_MESSAGE(instance.has_value(), bytee::tests::error_text(instance));
    CHECK_FALSE(instance->is_validation_enabled());
    CHECK_FALSE(instance->adapters().empty());
}

TEST_CASE_FIXTURE(GpuFixture, "Only one Instance may be alive at a time") {
    auto first = create_instance();
    REQUIRE(first.has_value());

    auto second = create_instance();
    REQUIRE_FALSE(second.has_value());
    CHECK(second.error().code() == core::ErrorCode::NotSupported);

    *first = rhi::Instance{}; // destroy the first instance
    auto third = create_instance();
    CHECK_MESSAGE(third.has_value(), bytee::tests::error_text(third));
}

TEST_CASE_FIXTURE(GpuFixture, "Instances are move-only values") {
    auto created = create_instance();
    REQUIRE(created.has_value());

    rhi::Instance instance = std::move(created).value();
    CHECK(instance.is_valid());
    rhi::Instance moved = std::move(instance);
    CHECK(moved.is_valid());
    CHECK_FALSE(instance.is_valid()); // NOLINT(bugprone-use-after-move): checking the moved-from state
}
