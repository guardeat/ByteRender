#include <bytee/core/version.hpp>

#include <doctest/doctest.h>

#include <format>

TEST_CASE("Linked library version matches the project version") {
    const bytee::core::Version expected{
        .major = BYTEE_EXPECTED_VERSION_MAJOR,
        .minor = BYTEE_EXPECTED_VERSION_MINOR,
        .patch = BYTEE_EXPECTED_VERSION_PATCH,
    };
    CHECK(bytee::core::version() == expected);
    CHECK(bytee::core::version_string() == std::format("{}.{}.{}", expected.major, expected.minor, expected.patch));
}
