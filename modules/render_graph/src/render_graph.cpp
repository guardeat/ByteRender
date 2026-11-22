#include <bytee/core/assert.hpp>
#include <bytee/render/graph/render_graph.hpp>

#include <algorithm>

namespace bytee::render {

RenderGraph::RenderGraph(rhi::Device& device)
    : device_(&device) {
    BYTEE_ASSERT(device.is_valid(), "RenderGraph needs a valid Device");
}

void RenderGraph::reset() noexcept {
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
                         .states = states});
    return TextureRef{static_cast<uint32_t>(textures_.size() - 1)};
}

BufferRef RenderGraph::import_buffer(std::string_view name, rhi::BufferHandle buffer, ImportedStates states) {
    BYTEE_ASSERT(rhi::is_buffer_state(states.before) && rhi::is_buffer_state(states.after),
                 "imported buffer '{}': {} -> {} are not buffer states", name, rhi::to_string(states.before),
                 rhi::to_string(states.after));
    buffers_.push_back(
        {.name = store_name(name), .desc = {.size = device_->info(buffer).size}, .imported = buffer, .states = states});
    return BufferRef{static_cast<uint32_t>(buffers_.size() - 1)};
}

PassView RenderGraph::pass(uint32_t index) const {
    BYTEE_ASSERT(index < passes_.size(), "pass index {} out of range [0, {})", index, passes_.size());
    const PassEntry& entry = passes_[index];
    return {.name = name_of(entry.name),
            .accesses = std::span{accesses_}.subspan(entry.first_access, entry.access_count),
            .colors = std::span{colors_}.subspan(entry.first_color, entry.color_count),
            .depth = entry.depth};
}

TextureView RenderGraph::texture(TextureRef texture) const {
    BYTEE_ASSERT(texture.index() < textures_.size(), "invalid TextureRef {}", texture.index());
    const TextureEntry& entry = textures_[texture.index()];
    return {.name = name_of(entry.name), .desc = entry.desc, .imported = entry.imported, .states = entry.states};
}

BufferView RenderGraph::buffer(BufferRef buffer) const {
    BYTEE_ASSERT(buffer.index() < buffers_.size(), "invalid BufferRef {}", buffer.index());
    const BufferEntry& entry = buffers_[buffer.index()];
    return {.name = name_of(entry.name), .desc = entry.desc, .imported = entry.imported, .states = entry.states};
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
    return TextureRef{static_cast<uint32_t>(graph_.textures_.size() - 1)};
}

BufferRef PassBuilder::create_buffer(std::string_view name, const BufferDesc& desc) {
    BYTEE_ASSERT(desc.size > 0, "transient buffer '{}': size is 0", name);
    graph_.buffers_.push_back({.name = graph_.store_name(name), .desc = desc});
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

} // namespace bytee::render
