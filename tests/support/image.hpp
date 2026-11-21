#pragma once

// RGBA8 images for rendering tests: comparison with tolerance and PPM output for inspecting failures.

#include "support/error_text.hpp"

#include <bytee/render/rhi/device.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace bytee::tests {

struct Rgba8 {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 0;

    friend constexpr bool operator==(const Rgba8&, const Rgba8&) noexcept = default;
};

struct Rgba8Image {
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<Rgba8> pixels; // row-major, top row first

    [[nodiscard]] const Rgba8& at(uint32_t x, uint32_t y) const { return pixels[std::size_t{y} * width + x]; }
};

struct ImageDifference {
    std::size_t mismatched_pixels = 0;
    int max_channel_difference = 0;
};

[[nodiscard]] inline ImageDifference compare(const Rgba8Image& actual, const Rgba8Image& expected, int tolerance) {
    REQUIRE(actual.width == expected.width);
    REQUIRE(actual.height == expected.height);
    ImageDifference difference;
    for (std::size_t i = 0; i < actual.pixels.size(); ++i) {
        const Rgba8& a = actual.pixels[i];
        const Rgba8& e = expected.pixels[i];
        const int channel =
            std::max({std::abs(a.r - e.r), std::abs(a.g - e.g), std::abs(a.b - e.b), std::abs(a.a - e.a)});
        difference.max_channel_difference = std::max(difference.max_channel_difference, channel);
        difference.mismatched_pixels += channel > tolerance ? 1 : 0;
    }
    return difference;
}

// Binary PPM (RGB, alpha dropped): viewable in most image viewers, no dependencies.
inline void write_ppm(const Rgba8Image& image, const std::filesystem::path& path) {
    std::ofstream file{path, std::ios::binary};
    file << "P6\n" << image.width << ' ' << image.height << "\n255\n";
    for (const Rgba8& pixel : image.pixels) {
        const char rgb[3] = {static_cast<char>(pixel.r), static_cast<char>(pixel.g), static_cast<char>(pixel.b)};
        file.write(rgb, 3);
    }
}

// Reads mip 0 / layer 0 of an RGBA8 texture (needs TransferSrc usage) currently in `state`; it stays in `state`.
[[nodiscard]] inline Rgba8Image read_rgba8(render::rhi::Device& device, render::rhi::TextureHandle texture,
                                           render::rhi::ResourceState state) {
    const render::rhi::TextureInfo& info = device.info(texture);
    REQUIRE(render::rhi::bytes_per_texel(info.format) == 4);
    Rgba8Image image{.width = info.extent.width,
                     .height = info.extent.height,
                     .pixels = std::vector<Rgba8>(std::size_t{info.extent.width} * info.extent.height)};
    auto downloaded =
        device.download(texture, std::as_writable_bytes(std::span{image.pixels}), {.before = state, .after = state});
    REQUIRE_MESSAGE(downloaded.has_value(), bytee::tests::error_text(downloaded));
    return image;
}

// Checks an image against the expectation; on failure writes both as PPM next to the test binary's working
// directory for inspection.
inline void check_image(const Rgba8Image& actual, const Rgba8Image& expected, int tolerance, std::string_view name) {
    const ImageDifference difference = compare(actual, expected, tolerance);
    if (difference.mismatched_pixels != 0) {
        write_ppm(actual, std::format("{}_actual.ppm", name));
        write_ppm(expected, std::format("{}_expected.ppm", name));
    }
    CHECK_MESSAGE(difference.mismatched_pixels == 0,
                  std::format("{}: {} pixels differ (max channel difference {}); wrote {}_actual.ppm / _expected.ppm",
                              name, difference.mismatched_pixels, difference.max_channel_difference, name));
}

[[nodiscard]] inline Rgba8Image solid_image(uint32_t width, uint32_t height, Rgba8 color) {
    return Rgba8Image{
        .width = width, .height = height, .pixels = std::vector<Rgba8>(std::size_t{width} * height, color)};
}

} // namespace bytee::tests
