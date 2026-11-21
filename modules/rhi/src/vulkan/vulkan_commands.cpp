// Command lists: recycled command contexts, barriers, copies, submission and upload helpers.

#include "vulkan/vulkan_convert.hpp"
#include "vulkan/vulkan_device.hpp"

#include <bytee/core/assert.hpp>

#include <algorithm>
#include <array>
#include <format>

namespace bytee::render::rhi {

namespace {

// Barriers are recorded in fixed-size batches from the stack: no allocation however many are passed.
constexpr std::size_t k_barrier_batch = 32;

uint32_t mip_dimension(uint32_t base, uint32_t mip_level) noexcept {
    const uint32_t value = base >> mip_level;
    return value > 0 ? value : 1;
}

// A region with its extent resolved (zero extent = rest of the mip level from `offset`).
core::Extent3D resolve_extent(const TextureInfo& info, const TextureRegion& region) noexcept {
    if (region.extent.width != 0 && region.extent.height != 0 && region.extent.depth != 0) {
        return region.extent;
    }
    return core::Extent3D{
        .width = mip_dimension(info.extent.width, region.mip_level) - static_cast<uint32_t>(region.offset.x),
        .height = mip_dimension(info.extent.height, region.mip_level) - static_cast<uint32_t>(region.offset.y),
        .depth = mip_dimension(info.extent.depth, region.mip_level) - static_cast<uint32_t>(region.offset.z),
    };
}

bool region_fits(const TextureInfo& info, const TextureRegion& region, const core::Extent3D& extent) noexcept {
    if (region.mip_level >= info.mip_levels || region.array_layer_count == 0 ||
        region.base_array_layer + region.array_layer_count > info.array_layers || region.offset.x < 0 ||
        region.offset.y < 0 || region.offset.z < 0) {
        return false;
    }
    return static_cast<uint64_t>(region.offset.x) + extent.width <=
               mip_dimension(info.extent.width, region.mip_level) &&
           static_cast<uint64_t>(region.offset.y) + extent.height <=
               mip_dimension(info.extent.height, region.mip_level) &&
           static_cast<uint64_t>(region.offset.z) + extent.depth <= mip_dimension(info.extent.depth, region.mip_level);
}

uint64_t region_bytes(const TextureInfo& info, const TextureRegion& region, const core::Extent3D& extent) noexcept {
    return uint64_t{extent.width} * extent.height * extent.depth * region.array_layer_count *
           bytes_per_texel(info.format);
}

VkBufferImageCopy2 to_vk_copy(const TextureInfo& info, const TextureRegion& region, const core::Extent3D& extent,
                              uint64_t buffer_offset) noexcept {
    return VkBufferImageCopy2{
        .sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
        .bufferOffset = buffer_offset,
        .bufferRowLength = 0, // tightly packed
        .bufferImageHeight = 0,
        .imageSubresource = {.aspectMask = vulkan::aspect_mask(info.format),
                             .mipLevel = region.mip_level,
                             .baseArrayLayer = region.base_array_layer,
                             .layerCount = region.array_layer_count},
        .imageOffset = {.x = region.offset.x, .y = region.offset.y, .z = region.offset.z},
        .imageExtent = {.width = extent.width, .height = extent.height, .depth = extent.depth},
    };
}

// Conservative global dependency for upload helpers (not on hot paths): everything before -> the copy -> everything
// after, so callers need no barriers for buffer uploads.
void record_upload_memory_barrier(const detail::DeviceImpl& impl, VkCommandBuffer buffer, bool before_copy) noexcept {
    const VkMemoryBarrier2 barrier{
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = before_copy ? VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT : VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
        .srcAccessMask =
            before_copy ? VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT : VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .dstStageMask = before_copy ? VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT : VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        .dstAccessMask =
            before_copy ? VK_ACCESS_2_TRANSFER_WRITE_BIT : VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
    };
    const VkDependencyInfo dependency{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .memoryBarrierCount = 1, .pMemoryBarriers = &barrier};
    impl.vk.vkCmdPipelineBarrier2(buffer, &dependency);
}

} // namespace

// ----------------------------------------------------------------------------------------------------------------
// Command contexts

detail::CommandContext* get_context(const CommandList& list) noexcept {
    return list.context_;
}

core::Result<detail::CommandContext*> detail::DeviceImpl::acquire_context(QueueType queue_type) {
    detail::CommandContext* context = nullptr;
    {
        const std::scoped_lock lock{contexts_mutex};
        // Pending contexts are in submission order: recycle every one whose work has completed.
        while (!pending_contexts.empty() &&
               pending_contexts.front()->last_submission <= queue(pending_contexts.front()->queue).cached_completed()) {
            free_contexts.push_back(pending_contexts.front());
            pending_contexts.pop_front();
        }
        const auto match = std::ranges::find_if(free_contexts, [queue_type](const detail::CommandContext* candidate) {
            return candidate->queue == queue_type;
        });
        if (match != free_contexts.end()) {
            context = *match;
            free_contexts.erase(match);
        }
    }

    if (context != nullptr) {
        // Exclusively ours now; resetting the pool recycles the command buffer's memory.
        if (const VkResult result = vk.vkResetCommandPool(device, context->pool, 0); result != VK_SUCCESS) {
            discard_context(context);
            return fail(result, "vkResetCommandPool");
        }
    } else {
        auto created = std::make_unique<detail::CommandContext>();
        created->queue = queue_type;
        const VkCommandPoolCreateInfo pool_info{
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
            .queueFamilyIndex = queue(queue_type).family(),
        };
        if (const VkResult result = vk.vkCreateCommandPool(device, &pool_info, nullptr, &created->pool);
            result != VK_SUCCESS) {
            return fail(result, "vkCreateCommandPool");
        }
        const VkCommandBufferAllocateInfo allocate_info{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = created->pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        if (const VkResult result = vk.vkAllocateCommandBuffers(device, &allocate_info, &created->buffer);
            result != VK_SUCCESS) {
            vk.vkDestroyCommandPool(device, created->pool, nullptr);
            return fail(result, "vkAllocateCommandBuffers");
        }
        context = created.get();
        const std::scoped_lock lock{contexts_mutex};
        vulkan::set_debug_name(device, VK_OBJECT_TYPE_COMMAND_POOL, created->pool,
                               std::format("{} command pool {}", debug_name, contexts.size()));
        contexts.push_back(std::move(created));
    }

    const VkCommandBufferBeginInfo begin_info{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                              .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    if (const VkResult result = vk.vkBeginCommandBuffer(context->buffer, &begin_info); result != VK_SUCCESS) {
        discard_context(context);
        return fail(result, "vkBeginCommandBuffer");
    }
    return context;
}

void detail::DeviceImpl::discard_context(detail::CommandContext* context) noexcept {
    const std::scoped_lock lock{contexts_mutex};
    free_contexts.push_back(context); // reset when it is acquired again
}

void detail::DeviceImpl::retire_context(detail::CommandContext* context, uint64_t value) noexcept {
    context->last_submission = value;
    const std::scoped_lock lock{contexts_mutex};
    pending_contexts.push_back(context);
}

void detail::DeviceImpl::destroy_contexts() noexcept {
    const std::scoped_lock lock{contexts_mutex};
    for (const std::unique_ptr<detail::CommandContext>& context : contexts) {
        vk.vkDestroyCommandPool(device, context->pool, nullptr); // frees its command buffer too
    }
    contexts.clear();
    free_contexts.clear();
    pending_contexts.clear();
}

// ----------------------------------------------------------------------------------------------------------------
// CommandList

CommandList::~CommandList() {
    if (context_ != nullptr) {
        device_->discard_context(context_);
        device_->live_command_lists.fetch_sub(1, std::memory_order_acq_rel);
    }
}

CommandList& CommandList::operator=(CommandList&& other) noexcept {
    if (this != &other) {
        if (context_ != nullptr) {
            device_->discard_context(context_);
            device_->live_command_lists.fetch_sub(1, std::memory_order_acq_rel);
        }
        device_ = std::exchange(other.device_, nullptr);
        context_ = std::exchange(other.context_, nullptr);
    }
    return *this;
}

void CommandList::barrier(std::span<const BufferBarrier> buffers, std::span<const TextureBarrier> textures) {
    BYTEE_ASSERT(is_valid(), "CommandList is empty");
    const detail::DeviceImpl& impl = *device_;

    std::array<VkBufferMemoryBarrier2, k_barrier_batch> buffer_batch{};
    std::array<VkImageMemoryBarrier2, k_barrier_batch> image_batch{};
    std::size_t buffer_index = 0;
    std::size_t texture_index = 0;

    while (buffer_index < buffers.size() || texture_index < textures.size()) {
        uint32_t buffer_count = 0;
        for (; buffer_index < buffers.size() && buffer_count < k_barrier_batch; ++buffer_index) {
            const BufferBarrier& barrier = buffers[buffer_index];
            BYTEE_ASSERT(vulkan::is_buffer_state(barrier.before) && vulkan::is_buffer_state(barrier.after),
                         "buffer barrier {} -> {}: not a buffer state", to_string(barrier.before),
                         to_string(barrier.after));
            const vulkan::StateScope before = vulkan::to_state_scope(barrier.before);
            const vulkan::StateScope after = vulkan::to_state_scope(barrier.after);
            buffer_batch[buffer_count++] = VkBufferMemoryBarrier2{
                .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
                .srcStageMask = before.stages,
                .srcAccessMask = before.access,
                .dstStageMask = after.stages,
                .dstAccessMask = after.access,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .buffer = impl.buffers.get(barrier.buffer).buffer,
                .offset = 0,
                .size = VK_WHOLE_SIZE,
            };
        }
        uint32_t image_count = 0;
        for (; texture_index < textures.size() && image_count < k_barrier_batch; ++texture_index) {
            const TextureBarrier& barrier = textures[texture_index];
            BYTEE_ASSERT(vulkan::is_texture_state(barrier.after), "texture barrier to {}: not a texture state",
                         to_string(barrier.after));
            BYTEE_ASSERT(barrier.after != ResourceState::Undefined, "textures cannot transition to Undefined");
            const vulkan::TextureRecord& record = impl.textures.get(barrier.texture);
            const vulkan::StateScope before = vulkan::to_state_scope(barrier.before);
            const vulkan::StateScope after = vulkan::to_state_scope(barrier.after);
            const TextureSubresources& range = barrier.subresources;
            image_batch[image_count++] = VkImageMemoryBarrier2{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .srcStageMask = before.stages,
                .srcAccessMask = before.access,
                .dstStageMask = after.stages,
                .dstAccessMask = after.access,
                .oldLayout = before.layout,
                .newLayout = after.layout,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = record.image,
                .subresourceRange = {.aspectMask = has_stencil(record.info.format)
                                                       ? VkImageAspectFlags{VK_IMAGE_ASPECT_DEPTH_BIT |
                                                                            VK_IMAGE_ASPECT_STENCIL_BIT}
                                                       : vulkan::aspect_mask(record.info.format),
                                     .baseMipLevel = range.base_mip_level,
                                     .levelCount = range.mip_level_count == k_all_mip_levels ? VK_REMAINING_MIP_LEVELS
                                                                                             : range.mip_level_count,
                                     .baseArrayLayer = range.base_array_layer,
                                     .layerCount = range.array_layer_count == k_all_array_layers
                                                       ? VK_REMAINING_ARRAY_LAYERS
                                                       : range.array_layer_count},
            };
        }
        const VkDependencyInfo dependency{
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .bufferMemoryBarrierCount = buffer_count,
            .pBufferMemoryBarriers = buffer_batch.data(),
            .imageMemoryBarrierCount = image_count,
            .pImageMemoryBarriers = image_batch.data(),
        };
        impl.vk.vkCmdPipelineBarrier2(context_->buffer, &dependency);
    }
}

void CommandList::fill_buffer(BufferHandle dst, uint64_t offset, uint64_t size, uint32_t value) {
    BYTEE_ASSERT(is_valid(), "CommandList is empty");
    const vulkan::BufferRecord& record = device_->buffers.get(dst);
    BYTEE_ASSERT(offset % 4 == 0 && size % 4 == 0, "fill_buffer: offset and size must be multiples of 4");
    BYTEE_ASSERT(offset <= record.info.size && size <= record.info.size - offset,
                 "fill_buffer: {} bytes at {} exceed buffer '{}' ({} bytes)", size, offset, record.info.debug_name,
                 record.info.size);
    device_->vk.vkCmdFillBuffer(context_->buffer, record.buffer, offset, size, value);
}

void CommandList::copy_buffer(BufferHandle src, uint64_t src_offset, BufferHandle dst, uint64_t dst_offset,
                              uint64_t size) {
    BYTEE_ASSERT(is_valid(), "CommandList is empty");
    const vulkan::BufferRecord& source = device_->buffers.get(src);
    const vulkan::BufferRecord& destination = device_->buffers.get(dst);
    BYTEE_ASSERT(src_offset <= source.info.size && size <= source.info.size - src_offset,
                 "copy_buffer: source range exceeds buffer '{}'", source.info.debug_name);
    BYTEE_ASSERT(dst_offset <= destination.info.size && size <= destination.info.size - dst_offset,
                 "copy_buffer: destination range exceeds buffer '{}'", destination.info.debug_name);
    const VkBufferCopy2 region{
        .sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2, .srcOffset = src_offset, .dstOffset = dst_offset, .size = size};
    const VkCopyBufferInfo2 copy{.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
                                 .srcBuffer = source.buffer,
                                 .dstBuffer = destination.buffer,
                                 .regionCount = 1,
                                 .pRegions = &region};
    device_->vk.vkCmdCopyBuffer2(context_->buffer, &copy);
}

void CommandList::copy_buffer_to_texture(BufferHandle src, uint64_t src_offset, TextureHandle dst,
                                         const TextureRegion& region) {
    BYTEE_ASSERT(is_valid(), "CommandList is empty");
    const vulkan::BufferRecord& source = device_->buffers.get(src);
    const vulkan::TextureRecord& destination = device_->textures.get(dst);
    const core::Extent3D extent = resolve_extent(destination.info, region);
    BYTEE_ASSERT(region_fits(destination.info, region, extent), "copy_buffer_to_texture: region outside texture '{}'",
                 destination.info.debug_name);
    BYTEE_ASSERT(src_offset <= source.info.size &&
                     region_bytes(destination.info, region, extent) <= source.info.size - src_offset,
                 "copy_buffer_to_texture: buffer '{}' is too small", source.info.debug_name);
    const VkBufferImageCopy2 copy_region = to_vk_copy(destination.info, region, extent, src_offset);
    const VkCopyBufferToImageInfo2 copy{.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2,
                                        .srcBuffer = source.buffer,
                                        .dstImage = destination.image,
                                        .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                        .regionCount = 1,
                                        .pRegions = &copy_region};
    device_->vk.vkCmdCopyBufferToImage2(context_->buffer, &copy);
}

void CommandList::copy_texture_to_buffer(TextureHandle src, const TextureRegion& region, BufferHandle dst,
                                         uint64_t dst_offset) {
    BYTEE_ASSERT(is_valid(), "CommandList is empty");
    const vulkan::TextureRecord& source = device_->textures.get(src);
    const vulkan::BufferRecord& destination = device_->buffers.get(dst);
    const core::Extent3D extent = resolve_extent(source.info, region);
    BYTEE_ASSERT(region_fits(source.info, region, extent), "copy_texture_to_buffer: region outside texture '{}'",
                 source.info.debug_name);
    BYTEE_ASSERT(dst_offset <= destination.info.size &&
                     region_bytes(source.info, region, extent) <= destination.info.size - dst_offset,
                 "copy_texture_to_buffer: buffer '{}' is too small", destination.info.debug_name);
    const VkBufferImageCopy2 copy_region = to_vk_copy(source.info, region, extent, dst_offset);
    const VkCopyImageToBufferInfo2 copy{.sType = VK_STRUCTURE_TYPE_COPY_IMAGE_TO_BUFFER_INFO_2,
                                        .srcImage = source.image,
                                        .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                        .dstBuffer = destination.buffer,
                                        .regionCount = 1,
                                        .pRegions = &copy_region};
    device_->vk.vkCmdCopyImageToBuffer2(context_->buffer, &copy);
}

// ----------------------------------------------------------------------------------------------------------------
// Device: begin / submit / uploads

core::Result<CommandList> Device::begin_commands(QueueType queue) {
    BYTEE_ASSERT(is_valid(), "Device is empty");
    if (impl_->lost.load(std::memory_order_acquire)) {
        return impl_->lost_error();
    }
    core::Result<detail::CommandContext*> context = impl_->acquire_context(queue);
    if (!context) {
        return std::move(context).error();
    }
    impl_->live_command_lists.fetch_add(1, std::memory_order_acq_rel);
    return CommandList{impl_, *context};
}

core::Result<SyncPoint> Device::submit(CommandList&& commands) {
    BYTEE_ASSERT(is_valid(), "Device is empty");
    BYTEE_ASSERT(commands.is_valid(), "submitting an empty CommandList");
    BYTEE_ASSERT(commands.device_ == impl_, "CommandList belongs to another device");

    // The list is consumed whatever happens next.
    detail::CommandContext* context = std::exchange(commands.context_, nullptr);
    commands.device_ = nullptr;
    impl_->live_command_lists.fetch_sub(1, std::memory_order_acq_rel);

    if (const VkResult result = impl_->vk.vkEndCommandBuffer(context->buffer); result != VK_SUCCESS) {
        impl_->discard_context(context);
        return impl_->fail(result, "vkEndCommandBuffer");
    }
    const VkCommandBufferSubmitInfo command_buffer{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                                                   .commandBuffer = context->buffer};
    core::Result<SyncPoint> submitted = impl_->submit(context->queue, std::span{&command_buffer, 1});
    if (!submitted) {
        impl_->discard_context(context);
        return submitted;
    }
    impl_->retire_context(context, submitted->value);
    return submitted;
}

core::Result<SyncPoint> Device::upload(BufferHandle dst, uint64_t dst_offset, std::span<const std::byte> data) {
    BYTEE_ASSERT(is_valid(), "Device is empty");
    const BufferInfo& info = impl_->buffers.get(dst).info;
    if (data.empty() || dst_offset > info.size || data.size() > info.size - dst_offset) {
        return core::make_error(core::ErrorCode::InvalidArgument,
                                "upload of {} bytes at offset {} does not fit buffer '{}' ({} bytes)", data.size(),
                                dst_offset, info.debug_name, info.size);
    }
    BYTEE_ASSERT(info.usage.has(BufferUsage::TransferDst), "upload: buffer '{}' lacks TransferDst usage",
                 info.debug_name);

    // RAII staging buffer: retired when this function returns, freed once the copy below has completed.
    core::Result<Buffer> staging = create_buffer({.size = data.size(),
                                                  .usage = BufferUsage::TransferSrc,
                                                  .memory = MemoryLocation::Upload,
                                                  .debug_name = "upload staging"});
    if (!staging) {
        return std::move(staging).error();
    }
    BYTEE_TRY(write(*staging, 0, data));

    core::Result<CommandList> commands = begin_commands();
    if (!commands) {
        return std::move(commands).error();
    }
    record_upload_memory_barrier(*impl_, get_context(*commands)->buffer, true);
    commands->copy_buffer(*staging, 0, dst, dst_offset, data.size());
    record_upload_memory_barrier(*impl_, get_context(*commands)->buffer, false);
    return submit(std::move(*commands));
}

core::Result<SyncPoint> Device::upload(TextureHandle dst, std::span<const std::byte> data,
                                       const TextureUploadDesc& desc) {
    BYTEE_ASSERT(is_valid(), "Device is empty");
    const TextureInfo& info = impl_->textures.get(dst).info;
    const core::Extent3D extent = resolve_extent(info, desc.region);
    // Upload regions often come from asset data: validate at runtime (command-list copies only assert).
    if (!region_fits(info, desc.region, extent)) {
        return core::make_error(core::ErrorCode::InvalidArgument,
                                "upload: region (mip {}, layers {}+{}) is outside texture '{}'", desc.region.mip_level,
                                desc.region.base_array_layer, desc.region.array_layer_count, info.debug_name);
    }
    BYTEE_ASSERT(info.usage.has(TextureUsage::TransferDst), "upload: texture '{}' lacks TransferDst usage",
                 info.debug_name);
    if (const uint64_t expected = region_bytes(info, desc.region, extent); data.size() != expected) {
        return core::make_error(core::ErrorCode::InvalidArgument,
                                "upload to texture '{}': {} bytes given, the region needs {}", info.debug_name,
                                data.size(), expected);
    }

    core::Result<Buffer> staging = create_buffer({.size = data.size(),
                                                  .usage = BufferUsage::TransferSrc,
                                                  .memory = MemoryLocation::Upload,
                                                  .debug_name = "upload staging"});
    if (!staging) {
        return std::move(staging).error();
    }
    BYTEE_TRY(write(*staging, 0, data));

    core::Result<CommandList> commands = begin_commands();
    if (!commands) {
        return std::move(commands).error();
    }
    const TextureSubresources subresources{.base_mip_level = desc.region.mip_level,
                                           .mip_level_count = 1,
                                           .base_array_layer = desc.region.base_array_layer,
                                           .array_layer_count = desc.region.array_layer_count};
    const TextureBarrier to_copy{
        .texture = dst, .before = desc.before, .after = ResourceState::CopyDst, .subresources = subresources};
    commands->barrier({}, std::span{&to_copy, 1});
    commands->copy_buffer_to_texture(*staging, 0, dst, desc.region);
    const TextureBarrier to_final{
        .texture = dst, .before = ResourceState::CopyDst, .after = desc.after, .subresources = subresources};
    commands->barrier({}, std::span{&to_final, 1});
    return submit(std::move(*commands));
}

} // namespace bytee::render::rhi
