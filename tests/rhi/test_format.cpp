#include <bytee/render/rhi/format.hpp>
#include <bytee/render/rhi/texture.hpp>

#include <doctest/doctest.h>

using namespace bytee::render::rhi;

static_assert(bytes_per_texel(Format::RGBA8Unorm) == 4);
static_assert(bytes_per_texel(Format::RGBA16Float) == 8);
static_assert(bytes_per_texel(Format::RGBA32Float) == 16);
static_assert(bytes_per_texel(Format::Undefined) == 0);
static_assert(is_depth_format(Format::D32Float) && !has_stencil(Format::D32Float));
static_assert(is_depth_format(Format::D32FloatS8Uint) && has_stencil(Format::D32FloatS8Uint));
static_assert(!is_depth_format(Format::RGBA8Srgb) && is_srgb_format(Format::RGBA8Srgb));

TEST_CASE("Full mip chains include the 1x1 level") {
    CHECK(full_mip_count({.width = 1, .height = 1}) == 1);
    CHECK(full_mip_count({.width = 256, .height = 256}) == 9);
    CHECK(full_mip_count({.width = 300, .height = 20}) == 9);           // driven by the largest dimension
    CHECK(full_mip_count({.width = 4, .height = 4, .depth = 64}) == 7); // 3D: depth counts too
}

TEST_CASE("Format names") {
    CHECK(to_string(Format::BGRA8Srgb) == "BGRA8Srgb");
    CHECK(to_string(Format::D32FloatS8Uint) == "D32FloatS8Uint");
}
