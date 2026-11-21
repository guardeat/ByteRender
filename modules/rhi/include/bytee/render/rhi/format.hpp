#pragma once

#include <cstdint>
#include <string_view>

namespace bytee::render::rhi {

// Texel formats supported by the RHI. Deliberately small: formats every desktop GPU (incl. Apple Silicon) supports.
// D24 depth formats are omitted on purpose (not supported by Apple GPUs).
enum class Format : uint16_t {
    Undefined,
    // 8-bit
    R8Unorm,
    RG8Unorm,
    RGBA8Unorm,
    RGBA8Srgb,
    BGRA8Unorm,
    BGRA8Srgb,
    // 16-bit float
    R16Float,
    RG16Float,
    RGBA16Float,
    // 32-bit
    R32Float,
    RG32Float,
    RGBA32Float,
    R32Uint,
    // packed
    RGB10A2Unorm,
    RG11B10Float,
    // depth / stencil
    D16Unorm,
    D32Float,
    D32FloatS8Uint,
};

[[nodiscard]] constexpr std::string_view to_string(Format format) noexcept {
    switch (format) {
    case Format::Undefined: return "Undefined";
    case Format::R8Unorm: return "R8Unorm";
    case Format::RG8Unorm: return "RG8Unorm";
    case Format::RGBA8Unorm: return "RGBA8Unorm";
    case Format::RGBA8Srgb: return "RGBA8Srgb";
    case Format::BGRA8Unorm: return "BGRA8Unorm";
    case Format::BGRA8Srgb: return "BGRA8Srgb";
    case Format::R16Float: return "R16Float";
    case Format::RG16Float: return "RG16Float";
    case Format::RGBA16Float: return "RGBA16Float";
    case Format::R32Float: return "R32Float";
    case Format::RG32Float: return "RG32Float";
    case Format::RGBA32Float: return "RGBA32Float";
    case Format::R32Uint: return "R32Uint";
    case Format::RGB10A2Unorm: return "RGB10A2Unorm";
    case Format::RG11B10Float: return "RG11B10Float";
    case Format::D16Unorm: return "D16Unorm";
    case Format::D32Float: return "D32Float";
    case Format::D32FloatS8Uint: return "D32FloatS8Uint";
    }
    return "Unknown";
}

// Size of one texel in bytes (0 for Undefined). For D32FloatS8Uint this is the depth-plane size used for copies.
[[nodiscard]] constexpr uint32_t bytes_per_texel(Format format) noexcept {
    switch (format) {
    case Format::Undefined: return 0;
    case Format::R8Unorm: return 1;
    case Format::RG8Unorm:
    case Format::R16Float:
    case Format::D16Unorm: return 2;
    case Format::RGBA8Unorm:
    case Format::RGBA8Srgb:
    case Format::BGRA8Unorm:
    case Format::BGRA8Srgb:
    case Format::RG16Float:
    case Format::R32Float:
    case Format::R32Uint:
    case Format::RGB10A2Unorm:
    case Format::RG11B10Float:
    case Format::D32Float:
    case Format::D32FloatS8Uint: return 4;
    case Format::RGBA16Float:
    case Format::RG32Float: return 8;
    case Format::RGBA32Float: return 16;
    }
    return 0;
}

[[nodiscard]] constexpr bool is_depth_format(Format format) noexcept {
    return format == Format::D16Unorm || format == Format::D32Float || format == Format::D32FloatS8Uint;
}

[[nodiscard]] constexpr bool has_stencil(Format format) noexcept {
    return format == Format::D32FloatS8Uint;
}

[[nodiscard]] constexpr bool is_srgb_format(Format format) noexcept {
    return format == Format::RGBA8Srgb || format == Format::BGRA8Srgb;
}

} // namespace bytee::render::rhi
