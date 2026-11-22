#pragma once

#include <bytee/core/compiler.hpp>
#include <bytee/core/handle.hpp>
#include <bytee/core/resource.hpp>
#include <bytee/render/rhi/export.hpp>

namespace bytee::render::rhi {

namespace detail {
struct BYTEE_HIDDEN DeviceImpl; // backend-defined, opaque
} // namespace detail

// Tags: empty marker types that make handles to different resource kinds distinct types (architecture §4.1).
struct BufferTag;
struct TextureTag;
struct GraphicsPipelineTag;
struct ComputePipelineTag;
struct SamplerTag;

// Non-owning 8-byte views, used by command recording and lookups.
using BufferHandle = core::Handle<BufferTag>;
using TextureHandle = core::Handle<TextureTag>;
using GraphicsPipelineHandle = core::Handle<GraphicsPipelineTag>;
using ComputePipelineHandle = core::Handle<ComputePipelineTag>;
using SamplerHandle = core::Handle<SamplerTag>;

// Bindless indices (architecture §7): a resource's slot in the device's descriptor heap is its pool index. Pass these
// to shaders (push constants or buffers) and use them with the helpers in shaders/bytee/bindless.glsl.
[[nodiscard]] constexpr uint32_t bindless_index(TextureHandle texture) noexcept {
    return texture.index();
}
[[nodiscard]] constexpr uint32_t bindless_index(SamplerHandle sampler) noexcept {
    return sampler.index();
}

// Deleter of every RHI resource: hands the handle back to its device, which destroys the GPU object once the GPU has
// finished all work that could use it (deferred destruction, architecture §6). Thread-safe.
class BYTEE_RHI_API ResourceReleaser {
private:
    detail::DeviceImpl* device_ = nullptr;

public:
    constexpr ResourceReleaser() noexcept = default;
    constexpr explicit ResourceReleaser(detail::DeviceImpl* device) noexcept
        : device_(device) {}

    void operator()(BufferHandle buffer) const noexcept;
    void operator()(TextureHandle texture) const noexcept;
    void operator()(GraphicsPipelineHandle pipeline) const noexcept;
    void operator()(ComputePipelineHandle pipeline) const noexcept;
    void operator()(SamplerHandle sampler) const noexcept;
};

// Owning, move-only RAII resources (handle + device pointer, 16 bytes). Usable wherever a handle is expected.
using Buffer = core::Resource<BufferTag, ResourceReleaser>;
using Texture = core::Resource<TextureTag, ResourceReleaser>;
using GraphicsPipeline = core::Resource<GraphicsPipelineTag, ResourceReleaser>;
using ComputePipeline = core::Resource<ComputePipelineTag, ResourceReleaser>;
using Sampler = core::Resource<SamplerTag, ResourceReleaser>;

} // namespace bytee::render::rhi
