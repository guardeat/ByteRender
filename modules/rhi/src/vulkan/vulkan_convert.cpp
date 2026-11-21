#include "vulkan/vulkan_convert.hpp"

namespace bytee::render::rhi::vulkan {

VkFormat to_vk_format(Format format) noexcept {
    switch (format) {
    case Format::Undefined: return VK_FORMAT_UNDEFINED;
    case Format::R8Unorm: return VK_FORMAT_R8_UNORM;
    case Format::RG8Unorm: return VK_FORMAT_R8G8_UNORM;
    case Format::RGBA8Unorm: return VK_FORMAT_R8G8B8A8_UNORM;
    case Format::RGBA8Srgb: return VK_FORMAT_R8G8B8A8_SRGB;
    case Format::BGRA8Unorm: return VK_FORMAT_B8G8R8A8_UNORM;
    case Format::BGRA8Srgb: return VK_FORMAT_B8G8R8A8_SRGB;
    case Format::R16Float: return VK_FORMAT_R16_SFLOAT;
    case Format::RG16Float: return VK_FORMAT_R16G16_SFLOAT;
    case Format::RGBA16Float: return VK_FORMAT_R16G16B16A16_SFLOAT;
    case Format::R32Float: return VK_FORMAT_R32_SFLOAT;
    case Format::RG32Float: return VK_FORMAT_R32G32_SFLOAT;
    case Format::RGBA32Float: return VK_FORMAT_R32G32B32A32_SFLOAT;
    case Format::R32Uint: return VK_FORMAT_R32_UINT;
    case Format::RGB10A2Unorm: return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
    case Format::RG11B10Float: return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
    case Format::D16Unorm: return VK_FORMAT_D16_UNORM;
    case Format::D32Float: return VK_FORMAT_D32_SFLOAT;
    case Format::D32FloatS8Uint: return VK_FORMAT_D32_SFLOAT_S8_UINT;
    }
    return VK_FORMAT_UNDEFINED;
}

VkBufferUsageFlags to_vk_buffer_usage(core::Flags<BufferUsage> usage) noexcept {
    // Every buffer is addressable from shaders (bindless access through buffer device address).
    VkBufferUsageFlags flags = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    if (usage.has(BufferUsage::TransferSrc))
        flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if (usage.has(BufferUsage::TransferDst))
        flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if (usage.has(BufferUsage::Uniform))
        flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    if (usage.has(BufferUsage::Storage))
        flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if (usage.has(BufferUsage::Index))
        flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if (usage.has(BufferUsage::Vertex))
        flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if (usage.has(BufferUsage::Indirect))
        flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    return flags;
}

VkImageUsageFlags to_vk_image_usage(core::Flags<TextureUsage> usage) noexcept {
    VkImageUsageFlags flags = 0;
    if (usage.has(TextureUsage::Sampled))
        flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if (usage.has(TextureUsage::Storage))
        flags |= VK_IMAGE_USAGE_STORAGE_BIT;
    if (usage.has(TextureUsage::ColorAttachment))
        flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (usage.has(TextureUsage::DepthStencilAttachment))
        flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if (usage.has(TextureUsage::TransferSrc))
        flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if (usage.has(TextureUsage::TransferDst))
        flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    return flags;
}

VkFormatFeatureFlags required_format_features(core::Flags<TextureUsage> usage) noexcept {
    VkFormatFeatureFlags flags = 0;
    if (usage.has(TextureUsage::Sampled))
        flags |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
    if (usage.has(TextureUsage::Storage))
        flags |= VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT;
    if (usage.has(TextureUsage::ColorAttachment))
        flags |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
    if (usage.has(TextureUsage::DepthStencilAttachment))
        flags |= VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if (usage.has(TextureUsage::TransferSrc))
        flags |= VK_FORMAT_FEATURE_TRANSFER_SRC_BIT;
    if (usage.has(TextureUsage::TransferDst))
        flags |= VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
    return flags;
}

VkImageAspectFlags aspect_mask(Format format) noexcept {
    return is_depth_format(format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
}

} // namespace bytee::render::rhi::vulkan
