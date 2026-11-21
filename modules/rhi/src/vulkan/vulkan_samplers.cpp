// Samplers: creation into the bindless heap and deferred destruction.

#include "vulkan/vulkan_convert.hpp"
#include "vulkan/vulkan_device.hpp"

#include <bytee/core/assert.hpp>

#include <string>

namespace bytee::render::rhi {

void ResourceReleaser::operator()(SamplerHandle sampler) const noexcept {
    if (device_ != nullptr) {
        device_->release(sampler);
    }
}

void detail::DeviceImpl::release(SamplerHandle sampler) {
    const vulkan::DeviceQueueValues retire_after = last_submitted();
    if (detail::is_reached(retire_after, cached_completed())) {
        destroy_now(sampler);
        return;
    }
    retired_samplers.push(retire_after, sampler);
}

void detail::DeviceImpl::destroy_now(SamplerHandle sampler) {
    const std::scoped_lock lock{resources_mutex};
    vulkan::SamplerRecord& record = samplers.get(sampler);
    vk.vkDestroySampler(device, record.sampler, nullptr);
    live_objects.remove(record.tracker_id);
    samplers.remove(sampler);
}

void detail::DeviceImpl::write_bindless(SamplerHandle sampler, VkSampler handle) {
    const VkDescriptorImageInfo image{.sampler = handle};
    const VkWriteDescriptorSet write{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                     .dstSet = bindless_sets[static_cast<std::size_t>(vulkan::BindlessSet::Samplers)],
                                     .dstBinding = 0,
                                     .dstArrayElement = sampler.index(),
                                     .descriptorCount = 1,
                                     .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
                                     .pImageInfo = &image};
    const std::scoped_lock lock{bindless_mutex};
    vk.vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
}

core::Result<Sampler> Device::create_sampler(const SamplerDesc& desc) {
    BYTEE_ASSERT(is_valid(), "Device is empty");
    detail::DeviceImpl& impl = *impl_;
    if (impl.lost.load(std::memory_order_acquire)) {
        return impl.lost_error();
    }
    const std::string_view name = desc.debug_name.empty() ? std::string_view{"(unnamed)"} : desc.debug_name;
    if (desc.max_anisotropy < 1.0f || desc.max_anisotropy > impl.device_limits.maxSamplerAnisotropy) {
        return core::make_error(core::ErrorCode::InvalidArgument, "sampler '{}': anisotropy {} outside [1, {}]", name,
                                desc.max_anisotropy, impl.device_limits.maxSamplerAnisotropy);
    }
    if (desc.min_lod < 0.0f || desc.min_lod > desc.max_lod) {
        return core::make_error(core::ErrorCode::InvalidArgument, "sampler '{}': invalid LOD range [{}, {}]", name,
                                desc.min_lod, desc.max_lod);
    }

    const VkSamplerCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = vulkan::to_vk_filter(desc.mag_filter),
        .minFilter = vulkan::to_vk_filter(desc.min_filter),
        .mipmapMode = vulkan::to_vk_mipmap_mode(desc.mipmap_mode),
        .addressModeU = vulkan::to_vk_address_mode(desc.address_u),
        .addressModeV = vulkan::to_vk_address_mode(desc.address_v),
        .addressModeW = vulkan::to_vk_address_mode(desc.address_w),
        .anisotropyEnable = desc.max_anisotropy > 1.0f ? VK_TRUE : VK_FALSE,
        .maxAnisotropy = desc.max_anisotropy,
        .compareEnable = desc.compare.has_value() ? VK_TRUE : VK_FALSE,
        .compareOp = vulkan::to_vk_compare_op(desc.compare.value_or(CompareOp::Always)),
        .minLod = desc.min_lod,
        .maxLod = desc.max_lod,
        .borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
    };
    vulkan::SamplerRecord record;
    if (const VkResult result = impl.vk.vkCreateSampler(impl.device, &info, nullptr, &record.sampler);
        result != VK_SUCCESS) {
        return impl.fail(result, "vkCreateSampler");
    }
    record.debug_name = name;
    vulkan::set_debug_name(impl.device, VK_OBJECT_TYPE_SAMPLER, record.sampler, name);

    const VkSampler sampler = record.sampler;
    SamplerHandle handle;
    {
        const std::scoped_lock lock{impl.resources_mutex};
        record.tracker_id = impl.live_objects.add("Sampler", name);
        handle = impl.samplers.insert(std::move(record)); // a full pool returns before consuming `record`
        if (!handle.is_valid()) {
            impl.live_objects.remove(record.tracker_id);
        }
    }
    if (!handle.is_valid()) {
        impl.vk.vkDestroySampler(impl.device, sampler, nullptr);
        return core::make_error(core::ErrorCode::LimitExceeded,
                                "sampler '{}': the device's sampler limit ({}) is reached", name,
                                impl.samplers.capacity());
    }
    impl.write_bindless(handle, sampler);
    return Sampler{handle, ResourceReleaser{impl_}};
}

} // namespace bytee::render::rhi
