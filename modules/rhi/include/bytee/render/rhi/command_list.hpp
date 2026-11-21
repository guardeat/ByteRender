#pragma once

#include <bytee/core/compiler.hpp>
#include <bytee/core/types.hpp>
#include <bytee/render/rhi/export.hpp>
#include <bytee/render/rhi/handles.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

namespace bytee::render::rhi {

namespace detail {
struct BYTEE_HIDDEN CommandContext; // backend-defined, opaque
} // namespace detail

// How a resource is about to be used. Barriers transition resources between states; the backend maps each state to
// pipeline stages, memory access and (for textures) an image layout.
enum class ResourceState : uint8_t {
    Undefined,       // contents may be discarded (initial state of textures)
    General,         // any access; slowest, use only when nothing else fits
    CopySrc,         // source of copy commands
    CopyDst,         // destination of copy/fill commands
    ShaderRead,      // sampled / read in shaders
    ShaderWrite,     // storage read-write in shaders
    ColorAttachment, // render target (textures)
    DepthWrite,      // depth-stencil attachment, writable (textures)
    DepthRead,       // depth-stencil read-only: depth test and/or sampling (textures)
    VertexBuffer,    // buffers
    IndexBuffer,     // buffers
    IndirectBuffer,  // buffers
    UniformBuffer,   // buffers
};

[[nodiscard]] constexpr std::string_view to_string(ResourceState state) noexcept {
    switch (state) {
    case ResourceState::Undefined: return "Undefined";
    case ResourceState::General: return "General";
    case ResourceState::CopySrc: return "CopySrc";
    case ResourceState::CopyDst: return "CopyDst";
    case ResourceState::ShaderRead: return "ShaderRead";
    case ResourceState::ShaderWrite: return "ShaderWrite";
    case ResourceState::ColorAttachment: return "ColorAttachment";
    case ResourceState::DepthWrite: return "DepthWrite";
    case ResourceState::DepthRead: return "DepthRead";
    case ResourceState::VertexBuffer: return "VertexBuffer";
    case ResourceState::IndexBuffer: return "IndexBuffer";
    case ResourceState::IndirectBuffer: return "IndirectBuffer";
    case ResourceState::UniformBuffer: return "UniformBuffer";
    }
    return "Unknown";
}

inline constexpr uint32_t k_all_mip_levels = ~0u;
inline constexpr uint32_t k_all_array_layers = ~0u;

struct TextureSubresources {
    uint32_t base_mip_level = 0;
    uint32_t mip_level_count = k_all_mip_levels;
    uint32_t base_array_layer = 0;
    uint32_t array_layer_count = k_all_array_layers;
};

// The RHI does not track resource states: barriers name both states explicitly (the render graph computes them).
struct BufferBarrier {
    BufferHandle buffer;
    ResourceState before = ResourceState::Undefined;
    ResourceState after = ResourceState::Undefined;
};

struct TextureBarrier {
    TextureHandle texture;
    ResourceState before = ResourceState::Undefined;
    ResourceState after = ResourceState::Undefined;
    TextureSubresources subresources;
};

// Part of one mip level of a texture, for copies. A zero extent means the whole mip level (from `offset`).
// Texel data in buffers is tightly packed (rows of extent.width texels, no padding).
struct TextureRegion {
    uint32_t mip_level = 0;
    uint32_t base_array_layer = 0;
    uint32_t array_layer_count = 1;
    core::Offset3D offset;
    core::Extent3D extent{.width = 0, .height = 0, .depth = 0};
};

enum class LoadOp : uint8_t {
    Load,     // keep the previous contents
    Clear,    // clear to the attachment's clear value
    DontCare, // contents are undefined (fastest when everything is overwritten)
};

enum class StoreOp : uint8_t {
    Store,    // keep the rendered contents
    DontCare, // contents may be discarded after the pass (e.g. depth used only during the pass)
};

struct ClearColor {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;
};

// A render target: mip level 0, array layer 0 of a texture with ColorAttachment usage, in ColorAttachment state.
struct ColorAttachment {
    TextureHandle texture;
    LoadOp load = LoadOp::Clear;
    StoreOp store = StoreOp::Store;
    ClearColor clear;
};

// Depth (and stencil) target: mip 0, layer 0 of a texture with DepthStencilAttachment usage, in DepthWrite state.
// Bytee uses reverse-Z (architecture §14): depth is cleared to 0 (far) and the depth test is GREATER.
struct DepthAttachment {
    TextureHandle texture;
    LoadOp load = LoadOp::Clear;
    StoreOp store = StoreOp::Store;
    float clear_depth = 0.0f;
    uint32_t clear_stencil = 0;
};

inline constexpr uint32_t k_max_color_attachments = 8;

// All attachments must have the same size; the render area is the whole attachment.
struct RenderPassDesc {
    std::span<const ColorAttachment> colors;
    std::optional<DepthAttachment> depth;
};

// Records GPU commands. Created by Device::begin_commands(), consumed by Device::submit(). Move-only.
// A command list may be recorded on any thread (one thread at a time); different lists record in parallel without
// locking. Destroying a list without submitting it discards its commands.
// Recording functions are hot paths: no allocation; misuse (invalid handles, wrong states) asserts in Debug.
class BYTEE_RHI_API CommandList {
private:
    detail::DeviceImpl* device_ = nullptr;
    detail::CommandContext* context_ = nullptr;

public:
    CommandList() noexcept = default;
    ~CommandList();

    CommandList(CommandList&& other) noexcept
        : device_(std::exchange(other.device_, nullptr))
        , context_(std::exchange(other.context_, nullptr)) {}
    CommandList& operator=(CommandList&& other) noexcept;
    CommandList(const CommandList&) = delete;
    CommandList& operator=(const CommandList&) = delete;

    [[nodiscard]] bool is_valid() const noexcept { return context_ != nullptr; }

    // Records all transitions as one batch.
    void barrier(std::span<const BufferBarrier> buffers, std::span<const TextureBarrier> textures);

    // Dynamic rendering. Copies and barriers are not allowed between begin_render_pass() and end_render_pass().
    void begin_render_pass(const RenderPassDesc& desc);
    void end_render_pass();

    // `dst` must be in CopyDst, sources in CopySrc.
    void fill_buffer(BufferHandle dst, uint64_t offset, uint64_t size, uint32_t value);
    void copy_buffer(BufferHandle src, uint64_t src_offset, BufferHandle dst, uint64_t dst_offset, uint64_t size);
    void copy_buffer_to_texture(BufferHandle src, uint64_t src_offset, TextureHandle dst, const TextureRegion& region);
    void copy_texture_to_buffer(TextureHandle src, const TextureRegion& region, BufferHandle dst, uint64_t dst_offset);

private:
    friend class Device;
    friend detail::CommandContext* get_context(const CommandList& list) noexcept;

    CommandList(detail::DeviceImpl* device, detail::CommandContext* context) noexcept
        : device_(device)
        , context_(context) {}
};

} // namespace bytee::render::rhi
