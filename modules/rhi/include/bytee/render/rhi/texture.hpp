#pragma once

#include <bytee/core/flags.hpp>
#include <bytee/core/types.hpp>
#include <bytee/render/rhi/format.hpp>

#include <cstdint>
#include <string>
#include <string_view>

namespace bytee::render::rhi {

enum class TextureUsage : uint32_t {
    Sampled = 1u << 0,
    Storage = 1u << 1,
    ColorAttachment = 1u << 2,
    DepthStencilAttachment = 1u << 3,
    TransferSrc = 1u << 4,
    TransferDst = 1u << 5,
};
BYTEE_ENABLE_FLAGS(TextureUsage)

enum class TextureDimension : uint8_t {
    Texture2D, // with optional array layers
    Texture3D,
};

struct TextureDesc {
    TextureDimension dimension = TextureDimension::Texture2D;
    core::Extent3D extent;
    uint32_t mip_levels = 1;
    uint32_t array_layers = 1; // 2D only
    Format format = Format::Undefined;
    core::Flags<TextureUsage> usage;
    std::string_view debug_name;
};

struct TextureInfo {
    TextureDimension dimension = TextureDimension::Texture2D;
    core::Extent3D extent;
    uint32_t mip_levels = 1;
    uint32_t array_layers = 1;
    Format format = Format::Undefined;
    core::Flags<TextureUsage> usage;
    std::string debug_name;
};

// Number of mip levels of a full chain for `extent` (1x1 included).
[[nodiscard]] constexpr uint32_t full_mip_count(core::Extent3D extent) noexcept {
    uint32_t largest = extent.width > extent.height ? extent.width : extent.height;
    largest = largest > extent.depth ? largest : extent.depth;
    uint32_t levels = 1;
    while (largest > 1) {
        largest >>= 1u;
        ++levels;
    }
    return levels;
}

} // namespace bytee::render::rhi
