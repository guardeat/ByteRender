#pragma once

// Render graph: each frame, passes declare which resources they read and write; the graph culls passes nobody needs,
// places every barrier and provides transient resources (reused across frames).
//
// A pass is any class with `setup(PassBuilder&)` and `execute(PassContext&)` (the RenderPass concept). Passes are
// long-lived objects that own their pipelines (created in the constructor, released by their destructor); the graph
// is rebuilt every frame and only refers to them:
//
//     graph.reset();
//     graph.add_pass("Scene", scene);           // runs scene.setup() right away
//     blur.set_input(scene.output());
//     graph.add_pass("Blur", blur);
//     device.present(swapchain, graph.record().value());   // or graph.execute() to submit
//
// Resource references (TextureRef / BufferRef) are valid in the graph that created them until its next reset().

#include <bytee/core/result.hpp>
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
class PassContext;

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

// Read-only views of a built graph (for compilation, debugging and tests). Names are valid until the graph changes.
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

template <class Pass_>
concept RenderPass = requires(Pass_& pass, PassBuilder& builder, PassContext& context) {
    pass.setup(builder);
    pass.execute(context);
};

class BYTEE_GRAPH_API RenderGraph {
private:
    friend class PassBuilder;
    friend class PassContext;

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
        uint32_t first_barrier = 0;
        uint32_t barrier_count = 0;
        bool culled = false;
    };

    struct TextureEntry {
        Name name;
        TextureDesc desc;
        std::optional<rhi::TextureHandle> imported;
        ImportedStates states;
        core::Flags<rhi::TextureUsage> usage;
        bool used = false;
        rhi::TextureHandle handle; // resolved by record()
    };

    struct BufferEntry {
        Name name;
        BufferDesc desc;
        std::optional<rhi::BufferHandle> imported;
        ImportedStates states;
        core::Flags<rhi::BufferUsage> usage;
        bool used = false;
        rhi::BufferHandle handle; // resolved by record()
    };

    // Per-resource state while compiling.
    struct Tracking {
        rhi::ResourceState state = rhi::ResourceState::Undefined;
        bool written = false; // the last access wrote (or, for imported resources, unknown work outside the graph)
        bool needed = false;  // culling: read or written by a pass that executes
    };

    // Transient resources survive reset(): a frame reuses a free one with the same shape and usage. Reuse across
    // frames is safe on one queue: the first barrier of a transient (from Undefined) waits for all earlier work.
    struct PooledTexture {
        TextureDesc desc;
        core::Flags<rhi::TextureUsage> usage;
        rhi::Texture texture;
        uint64_t last_used_frame = 0;
        bool in_use = false;
    };

    struct PooledBuffer {
        BufferDesc desc;
        core::Flags<rhi::BufferUsage> usage;
        rhi::Buffer buffer;
        uint64_t last_used_frame = 0;
        bool in_use = false;
    };

    // Flat arrays, cleared (capacity kept) by reset(): rebuilding the graph every frame does not allocate once warm.
    rhi::Device* device_ = nullptr;
    std::string names_;
    std::vector<PassEntry> passes_;
    std::vector<ResourceAccess> accesses_;
    std::vector<ColorAttachment> colors_;
    std::vector<TextureEntry> textures_;
    std::vector<BufferEntry> buffers_;
    std::vector<GraphBarrier> barriers_;
    uint32_t first_final_barrier_ = 0;
    std::vector<Tracking> texture_tracking_;
    std::vector<Tracking> buffer_tracking_;
    std::vector<rhi::TextureBarrier> texture_barriers_; // scratch for record()
    std::vector<rhi::BufferBarrier> buffer_barriers_;
    std::vector<PooledTexture> pooled_textures_;
    std::vector<PooledBuffer> pooled_buffers_;
    uint64_t frame_ = 0;
    bool compiled_ = false;
    bool recorded_ = false;

