#include "graph_state.hpp"

#include <bytee/core/assert.hpp>
#include <bytee/render/graph/render_graph.hpp>

#include <array>
#include <optional>

namespace bytee::render {

// ---------------------------------------------------------------------------------------------------------------------
// GraphState: compile and record

core::Result<void> detail::GraphState::compile() {
    BYTEE_ASSERT(!recorded, "RenderGraph::compile: the graph was already recorded; reset() first");
    compiled = false;
    BYTEE_TRY(compiler.compile(passes, resources, *device, schedule));
    compiled = true;
    return {};
}

core::Result<rhi::CommandList> detail::GraphState::record() {
    BYTEE_ASSERT(!recorded, "RenderGraph::record: already recorded this frame; reset() first");
    if (!compiled) {
        BYTEE_TRY(compile());
    }
    BYTEE_TRY(bind_resources());
    core::Result<rhi::CommandList> commands = device->begin_commands();
    if (!commands) {
        return std::move(commands).error();
    }
    for (uint32_t index = 0; index < passes.count(); ++index) {
        if (schedule.passes[index].culled) {
            continue;
        }
        commands->begin_debug_label(passes.name(passes.pass(index)));
        barrier_batch.record(*commands, schedule.barriers_before(index), bindings);
        record_pass(*commands, index);
        commands->end_debug_label();
    }
    barrier_batch.record(*commands, schedule.final_barriers(), bindings);
    recorded = true;
    return commands;
}

// Imported resources bind their own handles; used transients get one from the pool.
core::Result<void> detail::GraphState::bind_resources() {
    bindings.textures.assign(resources.texture_count(), rhi::TextureHandle{});
    for (uint32_t i = 0; i < resources.texture_count(); ++i) {
        const GraphResources::Texture& texture = resources.texture(i);
        if (texture.imported) {
            bindings.textures[i] = *texture.imported;
        } else if (schedule.textures[i].used) {
            core::Result<rhi::TextureHandle> acquired =
                transients.acquire(*device, resources.name(texture), texture.desc, schedule.textures[i].usage);
            if (!acquired) {
                return std::move(acquired).error();
            }
            bindings.textures[i] = *acquired;
        }
    }
    bindings.buffers.assign(resources.buffer_count(), rhi::BufferHandle{});
    for (uint32_t i = 0; i < resources.buffer_count(); ++i) {
        const GraphResources::Buffer& buffer = resources.buffer(i);
        if (buffer.imported) {
            bindings.buffers[i] = *buffer.imported;
        } else if (schedule.buffers[i].used) {
            core::Result<rhi::BufferHandle> acquired =
                transients.acquire(*device, resources.name(buffer), buffer.desc, schedule.buffers[i].usage);
            if (!acquired) {
                return std::move(acquired).error();
            }
            bindings.buffers[i] = *acquired;
        }
    }
    return {};
}

// A pass with attachments runs inside a render pass the graph begins and ends.
void detail::GraphState::record_pass(rhi::CommandList& commands, uint32_t index) {
    const GraphPasses::Pass& pass = passes.pass(index);
    if (pass.renders()) {
        std::array<rhi::ColorAttachment, rhi::k_max_color_attachments> colors{};
        const std::span<const ColorAttachment> declared = passes.colors(pass);
        for (std::size_t i = 0; i < declared.size(); ++i) {
            colors[i] = {.texture = bindings.textures[declared[i].texture.index()],
                         .load = declared[i].load,
                         .store = declared[i].store,
                         .clear = declared[i].clear};
        }
        std::optional<rhi::DepthAttachment> depth;
        if (pass.depth) {
            depth = rhi::DepthAttachment{.texture = bindings.textures[pass.depth->texture.index()],
                                         .load = pass.depth->load,
                                         .store = pass.depth->store,
                                         .clear_depth = pass.depth->clear_depth,
                                         .clear_stencil = pass.depth->clear_stencil};
        }
        commands.begin_render_pass({.colors = std::span{colors}.first(declared.size()), .depth = depth});
    }
    PassContext context{*this, commands};
    pass.execute(pass.object, context);
    if (pass.renders()) {
        commands.end_render_pass();
    }
}

void detail::BarrierBatch::record(rhi::CommandList& commands, std::span<const GraphBarrier> barriers,
                                  const GraphBindings& bindings) {
    if (barriers.empty()) {
        return;
    }
    textures_.clear();
    buffers_.clear();
    for (const GraphBarrier& barrier : barriers) {
        if (barrier.kind == ResourceKind::Texture) {
            textures_.push_back(
                {.texture = bindings.textures[barrier.resource], .before = barrier.before, .after = barrier.after});
        } else {
            buffers_.push_back(
                {.buffer = bindings.buffers[barrier.resource], .before = barrier.before, .after = barrier.after});
        }
    }
    commands.barrier(buffers_, textures_);
}

// ---------------------------------------------------------------------------------------------------------------------
// RenderGraph

RenderGraph::RenderGraph(rhi::Device& device)
    : state_(std::make_unique<detail::GraphState>()) {
    BYTEE_ASSERT(device.is_valid(), "RenderGraph needs a valid Device");
    state_->device = &device;
}

RenderGraph::~RenderGraph() = default;
RenderGraph::RenderGraph(RenderGraph&& other) noexcept = default;
RenderGraph& RenderGraph::operator=(RenderGraph&& other) noexcept = default;

void RenderGraph::reset() noexcept {
    state_->transients.begin_frame();
    state_->resources.clear();
    state_->passes.clear();
    state_->schedule.clear();
    state_->compiled = false;
    state_->recorded = false;
}

TextureRef RenderGraph::import_texture(std::string_view name, rhi::TextureHandle texture, ImportedStates states) {
    BYTEE_ASSERT(rhi::is_texture_state(states.before) && rhi::is_texture_state(states.after),
                 "imported texture '{}': {} -> {} are not texture states", name, rhi::to_string(states.before),
                 rhi::to_string(states.after));
    const rhi::TextureInfo& info = state_->device->info(texture);
    state_->compiled = false;
    return state_->resources.add_texture(name,
                                         {.dimension = info.dimension,
                                          .extent = info.extent,
                                          .mip_levels = info.mip_levels,
                                          .array_layers = info.array_layers,
                                          .format = info.format},
                                         texture, states);
}

BufferRef RenderGraph::import_buffer(std::string_view name, rhi::BufferHandle buffer, ImportedStates states) {
    BYTEE_ASSERT(rhi::is_buffer_state(states.before) && rhi::is_buffer_state(states.after),
                 "imported buffer '{}': {} -> {} are not buffer states", name, rhi::to_string(states.before),
                 rhi::to_string(states.after));
    state_->compiled = false;
    return state_->resources.add_buffer(name, {.size = state_->device->info(buffer).size}, buffer, states);
}

core::Result<void> RenderGraph::compile() {
    return state_->compile();
}

core::Result<rhi::CommandList> RenderGraph::record() {
    return state_->record();
}

core::Result<rhi::SyncPoint> RenderGraph::execute() {
    core::Result<rhi::CommandList> commands = state_->record();
    if (!commands) {
        return std::move(commands).error();
    }
    return state_->device->submit(std::move(*commands));
}

uint32_t RenderGraph::pass_count() const noexcept {
    return state_->passes.count();
}

uint32_t RenderGraph::texture_count() const noexcept {
    return state_->resources.texture_count();
}

uint32_t RenderGraph::buffer_count() const noexcept {
    return state_->resources.buffer_count();
}

PassView RenderGraph::pass(uint32_t index) const {
    const detail::GraphPasses& passes = state_->passes;
    const detail::GraphPasses::Pass& pass = passes.pass(index);
    PassView view{.name = passes.name(pass),
                  .accesses = passes.accesses(pass),
                  .colors = passes.colors(pass),
                  .depth = pass.depth};
    if (state_->compiled) {
        view.culled = state_->schedule.passes[index].culled;
        view.barriers = state_->schedule.barriers_before(index);
    }
    return view;
}

TextureView RenderGraph::texture(TextureRef texture) const {
    const detail::GraphResources::Texture& entry = state_->resources.texture(texture.index());
    TextureView view{
        .name = state_->resources.name(entry), .desc = entry.desc, .imported = entry.imported, .states = entry.states};
    if (state_->compiled) {
        view.usage = state_->schedule.textures[texture.index()].usage;
        view.used = state_->schedule.textures[texture.index()].used;
    }
    return view;
}

BufferView RenderGraph::buffer(BufferRef buffer) const {
    const detail::GraphResources::Buffer& entry = state_->resources.buffer(buffer.index());
    BufferView view{
        .name = state_->resources.name(entry), .desc = entry.desc, .imported = entry.imported, .states = entry.states};
    if (state_->compiled) {
        view.usage = state_->schedule.buffers[buffer.index()].usage;
        view.used = state_->schedule.buffers[buffer.index()].used;
    }
    return view;
}

std::span<const GraphBarrier> RenderGraph::final_barriers() const noexcept {
    return state_->compiled ? state_->schedule.final_barriers() : std::span<const GraphBarrier>{};
}

uint32_t RenderGraph::pooled_texture_count() const noexcept {
    return state_->transients.texture_count();
}

uint32_t RenderGraph::pooled_buffer_count() const noexcept {
    return state_->transients.buffer_count();
}

PassBuilder RenderGraph::begin_pass(std::string_view name, void* pass, ExecuteFunction execute) {
    state_->compiled = false;
    return PassBuilder{*state_, state_->passes.add(name, pass, execute)};
}

// ---------------------------------------------------------------------------------------------------------------------
// PassBuilder

TextureRef PassBuilder::create_texture(std::string_view name, const TextureDesc& desc) {
    BYTEE_ASSERT(desc.extent.width > 0 && desc.extent.height > 0 && desc.extent.depth > 0,
                 "transient texture '{}': empty extent", name);
    BYTEE_ASSERT(desc.format != rhi::Format::Undefined, "transient texture '{}': format is undefined", name);
    return state_.resources.add_texture(name, desc);
}

BufferRef PassBuilder::create_buffer(std::string_view name, const BufferDesc& desc) {
    BYTEE_ASSERT(desc.size > 0, "transient buffer '{}': size is 0", name);
    return state_.resources.add_buffer(name, desc);
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
    BYTEE_ASSERT(!rhi::is_depth_format(desc(attachment.texture).format),
                 "pass '{}': depth texture used as a color attachment",
                 state_.passes.name(state_.passes.pass(pass_index_)));
    access(ResourceKind::Texture, attachment.texture.index(), rhi::ResourceState::ColorAttachment, true);
    state_.passes.add_color(pass_index_, attachment);
}

void PassBuilder::depth_attachment(const DepthAttachment& attachment) {
    BYTEE_ASSERT(rhi::is_depth_format(desc(attachment.texture).format),
                 "pass '{}': depth attachment needs a depth format",
                 state_.passes.name(state_.passes.pass(pass_index_)));
    access(ResourceKind::Texture, attachment.texture.index(), rhi::ResourceState::DepthWrite, true);
    state_.passes.set_depth(pass_index_, attachment);
}

const TextureDesc& PassBuilder::desc(TextureRef texture) const {
    return state_.resources.texture(texture.index()).desc;
}

const BufferDesc& PassBuilder::desc(BufferRef buffer) const {
    return state_.resources.buffer(buffer.index()).desc;
}

void PassBuilder::access(ResourceKind kind, uint32_t resource, rhi::ResourceState state, bool writes) {
    [[maybe_unused]] const std::string_view pass_name = state_.passes.name(state_.passes.pass(pass_index_));
    BYTEE_ASSERT(resource < state_.resources.count(kind), "pass '{}': invalid {} reference {}", pass_name,
                 kind == ResourceKind::Texture ? "texture" : "buffer", resource);
    BYTEE_ASSERT(kind == ResourceKind::Texture ? rhi::is_texture_state(state) : rhi::is_buffer_state(state),
                 "pass '{}': {} is not a {} state", pass_name, rhi::to_string(state),
                 kind == ResourceKind::Texture ? "texture" : "buffer");
    state_.passes.add_access(pass_index_, {.kind = kind, .resource = resource, .state = state, .writes = writes});
}

// ---------------------------------------------------------------------------------------------------------------------
// PassContext

rhi::Device& PassContext::device() const noexcept {
    return *state_.device;
}

rhi::TextureHandle PassContext::texture(TextureRef texture) const {
    BYTEE_ASSERT(state_.schedule.is_used(ResourceKind::Texture, texture.index()),
                 "texture '{}' is not declared by any executing pass",
                 state_.resources.name(ResourceKind::Texture, texture.index()));
    return state_.bindings.textures[texture.index()];
}

rhi::BufferHandle PassContext::buffer(BufferRef buffer) const {
    BYTEE_ASSERT(state_.schedule.is_used(ResourceKind::Buffer, buffer.index()),
                 "buffer '{}' is not declared by any executing pass",
                 state_.resources.name(ResourceKind::Buffer, buffer.index()));
    return state_.bindings.buffers[buffer.index()];
}

const TextureDesc& PassContext::desc(TextureRef texture) const {
    return state_.resources.texture(texture.index()).desc;
}

const BufferDesc& PassContext::desc(BufferRef buffer) const {
    return state_.resources.buffer(buffer.index()).desc;
}

} // namespace bytee::render
