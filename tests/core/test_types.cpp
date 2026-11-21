#include <byte/core/types.hpp>

#include <doctest/doctest.h>

using namespace byte::core;

TEST_CASE("Extents compare by value and default to a single layer") {
    CHECK(Extent2D{.width = 4, .height = 2} == Extent2D{.width = 4, .height = 2});
    CHECK_FALSE(Extent2D{.width = 4, .height = 2} == Extent2D{.width = 2, .height = 4});
    CHECK(Extent3D{.width = 8, .height = 8}.depth == 1);
}
