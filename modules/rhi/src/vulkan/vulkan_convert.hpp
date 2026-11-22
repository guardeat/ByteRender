#pragma once

// Translation of RHI types into Vulkan types.

#include "vulkan/vulkan_common.hpp"

#include <bytee/render/rhi/buffer.hpp>
#include <bytee/render/rhi/command_list.hpp>
#include <bytee/render/rhi/format.hpp>
#include <bytee/render/rhi/pipeline.hpp>
#include <bytee/render/rhi/sampler.hpp>
#include <bytee/render/rhi/texture.hpp>

namespace bytee::render::rhi::vulkan {

[[nodiscard]] VkFormat to_vk_format(Format format) noexcept;
[[nodiscard]] VkBufferUsageFlags to_vk_buffer_usage(core::Flags<BufferUsage> usage) noexcept;
[[nodiscard]] VkImageUsageFlags to_vk_image_usage(core::Flags<TextureUsage> usage) noexcept;
// Format features (optimal tiling) a texture with `usage` needs.
[[nodiscard]] VkFormatFeatureFlags required_format_features(core::Flags<TextureUsage> usage) noexcept;
[[nodiscard]] VkImageAspectFlags aspect_mask(Format format) noexcept; // view aspect: depth only for depth formats

// Synchronization2 scope of a resource state.
struct StateScope {
    VkPipelineStageFlags2 stages = VK_PIPELINE_STAGE_2_NONE;
    VkAccessFlags2 access = VK_ACCESS_2_NONE;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED; // textures only
};

[[nodiscard]] StateScope to_state_scope(ResourceState state) noexcept;

[[nodiscard]] VkPrimitiveTopology to_vk_topology(PrimitiveTopology topology) noexcept;
[[nodiscard]] VkCullModeFlags to_vk_cull_mode(CullMode mode) noexcept;
[[nodiscard]] VkFrontFace to_vk_front_face(FrontFace face) noexcept;
[[nodiscard]] VkCompareOp to_vk_compare_op(CompareOp op) noexcept;
[[nodiscard]] VkPipelineColorBlendAttachmentState to_vk_blend(BlendMode mode) noexcept;
[[nodiscard]] VkIndexType to_vk_index_type(IndexType type) noexcept;
[[nodiscard]] VkFilter to_vk_filter(Filter filter) noexcept;
[[nodiscard]] VkSamplerMipmapMode to_vk_mipmap_mode(MipmapMode mode) noexcept;
[[nodiscard]] VkSamplerAddressMode to_vk_address_mode(AddressMode mode) noexcept;

} // namespace bytee::render::rhi::vulkan
