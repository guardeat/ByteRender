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

StateScope to_state_scope(ResourceState state) noexcept {
    constexpr VkPipelineStageFlags2 k_shaders = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT |
                                                VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT |
                                                VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    constexpr VkPipelineStageFlags2 k_depth_tests =
        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
    switch (state) {
    // Undefined waits for all prior commands (execution only, no memory access): a transition out of Undefined right
    // after a swapchain acquire must chain with the acquire semaphore wait (which uses ALL_COMMANDS).
    case ResourceState::Undefined:
        return {VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_UNDEFINED};
    case ResourceState::General:
        return {VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
                VK_IMAGE_LAYOUT_GENERAL};
    case ResourceState::CopySrc:
        return {VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL};
    case ResourceState::CopyDst:
        return {VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL};
    case ResourceState::ShaderRead:
        return {k_shaders, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    case ResourceState::ShaderWrite:
        return {k_shaders, VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                VK_IMAGE_LAYOUT_GENERAL};
    case ResourceState::ColorAttachment:
        return {VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    case ResourceState::DepthWrite:
        return {k_depth_tests,
                VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    case ResourceState::DepthRead:
        return {k_depth_tests | k_shaders,
                VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL};
    case ResourceState::VertexBuffer:
        return {VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT, VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED};
    case ResourceState::IndexBuffer:
        return {VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT, VK_ACCESS_2_INDEX_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED};
    case ResourceState::IndirectBuffer:
        return {VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED};
    case ResourceState::UniformBuffer: return {k_shaders, VK_ACCESS_2_UNIFORM_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED};
    // Presentation is ordered by the submission's semaphore signal (ALL_COMMANDS); no access after the transition.
    case ResourceState::Present: return {VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR};
    }
    return {};
}

bool is_buffer_state(ResourceState state) noexcept {
    switch (state) {
    case ResourceState::ColorAttachment:
    case ResourceState::DepthWrite:
    case ResourceState::DepthRead:
    case ResourceState::Present: return false;
    default: return true;
    }
}

VkPrimitiveTopology to_vk_topology(PrimitiveTopology topology) noexcept {
    switch (topology) {
    case PrimitiveTopology::TriangleList: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    case PrimitiveTopology::TriangleStrip: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    case PrimitiveTopology::LineList: return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    case PrimitiveTopology::LineStrip: return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
    case PrimitiveTopology::PointList: return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    }
    return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
}

VkCullModeFlags to_vk_cull_mode(CullMode mode) noexcept {
    switch (mode) {
    case CullMode::None: return VK_CULL_MODE_NONE;
    case CullMode::Front: return VK_CULL_MODE_FRONT_BIT;
    case CullMode::Back: return VK_CULL_MODE_BACK_BIT;
    }
    return VK_CULL_MODE_NONE;
}

VkFrontFace to_vk_front_face(FrontFace face) noexcept {
    // The viewport is flipped (negative height) so clip space is Y-up; the flip also mirrors winding, which makes the
    // Vulkan winding match the Y-up winding one to one (verified by the culling image test).
    return face == FrontFace::CounterClockwise ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
}

VkCompareOp to_vk_compare_op(CompareOp op) noexcept {
    switch (op) {
    case CompareOp::Never: return VK_COMPARE_OP_NEVER;
    case CompareOp::Less: return VK_COMPARE_OP_LESS;
    case CompareOp::Equal: return VK_COMPARE_OP_EQUAL;
    case CompareOp::LessOrEqual: return VK_COMPARE_OP_LESS_OR_EQUAL;
    case CompareOp::Greater: return VK_COMPARE_OP_GREATER;
    case CompareOp::NotEqual: return VK_COMPARE_OP_NOT_EQUAL;
    case CompareOp::GreaterOrEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
    case CompareOp::Always: return VK_COMPARE_OP_ALWAYS;
    }
    return VK_COMPARE_OP_ALWAYS;
}

VkPipelineColorBlendAttachmentState to_vk_blend(BlendMode mode) noexcept {
    constexpr VkColorComponentFlags k_all =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendAttachmentState state{.blendEnable = VK_TRUE,
                                              .colorBlendOp = VK_BLEND_OP_ADD,
                                              .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                                              .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                                              .alphaBlendOp = VK_BLEND_OP_ADD,
                                              .colorWriteMask = k_all};
    switch (mode) {
    case BlendMode::Opaque:
        return VkPipelineColorBlendAttachmentState{.blendEnable = VK_FALSE, .colorWriteMask = k_all};
    case BlendMode::AlphaBlend:
        state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        return state;
    case BlendMode::Premultiplied:
        state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        return state;
    case BlendMode::Additive:
        state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        return state;
    }
    return state;
}

VkIndexType to_vk_index_type(IndexType type) noexcept {
    return type == IndexType::Uint16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
}

VkFilter to_vk_filter(Filter filter) noexcept {
    return filter == Filter::Nearest ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
}

VkSamplerMipmapMode to_vk_mipmap_mode(MipmapMode mode) noexcept {
    return mode == MipmapMode::Nearest ? VK_SAMPLER_MIPMAP_MODE_NEAREST : VK_SAMPLER_MIPMAP_MODE_LINEAR;
}

VkSamplerAddressMode to_vk_address_mode(AddressMode mode) noexcept {
    switch (mode) {
    case AddressMode::Repeat: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    case AddressMode::MirroredRepeat: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    case AddressMode::ClampToEdge: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    }
    return VK_SAMPLER_ADDRESS_MODE_REPEAT;
}

bool is_texture_state(ResourceState state) noexcept {
    switch (state) {
    case ResourceState::VertexBuffer:
    case ResourceState::IndexBuffer:
    case ResourceState::IndirectBuffer:
    case ResourceState::UniformBuffer: return false;
    default: return true;
    }
}

} // namespace bytee::render::rhi::vulkan
