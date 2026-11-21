// The image comparison used by rendering tests must itself be trustworthy.

#include "support/image.hpp"

#include <doctest/doctest.h>

using bytee::tests::compare;
using bytee::tests::Rgba8;
using bytee::tests::solid_image;

TEST_CASE("Image comparison counts pixels beyond the tolerance") {
    const auto expected = solid_image(4, 4, Rgba8{100, 100, 100, 255});
    auto actual = expected;
    CHECK(compare(actual, expected, 0).mismatched_pixels == 0);

    actual.pixels[5] = Rgba8{103, 100, 100, 255}; // off by 3 in one channel
    actual.pixels[9] = Rgba8{100, 100, 90, 255};  // off by 10
    CHECK(compare(actual, expected, 0).mismatched_pixels == 2);
    CHECK(compare(actual, expected, 3).mismatched_pixels == 1);
    CHECK(compare(actual, expected, 10).mismatched_pixels == 0);
    CHECK(compare(actual, expected, 0).max_channel_difference == 10);
    CHECK(actual.at(1, 1) == Rgba8{103, 100, 100, 255}); // row-major: index 5 is (1, 1) in a 4-wide image
}
