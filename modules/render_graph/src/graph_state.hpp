#pragma once

#include "graph_compiler.hpp"
#include "graph_passes.hpp"
#include "graph_resources.hpp"
#include "transient_pool.hpp"

#include <bytee/core/result.hpp>
#include <bytee/render/graph/render_graph.hpp>

#include <vector>

namespace bytee::render {

namespace detail {

// The GPU handle behind each graph reference for the frame being recorded: imported handles, or transients from the
// pool.
struct GraphBindings {
    std::vector<rhi::TextureHandle> textures;
    std::vector<rhi::BufferHandle> buffers;
};

// Converts graph barriers into RHI barriers and records them as one batch. Keeps its scratch arrays between calls.
class BarrierBatch {
private:
    std::vector<rhi::TextureBarrier> textures_;
    std::vector<rhi::BufferBarrier> buffers_;

public:
    void record(rhi::CommandList& commands, std::span<const GraphBarrier> barriers, const GraphBindings& bindings);
};

} // namespace detail

// Everything behind a RenderGraph, composed of parts with one job each: declarations (resources, passes), their
// compilation (compiler -> schedule) and the frame's GPU side (transient pool, bindings, barrier recording).
struct BYTEE_HIDDEN detail::GraphState {
    rhi::Device* device = nullptr;
    GraphResources resources;
    GraphPasses passes;
    GraphCompiler compiler;
    GraphSchedule schedule;
    TransientPool transients{RenderGraph::k_transient_keep_frames};
    GraphBindings bindings;
    BarrierBatch barrier_batch;
    bool compiled = false; // the schedule matches the declarations
    bool recorded = false; // record() ran since the last reset()

    [[nodiscard]] core::Result<void> compile();
    [[nodiscard]] core::Result<rhi::CommandList> record();

private:
    [[nodiscard]] core::Result<void> bind_resources();
    void record_pass(rhi::CommandList& commands, uint32_t index);
};

} // namespace bytee::render
