#pragma once

#include <bytee/render/rhi/pipeline.hpp>

#include <cstdint>
#include <optional>
#include <string_view>

namespace bytee::render::rhi {

enum class Filter : uint8_t {
    Nearest,
    Linear,
};

enum class MipmapMode : uint8_t {
    Nearest,
    Linear,
};

enum class AddressMode : uint8_t {
    Repeat,
    MirroredRepeat,
    ClampToEdge,
};

struct SamplerDesc {
    Filter mag_filter = Filter::Linear;
    Filter min_filter = Filter::Linear;
    MipmapMode mipmap_mode = MipmapMode::Linear;
    AddressMode address_u = AddressMode::Repeat;
    AddressMode address_v = AddressMode::Repeat;
    AddressMode address_w = AddressMode::Repeat;
    float max_anisotropy = 1.0f; // 1 = off; at most the device limit (16 on most desktop GPUs)
    float min_lod = 0.0f;
    float max_lod = 1000.0f;          // effectively "all mip levels"
    std::optional<CompareOp> compare; // depth comparison (shadow maps)
    std::string_view debug_name;
};

} // namespace bytee::render::rhi