public:
    // Pooled transients unused for this many frames are released.
    static constexpr uint64_t k_transient_keep_frames = 4;

    explicit RenderGraph(rhi::Device& device);

    // Starts a new frame: forgets all passes and resources (references become invalid), keeps the storage and the
    // transient pool.
    void reset() noexcept;

    // Resources created outside the graph. `states` are the states before and after the graph.
    [[nodiscard]] TextureRef import_texture(std::string_view name, rhi::TextureHandle texture, ImportedStates states);
    [[nodiscard]] BufferRef import_buffer(std::string_view name, rhi::BufferHandle buffer, ImportedStates states);

    // Adds `pass` and runs its setup() immediately, so its outputs can feed the next pass. `pass` must stay alive until
    // the graph has executed. Passes execute in the order they are added.
    template <RenderPass Pass_>
    void add_pass(std::string_view name, Pass_& pass);

    // Culls passes, validates resource use (InvalidArgument on errors: reads before writes, transients in host-only
    // states, attachments of different sizes, imported resources without the usage a state needs) and computes
    // barriers and transient usages. Runs on the CPU only; record() calls it when the graph changed.
    [[nodiscard]] core::Result<void> compile();

    // Creates (or reuses) transient resources and records every pass that executes, with its barriers and render
    // pass, into one command list (each pass inside a debug label with its name); imported resources end in their
    // `after` state. Submit or present the list. Once per reset().
    [[nodiscard]] core::Result<rhi::CommandList> record();

    // record() and submit.
    [[nodiscard]] core::Result<rhi::SyncPoint> execute();

    [[nodiscard]] uint32_t pass_count() const noexcept { return static_cast<uint32_t>(passes_.size()); }
    [[nodiscard]] uint32_t texture_count() const noexcept { return static_cast<uint32_t>(textures_.size()); }
    [[nodiscard]] uint32_t buffer_count() const noexcept { return static_cast<uint32_t>(buffers_.size()); }

    [[nodiscard]] PassView pass(uint32_t index) const;
    [[nodiscard]] TextureView texture(TextureRef texture) const;
    [[nodiscard]] BufferView buffer(BufferRef buffer) const;
    [[nodiscard]] std::span<const GraphBarrier> final_barriers() const noexcept; // after compile()

    // The graph in Graphviz DOT format (render with `dot -Tsvg graph.dot -o graph.svg`): passes as boxes in execution
    // order, resources as ellipses (imported ones bold), edges labeled with states. After compile(), culled passes
    // are greyed out and dashed.
    [[nodiscard]] std::string to_graphviz() const;
    [[nodiscard]] uint32_t pooled_texture_count() const noexcept {
        return static_cast<uint32_t>(pooled_textures_.size());
    }
    [[nodiscard]] uint32_t pooled_buffer_count() const noexcept {
        return static_cast<uint32_t>(pooled_buffers_.size());
    }

private:
    [[nodiscard]] PassBuilder begin_pass(std::string_view name, void* pass, ExecuteFunction execute);
    [[nodiscard]] Name store_name(std::string_view name);
    [[nodiscard]] std::string_view name_of(Name name) const noexcept;
    void add_barrier(ResourceKind kind, uint32_t resource, Tracking& tracking, rhi::ResourceState state, bool writes);
    [[nodiscard]] core::Result<void> validate_pass(const PassEntry& pass) const;
    [[nodiscard]] core::Result<void> acquire_transients();
    void record_barriers(rhi::CommandList& commands, std::span<const GraphBarrier> barriers);
    void record_pass(rhi::CommandList& commands, const PassEntry& pass);
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

// What a pass gets in execute(): the command list and the GPU resources behind its references. Valid only during
// that call; render targets are already bound (begin_render_pass) for passes with attachments.
class BYTEE_GRAPH_API PassContext {
private:
    friend class RenderGraph;

    const RenderGraph& graph_;
    rhi::CommandList& commands_;

public:
    PassContext(const PassContext&) = delete;
    PassContext& operator=(const PassContext&) = delete;

    [[nodiscard]] rhi::CommandList& commands() const noexcept { return commands_; }
    [[nodiscard]] rhi::Device& device() const noexcept { return *graph_.device_; }
    [[nodiscard]] rhi::TextureHandle texture(TextureRef texture) const;
    [[nodiscard]] rhi::BufferHandle buffer(BufferRef buffer) const;
    [[nodiscard]] const TextureDesc& desc(TextureRef texture) const;
    [[nodiscard]] const BufferDesc& desc(BufferRef buffer) const;

private:
    PassContext(const RenderGraph& graph, rhi::CommandList& commands) noexcept
        : graph_(graph)
        , commands_(commands) {}
};

template <RenderPass Pass_>
void RenderGraph::add_pass(std::string_view name, Pass_& pass) {
    PassBuilder builder = begin_pass(
        name, &pass, [](void* erased, PassContext& context) { static_cast<Pass_*>(erased)->execute(context); });
    pass.setup(builder);
    compiled_ = false;
}

} // namespace bytee::render
