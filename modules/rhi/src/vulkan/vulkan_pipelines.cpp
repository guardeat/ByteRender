// Graphics pipelines: creation (dynamic rendering, vertex pulling, global layout) and deferred destruction.

#include "vulkan/vulkan_convert.hpp"
#include "vulkan/vulkan_device.hpp"

#include <bytee/core/assert.hpp>

#include <array>
#include <format>
#include <string>

namespace bytee::render::rhi {

namespace {

constexpr uint32_t k_spirv_magic = 0x07230203u;

std::string_view name_or_default(std::string_view name) noexcept {
    return name.empty() ? std::string_view{"(unnamed)"} : name;
}

core::Result<void> validate_shader(const ShaderDesc& shader, std::string_view stage, std::string_view pipeline) {
    if (shader.spirv.empty() || shader.spirv[0] != k_spirv_magic) {
        return core::make_error(core::ErrorCode::InvalidArgument, "pipeline '{}': {} shader is not SPIR-V", pipeline,
                                stage);
    }
    if (shader.entry_point.empty()) {
        return core::make_error(core::ErrorCode::InvalidArgument, "pipeline '{}': {} shader has no entry point",
                                pipeline, stage);
    }
    return {};
}

// RAII shader module: only needed while the pipeline is created.
class ShaderModule {
private:
    const detail::DeviceImpl& impl_;
    VkShaderModule module_ = VK_NULL_HANDLE;

public:
    explicit ShaderModule(const detail::DeviceImpl& impl) noexcept
        : impl_(impl) {}
    ShaderModule(const ShaderModule&) = delete;
    ShaderModule& operator=(const ShaderModule&) = delete;
    ~ShaderModule() {
        if (module_ != VK_NULL_HANDLE) {
            impl_.vk.vkDestroyShaderModule(impl_.device, module_, nullptr);
        }
    }

