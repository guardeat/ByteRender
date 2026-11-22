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

#include <bytee/core/compiler.hpp>
#include <bytee/core/result.hpp>
#include <bytee/render/graph/export.hpp>
#include <bytee/render/graph/graph_types.hpp>
#include <bytee/render/rhi/device.hpp>

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace bytee::render {

class PassBuilder;
class PassContext;

namespace detail {
struct BYTEE_HIDDEN GraphState; // the graph's internals (src/graph_state.hpp)
} // namespace detail

template <class Pass_>
concept RenderPass = requires(Pass_& pass, PassBuilder& builder, PassContext& context) {
    pass.setup(builder);
    pass.execute(context);
};

class BYTEE_GRAPH_API RenderGraph {
public:
    // Type-erased RenderPass::execute: the graph stores a pointer to the pass plus this function, no virtual calls.
    using ExecuteFunction = void (*)(void* pass, PassContext& context);

private:
    std::unique_ptr<detail::GraphState> state_;

public:
    // Pooled transients unused for this many frames are released.
    static constexpr uint64_t k_transient_keep_frames = 4;

    explicit RenderGraph(rhi::Device& device);
    ~RenderGraph();
    RenderGraph(RenderGraph&& other) noexcept;
    RenderGraph& operator=(RenderGraph&& other) noexcept;

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

    [[nodiscard]] uint32_t pass_count() const noexcept;
    [[nodiscard]] uint32_t texture_count() const noexcept;
    [[nodiscard]] uint32_t buffer_count() const noexcept;
    [[nodiscard]] PassView pass(uint32_t index) const;
    [[nodiscard]] TextureView texture(TextureRef texture) const;
    [[nodiscard]] BufferView buffer(BufferRef buffer) const;
    [[nodiscard]] std::span<const GraphBarrier> final_barriers() const noexcept; // after compile()
    [[nodiscard]] uint32_t pooled_texture_count() const noexcept;
    [[nodiscard]] uint32_t pooled_buffer_count() const noexcept;

    // The graph in Graphviz DOT format (render with `dot -Tsvg graph.dot -o graph.svg`): passes as boxes in execution
    // order, resources as ellipses (imported ones bold), edges labeled with states. After compile(), culled passes
    // are greyed out and dashed.
    [[nodiscard]] std::string to_graphviz() const;

private:
    [[nodiscard]] PassBuilder begin_pass(std::string_view name, void* pass, ExecuteFunction execute);
};

// Declares what a pass uses. Handed to RenderPass::setup(); valid only during that call.
class BYTEE_GRAPH_API PassBuilder {
private:
    friend class RenderGraph;

    detail::GraphState& state_;
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
    PassBuilder(detail::GraphState& state, uint32_t pass_index) noexcept
        : state_(state)
        , pass_index_(pass_index) {}

    void access(ResourceKind kind, uint32_t resource, rhi::ResourceState state, bool writes);
};

// What a pass gets in execute(): the command list and the GPU resources behind its references. Valid only during
// that call; render targets are already bound (begin_render_pass) for passes with attachments.
class BYTEE_GRAPH_API PassContext {
private:
    friend struct detail::GraphState;

    const detail::GraphState& state_;
    rhi::CommandList& commands_;

public:
    PassContext(const PassContext&) = delete;
    PassContext& operator=(const PassContext&) = delete;

    [[nodiscard]] rhi::CommandList& commands() const noexcept { return commands_; }
    [[nodiscard]] rhi::Device& device() const noexcept;
    [[nodiscard]] rhi::TextureHandle texture(TextureRef texture) const;
    [[nodiscard]] rhi::BufferHandle buffer(BufferRef buffer) const;
    [[nodiscard]] const TextureDesc& desc(TextureRef texture) const;
    [[nodiscard]] const BufferDesc& desc(BufferRef buffer) const;

private:
    PassContext(const detail::GraphState& state, rhi::CommandList& commands) noexcept
        : state_(state)
        , commands_(commands) {}
};

template <RenderPass Pass_>
void RenderGraph::add_pass(std::string_view name, Pass_& pass) {
    PassBuilder builder = begin_pass(
        name, &pass, [](void* erased, PassContext& context) { static_cast<Pass_*>(erased)->execute(context); });
    pass.setup(builder);
}

} // namespace bytee::render
