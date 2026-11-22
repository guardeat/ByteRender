#pragma once

#include <bytee/render/rhi/format.hpp>

#include <cstdint>
#include <span>
#include <string_view>

namespace bytee::render::rhi {

// Push constants: one range shared by every pipeline and every shader stage (architecture §7).
// 128 bytes is the minimum every Vulkan implementation guarantees.
inline constexpr uint32_t k_max_push_constant_bytes = 128;

enum class PrimitiveTopology : uint8_t {
    TriangleList,
    TriangleStrip,
    LineList,
    LineStrip,
    PointList,
};

enum class CullMode : uint8_t {
    None,
    Front,
    Back,
};

// Winding of front faces as seen in Bytee's clip space (Y up, architecture §14).
enum class FrontFace : uint8_t {
    CounterClockwise,
    Clockwise,
};

enum class CompareOp : uint8_t {
    Never,
    Less,
    Equal,
    LessOrEqual,
    Greater,
    NotEqual,
    GreaterOrEqual,
    Always,
};

enum class BlendMode : uint8_t {
    Opaque,        // no blending
    AlphaBlend,    // src * a + dst * (1 - a)
    Premultiplied, // src + dst * (1 - a)
    Additive,      // src * a + dst
};

// SPIR-V code (e.g. from bytee_add_shaders) and its entry point.
struct ShaderDesc {
    std::span<const uint32_t> spirv;
    std::string_view entry_point = "main";
};

// Bytee uses reverse-Z: nearer means larger depth, so the default comparison is Greater (with depth cleared to 0).
struct DepthState {
    bool test = false;
    bool write = false;
    CompareOp compare = CompareOp::Greater;
};

// Vertices are fetched by the vertex shader itself (vertex pulling: from buffers by device address, or computed), so
// there is no vertex input layout. Viewport and scissor are dynamic (set by begin_render_pass to the full target).
struct GraphicsPipelineDesc {
    ShaderDesc vertex;
    ShaderDesc fragment;                     // may be empty for depth-only pipelines
    std::span<const Format> color_formats;   // formats of the render pass color attachments, in order
    Format depth_format = Format::Undefined; // Undefined: no depth attachment
    PrimitiveTopology topology = PrimitiveTopology::TriangleList;
    CullMode cull = CullMode::None;
    FrontFace front_face = FrontFace::CounterClockwise;
    DepthState depth;
    BlendMode blend = BlendMode::Opaque; // applied to every color attachment
    std::string_view debug_name;
};

// Compute work is described entirely by its shader; resources are reached through bindless indices and buffer device
// addresses passed in push constants.
struct ComputePipelineDesc {
    ShaderDesc shader;
    std::string_view debug_name;
};

// Number of workgroups needed to cover `items` with workgroups of `group_size` (rounded up), for dispatch().
[[nodiscard]] constexpr uint32_t dispatch_group_count(uint32_t items, uint32_t group_size) noexcept {
    return group_size == 0 ? 0 : (items + group_size - 1) / group_size;
}

} // namespace bytee::render::rhi
