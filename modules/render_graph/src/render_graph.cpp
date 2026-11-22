#include <bytee/core/assert.hpp>
#include <bytee/render/graph/render_graph.hpp>

#include <algorithm>
#include <array>
#include <iterator>

namespace bytee::render {

namespace {

// Usage flags a transient needs for a state; none for states only imported resources can be in.
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

} // namespace

RenderGraph::RenderGraph(rhi::Device& device)
    : device_(&device) {
    BYTEE_ASSERT(device.is_valid(), "RenderGraph needs a valid Device");
}

void RenderGraph::reset() noexcept {
    ++frame_;
    // Release pooled transients that have not been used for a while (deferred: the GPU may still use them).
    std::erase_if(pooled_textures_, [this](const PooledTexture& pooled) {
        return pooled.last_used_frame + k_transient_keep_frames < frame_;
    });
    std::erase_if(pooled_buffers_, [this](const PooledBuffer& pooled) {
        return pooled.last_used_frame + k_transient_keep_frames < frame_;
    });
    for (PooledTexture& pooled : pooled_textures_) {
        pooled.in_use = false;
    }
    for (PooledBuffer& pooled : pooled_buffers_) {
        pooled.in_use = false;
    }
    compiled_ = false;
    recorded_ = false;
    barriers_.clear();
    first_final_barrier_ = 0;
    names_.clear();
    passes_.clear();
    accesses_.clear();
    colors_.clear();
    textures_.clear();
    buffers_.clear();
}

TextureRef RenderGraph::import_texture(std::string_view name, rhi::TextureHandle texture, ImportedStates states) {
    BYTEE_ASSERT(rhi::is_texture_state(states.before) && rhi::is_texture_state(states.after),
                 "imported texture '{}': {} -> {} are not texture states", name, rhi::to_string(states.before),
                 rhi::to_string(states.after));
    const rhi::TextureInfo& info = device_->info(texture);
    textures_.push_back({.name = store_name(name),
                         .desc = {.dimension = info.dimension,
                                  .extent = info.extent,
                                  .mip_levels = info.mip_levels,
                                  .array_layers = info.array_layers,
                                  .format = info.format},
                         .imported = texture,
                         .states = states,
                         .handle = texture});
    compiled_ = false;
    return TextureRef{static_cast<uint32_t>(textures_.size() - 1)};
}

BufferRef RenderGraph::import_buffer(std::string_view name, rhi::BufferHandle buffer, ImportedStates states) {
    BYTEE_ASSERT(rhi::is_buffer_state(states.before) && rhi::is_buffer_state(states.after),
                 "imported buffer '{}': {} -> {} are not buffer states", name, rhi::to_string(states.before),
                 rhi::to_string(states.after));
    buffers_.push_back({.name = store_name(name),
                        .desc = {.size = device_->info(buffer).size},
                        .imported = buffer,
                        .states = states,
                        .handle = buffer});
    compiled_ = false;
    return BufferRef{static_cast<uint32_t>(buffers_.size() - 1)};
}

PassView RenderGraph::pass(uint32_t index) const {
    BYTEE_ASSERT(index < passes_.size(), "pass index {} out of range [0, {})", index, passes_.size());
    const PassEntry& entry = passes_[index];
    return {.name = name_of(entry.name),
            .accesses = std::span{accesses_}.subspan(entry.first_access, entry.access_count),
            .colors = std::span{colors_}.subspan(entry.first_color, entry.color_count),
            .depth = entry.depth,
            .culled = entry.culled,
            .barriers = std::span{barriers_}.subspan(entry.first_barrier, entry.barrier_count)};
}

TextureView RenderGraph::texture(TextureRef texture) const {
    BYTEE_ASSERT(texture.index() < textures_.size(), "invalid TextureRef {}", texture.index());
    const TextureEntry& entry = textures_[texture.index()];
    return {.name = name_of(entry.name),
            .desc = entry.desc,
            .imported = entry.imported,
            .states = entry.states,
            .usage = entry.usage,
            .used = entry.used};
}

BufferView RenderGraph::buffer(BufferRef buffer) const {
    BYTEE_ASSERT(buffer.index() < buffers_.size(), "invalid BufferRef {}", buffer.index());
    const BufferEntry& entry = buffers_[buffer.index()];
    return {.name = name_of(entry.name),
            .desc = entry.desc,
            .imported = entry.imported,
            .states = entry.states,
            .usage = entry.usage,
            .used = entry.used};
}

std::span<const GraphBarrier> RenderGraph::final_barriers() const noexcept {
    return std::span{barriers_}.subspan(first_final_barrier_);
}

core::Result<void> RenderGraph::compile() {
    BYTEE_ASSERT(!recorded_, "RenderGraph::compile: the graph was already recorded; reset() first");
    barriers_.clear();
    texture_tracking_.assign(textures_.size(), Tracking{});
    buffer_tracking_.assign(buffers_.size(), Tracking{});
    const auto tracking_of = [this](const ResourceAccess& access) -> Tracking& {
        return access.kind == ResourceKind::Texture ? texture_tracking_[access.resource]
                                                    : buffer_tracking_[access.resource];
    };
    const auto is_imported = [this](const ResourceAccess& access) {
        return access.kind == ResourceKind::Texture ? textures_[access.resource].imported.has_value()
                                                    : buffers_[access.resource].imported.has_value();
    };

    // Culling, last pass first: a pass executes if it writes an imported resource or something a later executing
    // pass uses. Everything an executing pass touches is needed (writes too: they may only update part of it).
    for (auto pass = passes_.rbegin(); pass != passes_.rend(); ++pass) {
        const std::span<const ResourceAccess> accesses =
            std::span{accesses_}.subspan(pass->first_access, pass->access_count);
        pass->culled = std::ranges::none_of(accesses, [&](const ResourceAccess& access) {
            return access.writes && (is_imported(access) || tracking_of(access).needed);
        });
        if (!pass->culled) {
            for (const ResourceAccess& access : accesses) {
                tracking_of(access).needed = true;
            }
        }
    }

    // State tracking starts from Undefined (transients) or the `before` state (imported; earlier work outside the
    // graph counts as a write).
    for (std::size_t i = 0; i < textures_.size(); ++i) {
        textures_[i].used = false;
        textures_[i].usage = {};
        texture_tracking_[i] =
            textures_[i].imported ? Tracking{.state = textures_[i].states.before, .written = true} : Tracking{};
    }
    for (std::size_t i = 0; i < buffers_.size(); ++i) {
        buffers_[i].used = false;
        buffers_[i].usage = {};
        buffer_tracking_[i] =
            buffers_[i].imported ? Tracking{.state = buffers_[i].states.before, .written = true} : Tracking{};
    }

    // Validation, usages and barriers, in execution order.
    for (PassEntry& pass : passes_) {
        pass.first_barrier = static_cast<uint32_t>(barriers_.size());
        pass.barrier_count = 0;
        if (pass.culled) {
            continue;
        }
        const std::string_view pass_name = name_of(pass.name);
        for (const ResourceAccess& access : std::span{accesses_}.subspan(pass.first_access, pass.access_count)) {
            Tracking& tracking = tracking_of(access);
            const bool imported = is_imported(access);
            const std::string_view resource_name = access.kind == ResourceKind::Texture
                                                       ? name_of(textures_[access.resource].name)
                                                       : name_of(buffers_[access.resource].name);
            if (!imported && tracking.state == rhi::ResourceState::Undefined && !access.writes) {
                return core::make_error(core::ErrorCode::InvalidArgument,
                                        "render graph: pass '{}' reads '{}' before any pass writes it", pass_name,
                                        resource_name);
            }
            if (access.kind == ResourceKind::Texture) {
                TextureEntry& texture = textures_[access.resource];
                texture.used = true;
                if (!imported) {
                    const core::Flags<rhi::TextureUsage> usage = texture_usage(access.state);
                    if (usage.none()) {
                        return core::make_error(core::ErrorCode::InvalidArgument,
                                                "render graph: pass '{}' uses transient texture '{}' in state {}, "
                                                "which only imported textures can be in",
                                                pass_name, resource_name, rhi::to_string(access.state));
                    }
                    texture.usage |= usage;
                }
            } else {
                BufferEntry& buffer = buffers_[access.resource];
                buffer.used = true;
                if (!imported) {
                    const core::Flags<rhi::BufferUsage> usage = buffer_usage(access.state);
                    if (usage.none()) {
                        return core::make_error(core::ErrorCode::InvalidArgument,
                                                "render graph: pass '{}' uses transient buffer '{}' in state {}, "
                                                "which only imported buffers can be in",
                                                pass_name, resource_name, rhi::to_string(access.state));
                    }
                    buffer.usage |= usage;
                }
            }
            add_barrier(access.kind, access.resource, tracking, access.state, access.writes);
        }
        pass.barrier_count = static_cast<uint32_t>(barriers_.size()) - pass.first_barrier;
    }

    // Imported resources end in their `after` state (also those no executing pass touched).
    first_final_barrier_ = static_cast<uint32_t>(barriers_.size());
    for (std::size_t i = 0; i < textures_.size(); ++i) {
        if (textures_[i].imported) {
            add_barrier(ResourceKind::Texture, static_cast<uint32_t>(i), texture_tracking_[i],
                        textures_[i].states.after, false);
        }
    }
    for (std::size_t i = 0; i < buffers_.size(); ++i) {
        if (buffers_[i].imported) {
            add_barrier(ResourceKind::Buffer, static_cast<uint32_t>(i), buffer_tracking_[i], buffers_[i].states.after,
                        false);
        }
    }
    compiled_ = true;
    return {};
}

core::Result<rhi::CommandList> RenderGraph::record() {
    BYTEE_ASSERT(!recorded_, "RenderGraph::record: already recorded this frame; reset() first");
    if (!compiled_) {
        BYTEE_TRY(compile());
    }
    BYTEE_TRY(acquire_transients());
    core::Result<rhi::CommandList> commands = device_->begin_commands();
    if (!commands) {
        return std::move(commands).error();
    }
    for (const PassEntry& pass : passes_) {
        if (!pass.culled) {
            record_barriers(*commands, std::span{barriers_}.subspan(pass.first_barrier, pass.barrier_count));
            record_pass(*commands, pass);
        }
    }
    record_barriers(*commands, final_barriers());
    recorded_ = true;
    return commands;
}

core::Result<rhi::SyncPoint> RenderGraph::execute() {
    core::Result<rhi::CommandList> commands = record();
    if (!commands) {
        return std::move(commands).error();
    }
    return device_->submit(std::move(*commands));
}

void RenderGraph::add_barrier(ResourceKind kind, uint32_t resource, Tracking& tracking, rhi::ResourceState state,
                              bool writes) {
    // Same state with reads on both sides: nothing to wait for. Otherwise the state changes, or a write must be made
    // visible to the next access (or protected from a later write).
    if (tracking.state != state || tracking.written || writes) {
        barriers_.push_back({.kind = kind, .resource = resource, .before = tracking.state, .after = state});
    }
    tracking.state = state;
    tracking.written = writes;
}

core::Result<void> RenderGraph::acquire_transients() {
    for (TextureEntry& texture : textures_) {
        if (texture.imported || !texture.used) {
            continue;
        }
        auto pooled = std::ranges::find_if(pooled_textures_, [&](const PooledTexture& candidate) {
            return !candidate.in_use && candidate.desc == texture.desc && candidate.usage == texture.usage;
        });
        if (pooled == pooled_textures_.end()) {
            core::Result<rhi::Texture> created = device_->create_texture({.dimension = texture.desc.dimension,
                                                                          .extent = texture.desc.extent,
                                                                          .mip_levels = texture.desc.mip_levels,
                                                                          .array_layers = texture.desc.array_layers,
                                                                          .format = texture.desc.format,
                                                                          .usage = texture.usage,
                                                                          .debug_name = name_of(texture.name)});
            if (!created) {
                return std::move(created).error();
            }
            pooled_textures_.push_back({.desc = texture.desc, .usage = texture.usage, .texture = std::move(*created)});
            pooled = std::prev(pooled_textures_.end());
        }
        pooled->in_use = true;
        pooled->last_used_frame = frame_;
        texture.handle = pooled->texture.handle();
    }
    for (BufferEntry& buffer : buffers_) {
        if (buffer.imported || !buffer.used) {
            continue;
        }
        auto pooled = std::ranges::find_if(pooled_buffers_, [&](const PooledBuffer& candidate) {
            return !candidate.in_use && candidate.desc == buffer.desc && candidate.usage == buffer.usage;
        });
        if (pooled == pooled_buffers_.end()) {
            core::Result<rhi::Buffer> created = device_->create_buffer(
                {.size = buffer.desc.size, .usage = buffer.usage, .debug_name = name_of(buffer.name)});
            if (!created) {
                return std::move(created).error();
            }
            pooled_buffers_.push_back({.desc = buffer.desc, .usage = buffer.usage, .buffer = std::move(*created)});
            pooled = std::prev(pooled_buffers_.end());
        }
        pooled->in_use = true;
        pooled->last_used_frame = frame_;
        buffer.handle = pooled->buffer.handle();
    }
    return {};
}

void RenderGraph::record_barriers(rhi::CommandList& commands, std::span<const GraphBarrier> barriers) {
    if (barriers.empty()) {
        return;
    }
    texture_barriers_.clear();
    buffer_barriers_.clear();
    for (const GraphBarrier& barrier : barriers) {
        if (barrier.kind == ResourceKind::Texture) {
            texture_barriers_.push_back(
                {.texture = textures_[barrier.resource].handle, .before = barrier.before, .after = barrier.after});
        } else {
            buffer_barriers_.push_back(
                {.buffer = buffers_[barrier.resource].handle, .before = barrier.before, .after = barrier.after});
        }
    }
    commands.barrier(buffer_barriers_, texture_barriers_);
}

void RenderGraph::record_pass(rhi::CommandList& commands, const PassEntry& pass) {
    const bool renders = pass.color_count > 0 || pass.depth.has_value();
    if (renders) {
        std::array<rhi::ColorAttachment, rhi::k_max_color_attachments> colors{};
        for (uint32_t i = 0; i < pass.color_count; ++i) {
            const ColorAttachment& color = colors_[pass.first_color + i];
            colors[i] = {.texture = textures_[color.texture.index()].handle,
                         .load = color.load,
                         .store = color.store,
                         .clear = color.clear};
        }
        std::optional<rhi::DepthAttachment> depth;
        if (pass.depth) {
            depth = rhi::DepthAttachment{.texture = textures_[pass.depth->texture.index()].handle,
                                         .load = pass.depth->load,
                                         .store = pass.depth->store,
                                         .clear_depth = pass.depth->clear_depth,
                                         .clear_stencil = pass.depth->clear_stencil};
        }
        commands.begin_render_pass({.colors = std::span{colors}.first(pass.color_count), .depth = depth});
    }
    PassContext context{*this, commands};
    pass.execute(pass.pass, context);
    if (renders) {
        commands.end_render_pass();
    }
}

PassBuilder RenderGraph::begin_pass(std::string_view name, void* pass, ExecuteFunction execute) {
    passes_.push_back({.name = store_name(name),
                       .pass = pass,
                       .execute = execute,
                       .first_access = static_cast<uint32_t>(accesses_.size()),
                       .first_color = static_cast<uint32_t>(colors_.size())});
    return PassBuilder{*this, static_cast<uint32_t>(passes_.size() - 1)};
}

RenderGraph::Name RenderGraph::store_name(std::string_view name) {
    const Name stored{.offset = static_cast<uint32_t>(names_.size()), .size = static_cast<uint32_t>(name.size())};
    names_.append(name);
    return stored;
}

std::string_view RenderGraph::name_of(Name name) const noexcept {
    return std::string_view{names_}.substr(name.offset, name.size);
}

TextureRef PassBuilder::create_texture(std::string_view name, const TextureDesc& desc) {
    BYTEE_ASSERT(desc.extent.width > 0 && desc.extent.height > 0 && desc.extent.depth > 0,
                 "transient texture '{}': empty extent", name);
    BYTEE_ASSERT(desc.format != rhi::Format::Undefined, "transient texture '{}': format is undefined", name);
    graph_.textures_.push_back({.name = graph_.store_name(name), .desc = desc});
    graph_.compiled_ = false;
    return TextureRef{static_cast<uint32_t>(graph_.textures_.size() - 1)};
}

BufferRef PassBuilder::create_buffer(std::string_view name, const BufferDesc& desc) {
    BYTEE_ASSERT(desc.size > 0, "transient buffer '{}': size is 0", name);
    graph_.buffers_.push_back({.name = graph_.store_name(name), .desc = desc});
    graph_.compiled_ = false;
    return BufferRef{static_cast<uint32_t>(graph_.buffers_.size() - 1)};
}

void PassBuilder::read(TextureRef texture, rhi::ResourceState state) {
    access(ResourceKind::Texture, texture.index(), state, false);
}

void PassBuilder::write(TextureRef texture, rhi::ResourceState state) {
    access(ResourceKind::Texture, texture.index(), state, true);
}

void PassBuilder::read(BufferRef buffer, rhi::ResourceState state) {
    access(ResourceKind::Buffer, buffer.index(), state, false);
}

void PassBuilder::write(BufferRef buffer, rhi::ResourceState state) {
    access(ResourceKind::Buffer, buffer.index(), state, true);
}

void PassBuilder::color_attachment(const ColorAttachment& attachment) {
    RenderGraph::PassEntry& pass = graph_.passes_[pass_index_];
    BYTEE_ASSERT(pass.color_count < rhi::k_max_color_attachments, "pass '{}': more than {} color attachments",
                 graph_.name_of(pass.name), rhi::k_max_color_attachments);
    BYTEE_ASSERT(!rhi::is_depth_format(desc(attachment.texture).format),
                 "pass '{}': depth texture used as a color attachment", graph_.name_of(pass.name));
    access(ResourceKind::Texture, attachment.texture.index(), rhi::ResourceState::ColorAttachment, true);
    graph_.colors_.push_back(attachment);
    ++pass.color_count;
}

void PassBuilder::depth_attachment(const DepthAttachment& attachment) {
    RenderGraph::PassEntry& pass = graph_.passes_[pass_index_];
    BYTEE_ASSERT(!pass.depth.has_value(), "pass '{}': more than one depth attachment", graph_.name_of(pass.name));
    BYTEE_ASSERT(rhi::is_depth_format(desc(attachment.texture).format),
                 "pass '{}': depth attachment needs a depth format", graph_.name_of(pass.name));
    access(ResourceKind::Texture, attachment.texture.index(), rhi::ResourceState::DepthWrite, true);
    pass.depth = attachment;
}

const TextureDesc& PassBuilder::desc(TextureRef texture) const {
    BYTEE_ASSERT(texture.index() < graph_.textures_.size(), "invalid TextureRef {}", texture.index());
    return graph_.textures_[texture.index()].desc;
}

const BufferDesc& PassBuilder::desc(BufferRef buffer) const {
    BYTEE_ASSERT(buffer.index() < graph_.buffers_.size(), "invalid BufferRef {}", buffer.index());
    return graph_.buffers_[buffer.index()].desc;
}

void PassBuilder::access(ResourceKind kind, uint32_t resource, rhi::ResourceState state, bool writes) {
    RenderGraph::PassEntry& pass = graph_.passes_[pass_index_];
    const std::string_view pass_name = graph_.name_of(pass.name);
    if (kind == ResourceKind::Texture) {
        BYTEE_ASSERT(resource < graph_.textures_.size(), "pass '{}': invalid TextureRef {}", pass_name, resource);
        BYTEE_ASSERT(rhi::is_texture_state(state), "pass '{}': {} is not a texture state", pass_name,
                     rhi::to_string(state));
    } else {
        BYTEE_ASSERT(resource < graph_.buffers_.size(), "pass '{}': invalid BufferRef {}", pass_name, resource);
        BYTEE_ASSERT(rhi::is_buffer_state(state), "pass '{}': {} is not a buffer state", pass_name,
                     rhi::to_string(state));
    }
    // One state per resource per pass: the barrier before the pass moves each resource into exactly one state.
    const std::span<const ResourceAccess> previous =
        std::span{graph_.accesses_}.subspan(pass.first_access, pass.access_count);
    BYTEE_ASSERT(
        std::ranges::none_of(
            previous, [&](const ResourceAccess& other) { return other.kind == kind && other.resource == resource; }),
        "pass '{}': resource {} is used more than once", pass_name, resource);
    graph_.accesses_.push_back({.kind = kind, .resource = resource, .state = state, .writes = writes});
    ++pass.access_count;
}

rhi::TextureHandle PassContext::texture(TextureRef texture) const {
    BYTEE_ASSERT(texture.index() < graph_.textures_.size(), "invalid TextureRef {}", texture.index());
    const RenderGraph::TextureEntry& entry = graph_.textures_[texture.index()];
    BYTEE_ASSERT(entry.used, "texture '{}' is not declared by any executing pass", graph_.name_of(entry.name));
    return entry.handle;
}

rhi::BufferHandle PassContext::buffer(BufferRef buffer) const {
    BYTEE_ASSERT(buffer.index() < graph_.buffers_.size(), "invalid BufferRef {}", buffer.index());
    const RenderGraph::BufferEntry& entry = graph_.buffers_[buffer.index()];
    BYTEE_ASSERT(entry.used, "buffer '{}' is not declared by any executing pass", graph_.name_of(entry.name));
    return entry.handle;
}

const TextureDesc& PassContext::desc(TextureRef texture) const {
    BYTEE_ASSERT(texture.index() < graph_.textures_.size(), "invalid TextureRef {}", texture.index());
    return graph_.textures_[texture.index()].desc;
}

const BufferDesc& PassContext::desc(BufferRef buffer) const {
    BYTEE_ASSERT(buffer.index() < graph_.buffers_.size(), "invalid BufferRef {}", buffer.index());
    return graph_.buffers_[buffer.index()].desc;
}

} // namespace bytee::render
