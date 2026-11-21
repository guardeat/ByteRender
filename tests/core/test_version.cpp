#include <byte/core/version.hpp>

#include <doctest/doctest.h>

#include <format>

TEST_CASE("Linked library version matches the project version") {
    const byte::core::Version expected{
        .major = BYTE_RENDER_EXPECTED_VERSION_MAJOR,
        .minor = BYTE_RENDER_EXPECTED_VERSION_MINOR,
        .patch = BYTE_RENDER_EXPECTED_VERSION_PATCH,
    };
    CHECK(byte::core::version() == expected);
    CHECK(byte::core::version_string() == std::format("{}.{}.{}", expected.major, expected.minor, expected.patch));
}
