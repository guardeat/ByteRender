// Buffers and textures: creation, lookup, CPU access and deferred destruction.

#include "vulkan/vulkan_convert.hpp"
#include "vulkan/vulkan_device.hpp"

#include <bytee/core/assert.hpp>

#include <array>
#include <cstring>
#include <format>
#include <string>

namespace bytee::render::rhi {

namespace {

constexpr std::string_view k_unnamed = "(unnamed)";

std::string_view name_or_default(std::string_view name) noexcept {
    return name.empty() ? k_unnamed : name;
}

// Retires a pooled resource: destroyed now if the GPU is already past everything submitted, otherwise queued.
template <class Handle_, class List_, class Destroy_>
void retire_resource(detail::DeviceImpl& impl, List_& list, Handle_ handle, Destroy_&& destroy) {
    const vulkan::DeviceQueueValues retire_after = impl.last_submitted();
    if (detail::is_reached(retire_after, impl.cached_completed())) {
        destroy(handle);
        return;
    }
    list.push(retire_after, handle);
}

VmaAllocationCreateInfo allocation_info_for(MemoryLocation memory) noexcept {
    switch (memory) {
    case MemoryLocation::Device: return VmaAllocationCreateInfo{.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE};
    case MemoryLocation::Upload:
        return VmaAllocationCreateInfo{
            .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO,
        };
    case MemoryLocation::Readback:
        return VmaAllocationCreateInfo{
            .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO,
        };
    }
    return VmaAllocationCreateInfo{.usage = VMA_MEMORY_USAGE_AUTO};
}

core::Result<void> validate(const BufferDesc& desc) {
    if (desc.size == 0) {
        return core::make_error(core::ErrorCode::InvalidArgument, "buffer '{}': size must not be 0",
                                name_or_default(desc.debug_name));
    }
    if (desc.usage.none()) {
        return core::make_error(core::ErrorCode::InvalidArgument, "buffer '{}': usage must not be empty",
                                name_or_default(desc.debug_name));
    }
    return {};
}

core::Result<void> validate(const TextureDesc& desc, const VkPhysicalDeviceLimits& limits) {
    const std::string_view name = name_or_default(desc.debug_name);
    const core::Extent3D& extent = desc.extent;
    if (desc.format == Format::Undefined) {
        return core::make_error(core::ErrorCode::InvalidArgument, "texture '{}': format must not be Undefined", name);
    }
    if (desc.usage.none()) {
        return core::make_error(core::ErrorCode::InvalidArgument, "texture '{}': usage must not be empty", name);
    }
    if (extent.width == 0 || extent.height == 0 || extent.depth == 0 || desc.mip_levels == 0 ||
        desc.array_layers == 0) {
        return core::make_error(core::ErrorCode::InvalidArgument,
                                "texture '{}': extent ({}x{}x{}), mip levels and array layers must not be 0", name,
                                extent.width, extent.height, extent.depth);
    }
    if (desc.dimension == TextureDimension::Texture2D) {
        if (extent.depth != 1) {
            return core::make_error(core::ErrorCode::InvalidArgument, "texture '{}': 2D textures need depth 1", name);
        }
        if (extent.width > limits.maxImageDimension2D || extent.height > limits.maxImageDimension2D ||
            desc.array_layers > limits.maxImageArrayLayers) {
            return core::make_error(
                core::ErrorCode::InvalidArgument,
                "texture '{}': {}x{} with {} layers exceeds the device limits ({} pixels, {} layers)", name,
                extent.width, extent.height, desc.array_layers, limits.maxImageDimension2D, limits.maxImageArrayLayers);
        }
    } else {
        if (desc.array_layers != 1) {
            return core::make_error(core::ErrorCode::InvalidArgument, "texture '{}': 3D textures have one layer", name);
        }
        if (extent.width > limits.maxImageDimension3D || extent.height > limits.maxImageDimension3D ||
            extent.depth > limits.maxImageDimension3D) {
            return core::make_error(core::ErrorCode::InvalidArgument,
                                    "texture '{}': {}x{}x{} exceeds the device limit of {}", name, extent.width,
                                    extent.height, extent.depth, limits.maxImageDimension3D);
        }
        if (is_depth_format(desc.format)) {
            return core::make_error(core::ErrorCode::InvalidArgument, "texture '{}': 3D textures cannot use depth",
                                    name);
        }
    }
    if (desc.mip_levels > full_mip_count(extent)) {
        return core::make_error(core::ErrorCode::InvalidArgument, "texture '{}': {} mip levels, at most {} possible",
                                name, desc.mip_levels, full_mip_count(extent));
    }
    if (desc.usage.has(TextureUsage::DepthStencilAttachment) && !is_depth_format(desc.format)) {
        return core::make_error(core::ErrorCode::InvalidArgument,
                                "texture '{}': depth-stencil attachments need a depth format", name);
    }
    if (desc.usage.has(TextureUsage::ColorAttachment) && is_depth_format(desc.format)) {
        return core::make_error(core::ErrorCode::InvalidArgument,
                                "texture '{}': color attachments cannot use a depth format", name);
    }
    return {};
}

} // namespace

// ----------------------------------------------------------------------------------------------------------------
// Releaser (public deleter of Buffer / Texture)

void ResourceReleaser::operator()(BufferHandle buffer) const noexcept {
    if (device_ != nullptr) {
        device_->release(buffer);
    }
}

void ResourceReleaser::operator()(TextureHandle texture) const noexcept {
    if (device_ != nullptr) {
        device_->release(texture);
    }
}

// ----------------------------------------------------------------------------------------------------------------
// Deferred destruction

void detail::DeviceImpl::release(BufferHandle buffer) {
    retire_resource(*this, retired_buffers, buffer, [this](BufferHandle handle) { destroy_now(handle); });
}

void detail::DeviceImpl::release(TextureHandle texture) {
    retire_resource(*this, retired_textures, texture, [this](TextureHandle handle) { destroy_now(handle); });
}

void detail::DeviceImpl::write_bindless(TextureHandle texture, const vulkan::TextureRecord& record) {
    std::array<VkDescriptorImageInfo, 2> images{};
    std::array<VkWriteDescriptorSet, 2> writes{};
    uint32_t count = 0;
    const auto add = [&](vulkan::BindlessSet set, VkDescriptorType type, VkImageLayout layout) {
        images[count] = VkDescriptorImageInfo{.imageView = record.view, .imageLayout = layout};
        writes[count] = VkWriteDescriptorSet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                             .dstSet = bindless_sets[static_cast<std::size_t>(set)],
                                             .dstBinding = 0,
                                             .dstArrayElement = texture.index(),
                                             .descriptorCount = 1,
                                             .descriptorType = type,
                                             .pImageInfo = &images[count]};
        ++count;
    };
    if (record.info.usage.has(TextureUsage::Sampled)) {
        add(vulkan::BindlessSet::SampledTextures, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
    if (record.info.usage.has(TextureUsage::Storage)) {
        add(vulkan::BindlessSet::StorageTextures, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_IMAGE_LAYOUT_GENERAL);
    }
    if (count > 0) {
        const std::scoped_lock lock{bindless_mutex};
        vk.vkUpdateDescriptorSets(device, count, writes.data(), 0, nullptr);
    }
}

void detail::DeviceImpl::destroy_now(BufferHandle buffer) {
    const std::scoped_lock lock{resources_mutex};
    vulkan::BufferRecord& record = buffers.get(buffer);
    vmaDestroyBuffer(allocator, record.buffer, record.allocation);
    live_objects.remove(record.tracker_id);
    buffers.remove(buffer);
}

void detail::DeviceImpl::destroy_now(TextureHandle texture) {
    const std::scoped_lock lock{resources_mutex};
    vulkan::TextureRecord& record = textures.get(texture);
    if (record.attachment_view != record.view) {
        vk.vkDestroyImageView(device, record.attachment_view, nullptr);
    }
    vk.vkDestroyImageView(device, record.view, nullptr);
    if (!record.external) {
        vmaDestroyImage(allocator, record.image, record.allocation);
    }
    live_objects.remove(record.tracker_id);
    textures.remove(texture);
}

// ----------------------------------------------------------------------------------------------------------------
// Buffers

core::Result<Buffer> Device::create_buffer(const BufferDesc& desc) {
    BYTEE_ASSERT(is_valid(), "Device is empty");
    detail::DeviceImpl& impl = *impl_;
    if (impl.lost.load(std::memory_order_acquire)) {
        return impl.lost_error();
    }
    BYTEE_TRY(validate(desc));
    const std::string_view name = name_or_default(desc.debug_name);

    const VkBufferCreateInfo buffer_info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = desc.size,
        .usage = vulkan::to_vk_buffer_usage(desc.usage),
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    const VmaAllocationCreateInfo allocation_create_info = allocation_info_for(desc.memory);

    vulkan::BufferRecord record;
    VmaAllocationInfo allocation_info{};
    if (const VkResult result = vmaCreateBuffer(impl.allocator, &buffer_info, &allocation_create_info, &record.buffer,
                                                &record.allocation, &allocation_info);
        result != VK_SUCCESS) {
        return impl.fail(result, "vmaCreateBuffer");
    }
    record.mapped = static_cast<std::byte*>(allocation_info.pMappedData);

    const VkBufferDeviceAddressInfo address_info{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                                                 .buffer = record.buffer};
    record.info = BufferInfo{
        .size = desc.size,
        .usage = desc.usage,
        .memory = desc.memory,
        .device_address = impl.vk.vkGetBufferDeviceAddress(impl.device, &address_info),
        .debug_name = std::string{name},
    };
    vulkan::set_debug_name(impl.device, VK_OBJECT_TYPE_BUFFER, record.buffer, name);

    BufferHandle handle;
    {
        const std::scoped_lock lock{impl.resources_mutex};
        record.tracker_id = impl.live_objects.add("Buffer", name);
        handle = impl.buffers.insert(std::move(record)); // a full pool returns before consuming `record`
        if (!handle.is_valid()) {
            impl.live_objects.remove(record.tracker_id);
        }
    }
    if (!handle.is_valid()) {
        vmaDestroyBuffer(impl.allocator, record.buffer, record.allocation);
        return core::make_error(core::ErrorCode::LimitExceeded,
                                "buffer '{}': the device's buffer limit ({}) is reached", name,
                                impl.buffers.capacity());
    }
    return Buffer{handle, ResourceReleaser{impl_}};
}

const BufferInfo& Device::info(BufferHandle buffer) const noexcept {
    BYTEE_ASSERT(is_valid(), "Device is empty");
    return impl_->buffers.get(buffer).info;
}

core::Result<void> Device::write(BufferHandle buffer, uint64_t offset, std::span<const std::byte> data) {
    BYTEE_ASSERT(is_valid(), "Device is empty");
    const vulkan::BufferRecord& record = impl_->buffers.get(buffer);
    BYTEE_ASSERT(record.mapped != nullptr, "buffer '{}' is not CPU-accessible (memory: {})", record.info.debug_name,
                 to_string(record.info.memory));
    BYTEE_ASSERT(offset <= record.info.size && data.size() <= record.info.size - offset,
                 "write of {} bytes at offset {} exceeds buffer '{}' ({} bytes)", data.size(), offset,
                 record.info.debug_name, record.info.size);
    if (data.empty()) {
        return {};
    }
    std::memcpy(record.mapped + offset, data.data(), data.size());
    if (const VkResult result = vmaFlushAllocation(impl_->allocator, record.allocation, offset, data.size());
        result != VK_SUCCESS) {
        return impl_->fail(result, "vmaFlushAllocation");
    }
    return {};
}

core::Result<void> Device::read(BufferHandle buffer, uint64_t offset, std::span<std::byte> data) const {
    BYTEE_ASSERT(is_valid(), "Device is empty");
    const vulkan::BufferRecord& record = impl_->buffers.get(buffer);
    BYTEE_ASSERT(record.mapped != nullptr, "buffer '{}' is not CPU-accessible (memory: {})", record.info.debug_name,
                 to_string(record.info.memory));
    BYTEE_ASSERT(offset <= record.info.size && data.size() <= record.info.size - offset,
                 "read of {} bytes at offset {} exceeds buffer '{}' ({} bytes)", data.size(), offset,
                 record.info.debug_name, record.info.size);
    if (data.empty()) {
        return {};
    }
    if (const VkResult result = vmaInvalidateAllocation(impl_->allocator, record.allocation, offset, data.size());
        result != VK_SUCCESS) {
        return impl_->fail(result, "vmaInvalidateAllocation");
    }
    std::memcpy(data.data(), record.mapped + offset, data.size());
    return {};
}

// ----------------------------------------------------------------------------------------------------------------
// Textures

core::Result<Texture> Device::create_texture(const TextureDesc& desc) {
    BYTEE_ASSERT(is_valid(), "Device is empty");
    detail::DeviceImpl& impl = *impl_;
    if (impl.lost.load(std::memory_order_acquire)) {
        return impl.lost_error();
    }
    BYTEE_TRY(validate(desc, impl.device_limits));
    const std::string_view name = name_or_default(desc.debug_name);
    const VkFormat format = vulkan::to_vk_format(desc.format);

    VkFormatProperties format_properties{};
    vkGetPhysicalDeviceFormatProperties(impl.physical_device, format, &format_properties);
    const VkFormatFeatureFlags required = vulkan::required_format_features(desc.usage);
    if ((format_properties.optimalTilingFeatures & required) != required) {
        return core::make_error(core::ErrorCode::NotSupported,
                                "texture '{}': format {} does not support the requested usage on this GPU", name,
                                to_string(desc.format));
    }

    const bool is_3d = desc.dimension == TextureDimension::Texture3D;
    const VkImageCreateInfo image_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = is_3d ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = {.width = desc.extent.width, .height = desc.extent.height, .depth = desc.extent.depth},
        .mipLevels = desc.mip_levels,
        .arrayLayers = desc.array_layers,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = vulkan::to_vk_image_usage(desc.usage),
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    const VmaAllocationCreateInfo allocation_create_info{.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE};

    vulkan::TextureRecord record;
    if (const VkResult result = vmaCreateImage(impl.allocator, &image_info, &allocation_create_info, &record.image,
                                               &record.allocation, nullptr);
        result != VK_SUCCESS) {
        return impl.fail(result, "vmaCreateImage");
    }

    const VkImageViewCreateInfo view_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = record.image,
        .viewType = is_3d                   ? VK_IMAGE_VIEW_TYPE_3D
                    : desc.array_layers > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY
                                            : VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .subresourceRange = {.aspectMask = vulkan::aspect_mask(desc.format),
                             .baseMipLevel = 0,
                             .levelCount = desc.mip_levels,
                             .baseArrayLayer = 0,
                             .layerCount = desc.array_layers},
    };
    // Views exist only for usages that read or write through one; transfer-only textures must not have a view.
    constexpr core::Flags<TextureUsage> k_view_usages = TextureUsage::Sampled | TextureUsage::Storage |
                                                        TextureUsage::ColorAttachment |
                                                        TextureUsage::DepthStencilAttachment;
    if (desc.usage.has_any(k_view_usages)) {
        if (const VkResult result = impl.vk.vkCreateImageView(impl.device, &view_info, nullptr, &record.view);
            result != VK_SUCCESS) {
            vmaDestroyImage(impl.allocator, record.image, record.allocation);
            return impl.fail(result, "vkCreateImageView");
        }
        vulkan::set_debug_name(impl.device, VK_OBJECT_TYPE_IMAGE_VIEW, record.view, std::format("{} view", name));
    }

    // Attachments need a single-mip view; it covers mip 0 / layer 0 (rendering into other mips or layers: backlog).
    if (desc.usage.has_any(TextureUsage::ColorAttachment | TextureUsage::DepthStencilAttachment)) {
        VkImageViewCreateInfo attachment_info = view_info;
        attachment_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        attachment_info.subresourceRange = {
            .aspectMask = has_stencil(desc.format)
                              ? VkImageAspectFlags{VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT}
                              : vulkan::aspect_mask(desc.format),
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        };
        if (const VkResult result =
                impl.vk.vkCreateImageView(impl.device, &attachment_info, nullptr, &record.attachment_view);
            result != VK_SUCCESS) {
            impl.vk.vkDestroyImageView(impl.device, record.view, nullptr);
            vmaDestroyImage(impl.allocator, record.image, record.allocation);
            return impl.fail(result, "vkCreateImageView (attachment)");
        }
        vulkan::set_debug_name(impl.device, VK_OBJECT_TYPE_IMAGE_VIEW, record.attachment_view,
                               std::format("{} attachment view", name));
    }

    record.info = TextureInfo{
        .dimension = desc.dimension,
        .extent = desc.extent,
        .mip_levels = desc.mip_levels,
        .array_layers = desc.array_layers,
        .format = desc.format,
        .usage = desc.usage,
        .debug_name = std::string{name},
    };
    vulkan::set_debug_name(impl.device, VK_OBJECT_TYPE_IMAGE, record.image, name);

    TextureHandle handle;
    {
        const std::scoped_lock lock{impl.resources_mutex};
        record.tracker_id = impl.live_objects.add("Texture", name);
        handle = impl.textures.insert(std::move(record)); // a full pool returns before consuming `record`
        if (!handle.is_valid()) {
            impl.live_objects.remove(record.tracker_id);
        }
    }
    if (handle.is_valid()) {
        impl.write_bindless(handle, impl.textures.get(handle));
    } else {
        impl.vk.vkDestroyImageView(impl.device, record.attachment_view, nullptr);
        impl.vk.vkDestroyImageView(impl.device, record.view, nullptr);
        vmaDestroyImage(impl.allocator, record.image, record.allocation);
        return core::make_error(core::ErrorCode::LimitExceeded,
                                "texture '{}': the device's texture limit ({}) is reached", name,
                                impl.textures.capacity());
    }
    return Texture{handle, ResourceReleaser{impl_}};
}

const TextureInfo& Device::info(TextureHandle texture) const noexcept {
    BYTEE_ASSERT(is_valid(), "Device is empty");
    return impl_->textures.get(texture).info;
}

} // namespace bytee::render::rhi
