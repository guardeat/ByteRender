#include "graph_compiler.hpp"

#include <algorithm>
#include <optional>
#include <string_view>

namespace bytee::render::detail {

core::Flags<rhi::TextureUsage> texture_usage(rhi::ResourceState state) noexcept {
    using rhi::ResourceState;
    using rhi::TextureUsage;
    switch (state) {
    case ResourceState::General: return TextureUsage::Storage | TextureUsage::Sampled;
    case ResourceState::CopySrc: return TextureUsage::CopySrc;
    case ResourceState::CopyDst: return TextureUsage::CopyDst;
    case ResourceState::ShaderRead: return TextureUsage::Sampled;
    case ResourceState::ShaderWrite: return TextureUsage::Storage;
    case ResourceState::ColorAttachment: return TextureUsage::ColorAttachment;
    case ResourceState::DepthWrite: return TextureUsage::DepthStencilAttachment;
    case ResourceState::DepthRead: return TextureUsage::DepthStencilAttachment | TextureUsage::Sampled;
    default: return {};
    }
}

core::Flags<rhi::BufferUsage> buffer_usage(rhi::ResourceState state) noexcept {
    using rhi::BufferUsage;
    using rhi::ResourceState;
    switch (state) {
    case ResourceState::General:
    case ResourceState::ShaderRead:
    case ResourceState::ShaderWrite: return BufferUsage::Storage;
    case ResourceState::CopySrc: return BufferUsage::CopySrc;
    case ResourceState::CopyDst: return BufferUsage::CopyDst;
    case ResourceState::VertexBuffer: return BufferUsage::Vertex;
    case ResourceState::IndexBuffer: return BufferUsage::Index;
    case ResourceState::IndirectBuffer: return BufferUsage::Indirect;
    case ResourceState::UniformBuffer: return BufferUsage::Uniform;
    default: return {};
    }
}

core::Result<void> GraphCompiler::compile(const GraphPasses& passes, const GraphResources& resources,
                                          const rhi::Device& device, GraphSchedule& schedule) {
    schedule.clear();
    schedule.passes.resize(passes.count());
    schedule.textures.resize(resources.texture_count());
    schedule.buffers.resize(resources.buffer_count());

    reset_tracking(resources);
    cull(passes, resources, schedule);
    reset_tracking(resources); // state tracking starts over for the barrier pass
    for (uint32_t index = 0; index < passes.count(); ++index) {
        schedule.passes[index].first_barrier = static_cast<uint32_t>(schedule.barriers.size());
        if (schedule.passes[index].culled) {
            continue;
        }
        BYTEE_TRY(validate_pass(passes, passes.pass(index), resources, device));
        BYTEE_TRY(schedule_pass(passes, index, resources, schedule));
        schedule.passes[index].barrier_count =
            static_cast<uint32_t>(schedule.barriers.size()) - schedule.passes[index].first_barrier;
    }
    add_final_barriers(resources, schedule);
    return {};
}

// Transients start Undefined; imported resources start in `before`, and earlier work outside the graph counts as a
// write (so the first access always waits for it).
void GraphCompiler::reset_tracking(const GraphResources& resources) {
    texture_tracking_.resize(resources.texture_count());
    buffer_tracking_.resize(resources.buffer_count());
    for (uint32_t i = 0; i < resources.texture_count(); ++i) {
        const GraphResources::Texture& texture = resources.texture(i);
        texture_tracking_[i] =
            texture.imported ? Tracking{.state = texture.states.before, .written = true} : Tracking{};
    }
    for (uint32_t i = 0; i < resources.buffer_count(); ++i) {
        const GraphResources::Buffer& buffer = resources.buffer(i);
        buffer_tracking_[i] = buffer.imported ? Tracking{.state = buffer.states.before, .written = true} : Tracking{};
    }
}

// Last pass first: a pass executes if it writes an imported resource or something a later executing pass uses.
// Everything an executing pass touches is needed (writes too: they may update only part of a resource).
void GraphCompiler::cull(const GraphPasses& passes, const GraphResources& resources, GraphSchedule& schedule) {
    for (uint32_t index = passes.count(); index-- > 0;) {
        const std::span<const ResourceAccess> accesses = passes.accesses(passes.pass(index));
        const bool culled = std::ranges::none_of(accesses, [&](const ResourceAccess& access) {
            return access.writes && (resources.is_imported(access.kind, access.resource) ||
                                     tracking(access.kind, access.resource).needed);
        });
        schedule.passes[index].culled = culled;
        if (!culled) {
            for (const ResourceAccess& access : accesses) {
                tracking(access.kind, access.resource).needed = true;
            }
        }
    }
}

core::Result<void> GraphCompiler::schedule_pass(const GraphPasses& passes, uint32_t index,
                                                const GraphResources& resources, GraphSchedule& schedule) {
    const GraphPasses::Pass& pass = passes.pass(index);
    const std::string_view pass_name = passes.name(pass);
    for (const ResourceAccess& access : passes.accesses(pass)) {
        const bool imported = resources.is_imported(access.kind, access.resource);
        const std::string_view resource_name = resources.name(access.kind, access.resource);
        if (!imported && tracking(access.kind, access.resource).state == rhi::ResourceState::Undefined &&
            !access.writes) {
            return core::make_error(core::ErrorCode::InvalidArgument,
                                    "render graph: pass '{}' reads '{}' before any pass writes it", pass_name,
                                    resource_name);
        }
        if (access.kind == ResourceKind::Texture) {
            GraphSchedule::TextureUse& use = schedule.textures[access.resource];
            use.used = true;
            if (!imported) {
                const core::Flags<rhi::TextureUsage> usage = texture_usage(access.state);
                if (usage.none()) {
                    return core::make_error(core::ErrorCode::InvalidArgument,
                                            "render graph: pass '{}' uses transient texture '{}' in state {}, which "
                                            "only imported textures can be in",
                                            pass_name, resource_name, rhi::to_string(access.state));
                }
                use.usage |= usage;
            }
        } else {
            GraphSchedule::BufferUse& use = schedule.buffers[access.resource];
            use.used = true;
            if (!imported) {
                const core::Flags<rhi::BufferUsage> usage = buffer_usage(access.state);
                if (usage.none()) {
                    return core::make_error(core::ErrorCode::InvalidArgument,
                                            "render graph: pass '{}' uses transient buffer '{}' in state {}, which "
                                            "only imported buffers can be in",
                                            pass_name, resource_name, rhi::to_string(access.state));
                }
                use.usage |= usage;
            }
        }
        add_barrier(schedule, access.kind, access.resource, access.state, access.writes);
    }
    return {};
}

// Imported resources end in their `after` state, also those no executing pass touched.
void GraphCompiler::add_final_barriers(const GraphResources& resources, GraphSchedule& schedule) {
    schedule.first_final_barrier = static_cast<uint32_t>(schedule.barriers.size());
    for (uint32_t i = 0; i < resources.texture_count(); ++i) {
        if (resources.texture(i).imported) {
            add_barrier(schedule, ResourceKind::Texture, i, resources.texture(i).states.after, false);
        }
    }
    for (uint32_t i = 0; i < resources.buffer_count(); ++i) {
        if (resources.buffer(i).imported) {
            add_barrier(schedule, ResourceKind::Buffer, i, resources.buffer(i).states.after, false);
        }
    }
}

// Same state with reads on both sides: nothing to wait for. Otherwise the state changes, or a write must be made
// visible to the next access (or protected from a later write).
void GraphCompiler::add_barrier(GraphSchedule& schedule, ResourceKind kind, uint32_t index, rhi::ResourceState state,
                                bool writes) {
    Tracking& current = tracking(kind, index);
    if (current.state != state || current.written || writes) {
        schedule.barriers.push_back({.kind = kind, .resource = index, .before = current.state, .after = state});
    }
    current.state = state;
    current.written = writes;
}

core::Result<void> validate_pass(const GraphPasses& passes, const GraphPasses::Pass& pass,
                                 const GraphResources& resources, const rhi::Device& device) {
    const std::string_view pass_name = passes.name(pass);

    // Render targets of one pass share the render area.
    std::optional<uint32_t> first_attachment;
    const auto check_size = [&](TextureRef texture) -> core::Result<void> {
        if (!first_attachment) {
            first_attachment = texture.index();
            return {};
        }
        const GraphResources::Texture& first = resources.texture(*first_attachment);
        const GraphResources::Texture& other = resources.texture(texture.index());
        if (first.desc.extent.width != other.desc.extent.width ||
            first.desc.extent.height != other.desc.extent.height) {
            return core::make_error(
                core::ErrorCode::InvalidArgument,
                "render graph: pass '{}' has attachments of different sizes ('{}' {}x{}, '{}' {}x{})", pass_name,
                resources.name(first), first.desc.extent.width, first.desc.extent.height, resources.name(other),
                other.desc.extent.width, other.desc.extent.height);
        }
        return {};
    };
    for (const ColorAttachment& color : passes.colors(pass)) {
        BYTEE_TRY(check_size(color.texture));
    }
    if (pass.depth) {
        BYTEE_TRY(check_size(pass.depth->texture));
    }

    // Imported resources must have been created with the usage their states need (transients get it automatically).
    for (const ResourceAccess& access : passes.accesses(pass)) {
        if (access.state == rhi::ResourceState::General || !resources.is_imported(access.kind, access.resource)) {
            continue;
        }
        bool supported = true;
        if (access.kind == ResourceKind::Texture) {
            // DepthRead may be depth testing only: sampling usage is the RHI's to check.
            const core::Flags<rhi::TextureUsage> needed = access.state == rhi::ResourceState::DepthRead
                                                              ? rhi::TextureUsage::DepthStencilAttachment
                                                              : texture_usage(access.state);
            supported = device.info(*resources.texture(access.resource).imported).usage.has(needed);
        } else {
            supported = device.info(*resources.buffer(access.resource).imported).usage.has(buffer_usage(access.state));
        }
        if (!supported) {
            return core::make_error(core::ErrorCode::InvalidArgument,
                                    "render graph: pass '{}' uses '{}' as {}, but it was created without the usage "
                                    "flags that state needs",
                                    pass_name, resources.name(access.kind, access.resource),
                                    rhi::to_string(access.state));
        }
    }
    return {};
}

} // namespace bytee::render::detail
