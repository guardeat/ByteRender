#pragma once

#include "graph_passes.hpp"
#include "graph_resources.hpp"

#include <bytee/core/result.hpp>
#include <bytee/render/graph/graph_types.hpp>
#include <bytee/render/rhi/device.hpp>

#include <cstdint>
#include <span>
#include <vector>

namespace bytee::render::detail {

// What compile() derives from the declarations: which passes execute, which resources they use (and so must exist,
// with which usage), and the barriers before each pass and after the last one.
struct GraphSchedule {
    struct PassSchedule {
        bool culled = false;
        uint32_t first_barrier = 0;
        uint32_t barrier_count = 0;
    };

    struct TextureUse {
        bool used = false;
        core::Flags<rhi::TextureUsage> usage; // transients only
    };

    struct BufferUse {
        bool used = false;
        core::Flags<rhi::BufferUsage> usage; // transients only
    };

    std::vector<PassSchedule> passes;
    std::vector<TextureUse> textures;
    std::vector<BufferUse> buffers;
    std::vector<GraphBarrier> barriers;
    uint32_t first_final_barrier = 0;

    void clear() noexcept {
        passes.clear();
        textures.clear();
        buffers.clear();
        barriers.clear();
        first_final_barrier = 0;
    }

    [[nodiscard]] bool is_used(ResourceKind kind, uint32_t index) const noexcept {
        return kind == ResourceKind::Texture ? textures[index].used : buffers[index].used;
    }
    [[nodiscard]] std::span<const GraphBarrier> barriers_before(uint32_t pass) const noexcept {
        return std::span{barriers}.subspan(passes[pass].first_barrier, passes[pass].barrier_count);
    }
    [[nodiscard]] std::span<const GraphBarrier> final_barriers() const noexcept {
        return std::span{barriers}.subspan(first_final_barrier);
    }
};

// Turns declared passes and resources into a GraphSchedule: culling, validation, transient usages and barriers.
// Keeps its per-resource scratch between frames (no allocation once warm).
class GraphCompiler {
private:
    struct Tracking {
        rhi::ResourceState state = rhi::ResourceState::Undefined;
        bool written = false; // the last access wrote (or, for imported resources, unknown work outside the graph)
        bool needed = false;  // culling: read or written by a pass that executes
    };

    std::vector<Tracking> texture_tracking_;
    std::vector<Tracking> buffer_tracking_;

public:
    // InvalidArgument (naming the pass and resource) for misuse the declarations alone cannot rule out.
    [[nodiscard]] core::Result<void> compile(const GraphPasses& passes, const GraphResources& resources,
                                             const rhi::Device& device, GraphSchedule& schedule);

private:
    [[nodiscard]] Tracking& tracking(ResourceKind kind, uint32_t index) noexcept {
        return kind == ResourceKind::Texture ? texture_tracking_[index] : buffer_tracking_[index];
    }

    void reset_tracking(const GraphResources& resources);
    void cull(const GraphPasses& passes, const GraphResources& resources, GraphSchedule& schedule);
    [[nodiscard]] core::Result<void> schedule_pass(const GraphPasses& passes, uint32_t index,
                                                   const GraphResources& resources, GraphSchedule& schedule);
    void add_final_barriers(const GraphResources& resources, GraphSchedule& schedule);
    void add_barrier(GraphSchedule& schedule, ResourceKind kind, uint32_t index, rhi::ResourceState state, bool writes);
};

// Checks that need the RHI: attachment sizes and the usage flags of imported resources.
[[nodiscard]] core::Result<void> validate_pass(const GraphPasses& passes, const GraphPasses::Pass& pass,
                                               const GraphResources& resources, const rhi::Device& device);

// Usage flags a transient needs for a state; none for states only imported resources can be in.
[[nodiscard]] core::Flags<rhi::TextureUsage> texture_usage(rhi::ResourceState state) noexcept;
[[nodiscard]] core::Flags<rhi::BufferUsage> buffer_usage(rhi::ResourceState state) noexcept;

} // namespace bytee::render::detail