    [[nodiscard]] VkResult create(std::span<const uint32_t> spirv) {
        const VkShaderModuleCreateInfo info{.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                            .codeSize = spirv.size_bytes(),
                                            .pCode = spirv.data()};
        return impl_.vk.vkCreateShaderModule(impl_.device, &info, nullptr, &module_);
    }
    [[nodiscard]] VkShaderModule handle() const noexcept { return module_; }
};

} // namespace

void ResourceReleaser::operator()(GraphicsPipelineHandle pipeline) const noexcept {
    if (device_ != nullptr) {
        device_->release(pipeline);
    }
}

void detail::DeviceImpl::release(GraphicsPipelineHandle pipeline) {
    retire_or_destroy(retired_graphics_pipelines, pipeline,
                      [this](GraphicsPipelineHandle handle) { destroy_now(handle); });
}

void ResourceReleaser::operator()(ComputePipelineHandle pipeline) const noexcept {
    if (device_ != nullptr) {
        device_->release(pipeline);
    }
}

void detail::DeviceImpl::release(ComputePipelineHandle pipeline) {
    retire_or_destroy(retired_compute_pipelines, pipeline,
                      [this](ComputePipelineHandle handle) { destroy_now(handle); });
}

void detail::DeviceImpl::destroy_now(ComputePipelineHandle pipeline) {
    const std::scoped_lock lock{resources_mutex};
    vulkan::PipelineRecord& record = compute_pipelines.get(pipeline);
    vk.vkDestroyPipeline(device, record.pipeline, nullptr);
    live_objects.remove(record.tracker_id);
    compute_pipelines.remove(pipeline);
}

void detail::DeviceImpl::destroy_now(GraphicsPipelineHandle pipeline) {
    const std::scoped_lock lock{resources_mutex};
    vulkan::PipelineRecord& record = graphics_pipelines.get(pipeline);
    vk.vkDestroyPipeline(device, record.pipeline, nullptr);
    live_objects.remove(record.tracker_id);
    graphics_pipelines.remove(pipeline);
}

core::Result<GraphicsPipeline> Device::create_graphics_pipeline(const GraphicsPipelineDesc& desc) {
    BYTEE_ASSERT(is_valid(), "Device is empty");
    detail::DeviceImpl& impl = *impl_;
    if (impl.lost.load(std::memory_order_acquire)) {
        return impl.lost_error();
    }
    const std::string_view name = name_or_default(desc.debug_name);

    BYTEE_TRY(validate_shader(desc.vertex, "vertex", name));
    const bool has_fragment = !desc.fragment.spirv.empty();
    if (has_fragment) {
        BYTEE_TRY(validate_shader(desc.fragment, "fragment", name));
    } else if (!desc.color_formats.empty()) {
        return core::make_error(core::ErrorCode::InvalidArgument,
                                "pipeline '{}': color attachments need a fragment shader", name);
    }
    if (desc.color_formats.size() > k_max_color_attachments) {
        return core::make_error(core::ErrorCode::InvalidArgument, "pipeline '{}': {} color formats, at most {}", name,
                                desc.color_formats.size(), k_max_color_attachments);
    }
    for (const Format format : desc.color_formats) {
        if (format == Format::Undefined || is_depth_format(format)) {
            return core::make_error(core::ErrorCode::InvalidArgument, "pipeline '{}': {} is not a color format", name,
                                    to_string(format));
        }
    }
    if (desc.depth_format != Format::Undefined && !is_depth_format(desc.depth_format)) {
        return core::make_error(core::ErrorCode::InvalidArgument, "pipeline '{}': {} is not a depth format", name,
                                to_string(desc.depth_format));
    }
    if ((desc.depth.test || desc.depth.write) && desc.depth_format == Format::Undefined) {
        return core::make_error(core::ErrorCode::InvalidArgument,
                                "pipeline '{}': depth test/write needs a depth format", name);
    }

    ShaderModule vertex{impl};
    ShaderModule fragment{impl};
    if (const VkResult result = vertex.create(desc.vertex.spirv); result != VK_SUCCESS) {
        return impl.fail(result, "vkCreateShaderModule (vertex)");
    }
    if (has_fragment) {
        if (const VkResult result = fragment.create(desc.fragment.spirv); result != VK_SUCCESS) {
            return impl.fail(result, "vkCreateShaderModule (fragment)");
        }
    }
    const std::string vertex_entry{desc.vertex.entry_point};
    const std::string fragment_entry{desc.fragment.entry_point};
    const std::array stages{
        VkPipelineShaderStageCreateInfo{.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                        .stage = VK_SHADER_STAGE_VERTEX_BIT,
                                        .module = vertex.handle(),
                                        .pName = vertex_entry.c_str()},
        VkPipelineShaderStageCreateInfo{.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                                        .module = fragment.handle(),
                                        .pName = fragment_entry.c_str()},
    };

    std::array<VkFormat, k_max_color_attachments> color_formats{};
    std::array<VkPipelineColorBlendAttachmentState, k_max_color_attachments> blend_states{};
    for (std::size_t index = 0; index < desc.color_formats.size(); ++index) {
        color_formats[index] = vulkan::to_vk_format(desc.color_formats[index]);
        blend_states[index] = vulkan::to_vk_blend(desc.blend);
    }
    const VkFormat depth_format = vulkan::to_vk_format(desc.depth_format);
    const VkPipelineRenderingCreateInfo rendering{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = static_cast<uint32_t>(desc.color_formats.size()),
        .pColorAttachmentFormats = color_formats.data(),
        .depthAttachmentFormat = depth_format,
        .stencilAttachmentFormat = has_stencil(desc.depth_format) ? depth_format : VK_FORMAT_UNDEFINED,
    };

    const VkPipelineVertexInputStateCreateInfo vertex_input{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO}; // vertex pulling
    const VkPipelineInputAssemblyStateCreateInfo input_assembly{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = vulkan::to_vk_topology(desc.topology)};
    const VkPipelineViewportStateCreateInfo viewport{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, .viewportCount = 1, .scissorCount = 1};
    const VkPipelineRasterizationStateCreateInfo rasterization{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = vulkan::to_vk_cull_mode(desc.cull),
        .frontFace = vulkan::to_vk_front_face(desc.front_face),
        .lineWidth = 1.0f,
    };
    const VkPipelineMultisampleStateCreateInfo multisample{.sType =
                                                               VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                                                           .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};
    const VkPipelineDepthStencilStateCreateInfo depth_stencil{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = desc.depth.test ? VK_TRUE : VK_FALSE,
        .depthWriteEnable = desc.depth.write ? VK_TRUE : VK_FALSE,
        .depthCompareOp = vulkan::to_vk_compare_op(desc.depth.compare),
    };
    const VkPipelineColorBlendStateCreateInfo blend{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = static_cast<uint32_t>(desc.color_formats.size()),
        .pAttachments = blend_states.data(),
    };
    constexpr std::array k_dynamic_states{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    const VkPipelineDynamicStateCreateInfo dynamic{.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                                                   .dynamicStateCount = static_cast<uint32_t>(k_dynamic_states.size()),
                                                   .pDynamicStates = k_dynamic_states.data()};

    const VkGraphicsPipelineCreateInfo pipeline_info{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &rendering,
        .stageCount = has_fragment ? 2u : 1u,
        .pStages = stages.data(),
        .pVertexInputState = &vertex_input,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport,
        .pRasterizationState = &rasterization,
        .pMultisampleState = &multisample,
        .pDepthStencilState = &depth_stencil,
        .pColorBlendState = &blend,
        .pDynamicState = &dynamic,
        .layout = impl.pipeline_layout,
    };

    vulkan::PipelineRecord record;
    if (const VkResult result = impl.vk.vkCreateGraphicsPipelines(impl.device, VK_NULL_HANDLE, 1, &pipeline_info,
                                                                  nullptr, &record.pipeline);
        result != VK_SUCCESS) {
        return impl.fail(result, "vkCreateGraphicsPipelines");
    }
    record.debug_name = name;
    vulkan::set_debug_name(impl.device, VK_OBJECT_TYPE_PIPELINE, record.pipeline, name);

    GraphicsPipelineHandle handle;
    {
        const std::scoped_lock lock{impl.resources_mutex};
        record.tracker_id = impl.live_objects.add("GraphicsPipeline", name);
        handle = impl.graphics_pipelines.insert(std::move(record)); // a full pool returns before consuming `record`
        if (!handle.is_valid()) {
            impl.live_objects.remove(record.tracker_id);
        }
    }
    if (!handle.is_valid()) {
        impl.vk.vkDestroyPipeline(impl.device, record.pipeline, nullptr);
        return core::make_error(core::ErrorCode::LimitExceeded,
                                "pipeline '{}': the device's graphics pipeline capacity ({}) is reached", name,
                                impl.graphics_pipelines.capacity());
    }
    return GraphicsPipeline{handle, ResourceReleaser{impl_}};
}

core::Result<ComputePipeline> Device::create_compute_pipeline(const ComputePipelineDesc& desc) {
    BYTEE_ASSERT(is_valid(), "Device is empty");
    detail::DeviceImpl& impl = *impl_;
    if (impl.lost.load(std::memory_order_acquire)) {
        return impl.lost_error();
    }
    const std::string_view name = name_or_default(desc.debug_name);
    BYTEE_TRY(validate_shader(desc.shader, "compute", name));

    ShaderModule shader{impl};
    if (const VkResult result = shader.create(desc.shader.spirv); result != VK_SUCCESS) {
        return impl.fail(result, "vkCreateShaderModule (compute)");
    }
    const std::string entry{desc.shader.entry_point};
    const VkComputePipelineCreateInfo pipeline_info{
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                  .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                  .module = shader.handle(),
                  .pName = entry.c_str()},
        .layout = impl.pipeline_layout,
    };
    vulkan::PipelineRecord record;
    if (const VkResult result =
            impl.vk.vkCreateComputePipelines(impl.device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &record.pipeline);
        result != VK_SUCCESS) {
        return impl.fail(result, "vkCreateComputePipelines");
    }
    record.debug_name = name;
    vulkan::set_debug_name(impl.device, VK_OBJECT_TYPE_PIPELINE, record.pipeline, name);

    const VkPipeline created = record.pipeline;
    ComputePipelineHandle handle;
    {
        const std::scoped_lock lock{impl.resources_mutex};
        record.tracker_id = impl.live_objects.add("ComputePipeline", name);
        handle = impl.compute_pipelines.insert(std::move(record)); // a full pool returns before consuming `record`
        if (!handle.is_valid()) {
            impl.live_objects.remove(record.tracker_id);
        }
    }
    if (!handle.is_valid()) {
        impl.vk.vkDestroyPipeline(impl.device, created, nullptr);
        return core::make_error(core::ErrorCode::LimitExceeded,
                                "pipeline '{}': the device's compute pipeline capacity ({}) is reached", name,
                                impl.compute_pipelines.capacity());
    }
    return ComputePipeline{handle, ResourceReleaser{impl_}};
}

} // namespace bytee::render::rhi
