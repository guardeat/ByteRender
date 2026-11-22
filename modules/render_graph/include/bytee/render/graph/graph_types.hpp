#pragma once

// Value types of the render graph: resource references and descriptions, attachments, and the read-only views a built
// graph exposes.

#include <bytee/core/flags.hpp>
#include <bytee/core/types.hpp>
#include <bytee/render/rhi/buffer.hpp>
#include <bytee/render/rhi/command_list.hpp>
#include <bytee/render/rhi/handles.hpp>
#include <bytee/render/rhi/texture.hpp>

#include <compare>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace bytee::render {

inline constexpr uint32_t k_invalid_graph_index = ~0u;

// Reference to a graph resource: an index into the graph that created it, valid until its next reset().
template <class Tag_>
class GraphRef {
private:
    uint32_t index_ = k_invalid_graph_index;

public:
    constexpr GraphRef() noexcept = default;
    constexpr explicit GraphRef(uint32_t index) noexcept
        : index_(index) {}

    [[nodiscard]] constexpr bool is_valid() const noexcept { return index_ != k_invalid_graph_index; }
    [[nodiscard]] constexpr uint32_t index() const noexcept { return index_; }

    friend constexpr bool operator==(GraphRef, GraphRef) noexcept = default;
};

struct GraphTextureTag;
struct GraphBufferTag;
using TextureRef = GraphRef<GraphTextureTag>;
using BufferRef = GraphRef<GraphBufferTag>;

// Shape of a graph texture. Transient textures get their usage flags from the accesses declared on them.
struct TextureDesc {
    rhi::TextureDimension dimension = rhi::TextureDimension::Texture2D;
    core::Extent3D extent;
    uint32_t mip_levels = 1;
    uint32_t array_layers = 1;
    rhi::Format format = rhi::Format::Undefined;

    friend constexpr bool operator==(const TextureDesc&, const TextureDesc&) noexcept = default;
};

// Transient buffers live in device memory; CPU-visible buffers are imported.
struct BufferDesc {
    uint64_t size = 0;

    friend constexpr bool operator==(const BufferDesc&, const BufferDesc&) noexcept = default;
};

// States of an imported resource at the graph's boundaries: the graph transitions it from `before` on first use and
// leaves it in `after` when the graph has executed.
struct ImportedStates {
    rhi::ResourceState before = rhi::ResourceState::Undefined;
    rhi::ResourceState after = rhi::ResourceState::ShaderRead;
};

// Render targets of a pass: the graph begins and ends the render pass around execute().
struct ColorAttachment {
    TextureRef texture;
    rhi::LoadOp load = rhi::LoadOp::Clear;
    rhi::StoreOp store = rhi::StoreOp::Store;
    rhi::ClearColor clear;
};

struct DepthAttachment {
    TextureRef texture;
    rhi::LoadOp load = rhi::LoadOp::Clear;
    rhi::StoreOp store = rhi::StoreOp::Store;
    float clear_depth = 0.0f; // reverse-Z: 0 is far
    uint32_t clear_stencil = 0;
};

enum class ResourceKind : uint8_t {
    Texture,
    Buffer,
};

// One resource used by one pass, in one state.
struct ResourceAccess {
    ResourceKind kind = ResourceKind::Texture;
    uint32_t resource = k_invalid_graph_index; // TextureRef / BufferRef index
    rhi::ResourceState state = rhi::ResourceState::Undefined;
    bool writes = false;

    friend constexpr bool operator==(const ResourceAccess&, const ResourceAccess&) noexcept = default;
};

// A state transition the graph records before a pass (or after the last one, for imported resources).
struct GraphBarrier {
    ResourceKind kind = ResourceKind::Texture;
    uint32_t resource = k_invalid_graph_index;
    rhi::ResourceState before = rhi::ResourceState::Undefined;
    rhi::ResourceState after = rhi::ResourceState::Undefined;

    friend constexpr bool operator==(const GraphBarrier&, const GraphBarrier&) noexcept = default;
};

// Read-only views of a built graph (debugging, tools, tests). Valid until the graph changes.
struct PassView {
    std::string_view name;
    std::span<const ResourceAccess> accesses;
    std::span<const ColorAttachment> colors;
    std::optional<DepthAttachment> depth;
    bool culled = false;                    // after compile(): nothing needs its results, it does not execute
    std::span<const GraphBarrier> barriers; // after compile(): recorded before the pass
};

struct TextureView {
    std::string_view name;
    TextureDesc desc;
    std::optional<rhi::TextureHandle> imported; // empty for transient textures
    ImportedStates states;                      // imported textures only
    core::Flags<rhi::TextureUsage> usage;       // after compile(): transient textures only
    bool used = false;                          // after compile(): accessed by a pass that executes
};

struct BufferView {
    std::string_view name;
    BufferDesc desc;
    std::optional<rhi::BufferHandle> imported; // empty for transient buffers
    ImportedStates states;                     // imported buffers only
    core::Flags<rhi::BufferUsage> usage;       // after compile(): transient buffers only
    bool used = false;                         // after compile(): accessed by a pass that executes
};

} // namespace bytee::render
