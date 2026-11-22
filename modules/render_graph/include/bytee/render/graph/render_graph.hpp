#pragma once

// Render graph: each frame, passes declare which resources they read and write; the graph derives execution order,
// barriers and transient resources from that (compile and execute: roadmap step 15).
//
// A pass is any class with `setup(PassBuilder&)` and `execute(PassContext&)` (the RenderPass concept). Passes are
// long-lived objects that own their pipelines (created in the constructor, released by their destructor); the graph
// is rebuilt every frame and only refers to them:
//
//     graph.reset();
//     graph.add_pass("Scene", scene);           // runs scene.setup() right away
//     blur.set_input(scene.output());
//     graph.add_pass("Blur", blur);
//
// Resource references (TextureRef / BufferRef) are valid in the graph that created them until its next reset().

#include <bytee/render/graph/export.hpp>
#include <bytee/render/rhi/command_list.hpp>
#include <bytee/render/rhi/device.hpp>

#include <compare>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace bytee::render {

class PassBuilder;
class PassContext; // defined with graph execution (roadmap step 15)

inline constexpr uint32_t k_invalid_graph_index = ~0u;

// Reference to a graph resource: an index into the graph that created it.
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
};

// Transient buffers live in device memory; CPU-visible buffers are imported.
struct BufferDesc {
    uint64_t size = 0;
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

// Read-only views of a built graph (for compilation, debugging and tests). Names are valid until the graph changes.
struct PassView {
    std::string_view name;
    std::span<const ResourceAccess> accesses;
    std::span<const ColorAttachment> colors;
    std::optional<DepthAttachment> depth;
};

struct TextureView {
    std::string_view name;
    TextureDesc desc;
    std::optional<rhi::TextureHandle> imported; // empty for transient textures
    ImportedStates states;                      // imported textures only
};

struct BufferView {
    std::string_view name;
    BufferDesc desc;
    std::optional<rhi::BufferHandle> imported; // empty for transient buffers
    ImportedStates states;                     // imported buffers only
};

template <class Pass_>
concept RenderPass = requires(Pass_& pass, PassBuilder& builder, PassContext& context) {
    pass.setup(builder);
    pass.execute(context);
};

class BYTEE_GRAPH_API RenderGraph {
private:
    friend class PassBuilder;

    using ExecuteFunction = void (*)(void* pass, PassContext& context);

    struct Name {
        uint32_t offset = 0;
        uint32_t size = 0;
    };

    struct PassEntry {
        Name name;
        void* pass = nullptr;
        ExecuteFunction execute = nullptr;
        uint32_t first_access = 0;
        uint32_t access_count = 0;
        uint32_t first_color = 0;
        uint32_t color_count = 0;
        std::optional<DepthAttachment> depth;
    };

    struct TextureEntry {
        Name name;
        TextureDesc desc;
        std::optional<rhi::TextureHandle> imported;
        ImportedStates states;
    };

    struct BufferEntry {
        Name name;
        BufferDesc desc;
        std::optional<rhi::BufferHandle> imported;
        ImportedStates states;
    };

    // Flat arrays, cleared (capacity kept) by reset(): rebuilding the graph every frame does not allocate once warm.
    rhi::Device* device_ = nullptr;
    std::string names_;
    std::vector<PassEntry> passes_;
    std::vector<ResourceAccess> accesses_;
    std::vector<ColorAttachment> colors_;
    std::vector<TextureEntry> textures_;
    std::vector<BufferEntry> buffers_;

public:
    explicit RenderGraph(rhi::Device& device);

    // Forgets all passes and resources (references become invalid); keeps the storage for the next frame.
    void reset() noexcept;

    // Resources created outside the graph. `states` are the states before and after the graph.
    [[nodiscard]] TextureRef import_texture(std::string_view name, rhi::TextureHandle texture, ImportedStates states);
    [[nodiscard]] BufferRef import_buffer(std::string_view name, rhi::BufferHandle buffer, ImportedStates states);

    // Adds `pass` and runs its setup() immediately, so its outputs can feed the next pass. `pass` must stay alive until
    // the graph has executed. Passes execute in the order they are added.
    template <RenderPass Pass_>
    void add_pass(std::string_view name, Pass_& pass);

    [[nodiscard]] uint32_t pass_count() const noexcept { return static_cast<uint32_t>(passes_.size()); }
    [[nodiscard]] uint32_t texture_count() const noexcept { return static_cast<uint32_t>(textures_.size()); }
    [[nodiscard]] uint32_t buffer_count() const noexcept { return static_cast<uint32_t>(buffers_.size()); }

    [[nodiscard]] PassView pass(uint32_t index) const;
    [[nodiscard]] TextureView texture(TextureRef texture) const;
    [[nodiscard]] BufferView buffer(BufferRef buffer) const;

private:
    [[nodiscard]] PassBuilder begin_pass(std::string_view name, void* pass, ExecuteFunction execute);
    [[nodiscard]] Name store_name(std::string_view name);
    [[nodiscard]] std::string_view name_of(Name name) const noexcept;
};

// Declares what a pass uses. Handed to RenderPass::setup(); valid only during that call.
class BYTEE_GRAPH_API PassBuilder {
private:
    friend class RenderGraph;

    RenderGraph& graph_;
    uint32_t pass_index_ = 0;

public:
    PassBuilder(const PassBuilder&) = delete;
    PassBuilder& operator=(const PassBuilder&) = delete;

    // Transient resources: created by the graph for the frame, written first by this pass.
    [[nodiscard]] TextureRef create_texture(std::string_view name, const TextureDesc& desc);
    [[nodiscard]] BufferRef create_buffer(std::string_view name, const BufferDesc& desc);

    // Each resource may be used once per pass, in one state.
    void read(TextureRef texture, rhi::ResourceState state = rhi::ResourceState::ShaderRead);
    void write(TextureRef texture, rhi::ResourceState state = rhi::ResourceState::ShaderWrite);
    void read(BufferRef buffer, rhi::ResourceState state = rhi::ResourceState::ShaderRead);
    void write(BufferRef buffer, rhi::ResourceState state = rhi::ResourceState::ShaderWrite);

    // Render targets (written in ColorAttachment / DepthWrite). A pass with attachments is a raster pass.
    void color_attachment(const ColorAttachment& attachment);
    void depth_attachment(const DepthAttachment& attachment);

    [[nodiscard]] const TextureDesc& desc(TextureRef texture) const;
    [[nodiscard]] const BufferDesc& desc(BufferRef buffer) const;

private:
    PassBuilder(RenderGraph& graph, uint32_t pass_index) noexcept
        : graph_(graph)
        , pass_index_(pass_index) {}

    void access(ResourceKind kind, uint32_t resource, rhi::ResourceState state, bool writes);
};

template <RenderPass Pass_>
void RenderGraph::add_pass(std::string_view name, Pass_& pass) {
    PassBuilder builder = begin_pass(
        name, &pass, [](void* erased, PassContext& context) { static_cast<Pass_*>(erased)->execute(context); });
    pass.setup(builder);
}

} // namespace bytee::render
