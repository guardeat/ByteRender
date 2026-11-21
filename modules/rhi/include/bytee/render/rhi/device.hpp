#pragma once

#include <bytee/core/compiler.hpp>
#include <bytee/core/result.hpp>
#include <bytee/render/rhi/adapter.hpp>
#include <bytee/render/rhi/buffer.hpp>
#include <bytee/render/rhi/command_list.hpp>
#include <bytee/render/rhi/export.hpp>
#include <bytee/render/rhi/handles.hpp>
#include <bytee/render/rhi/pipeline.hpp>
#include <bytee/render/rhi/queue.hpp>
#include <bytee/render/rhi/texture.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

namespace bytee::render::rhi {

// Fixed capacities of a device's resource pools (architecture §4.3). Creating more fails with LimitExceeded.
struct DeviceLimits {
    uint32_t max_buffers = 1u << 16;
    uint32_t max_textures = 1u << 16;
    uint32_t max_graphics_pipelines = 1u << 12;
};

// Which part of a texture to read back and which states the texture is in before and after.
struct TextureDownloadDesc {
    TextureRegion region;                          // zero extent: the whole mip level
    ResourceState before = ResourceState::CopySrc; // the texture's current state
    ResourceState after = ResourceState::CopySrc;  // state after the download
};

// Where texture upload data goes and which states the texture is in before and after.
struct TextureUploadDesc {
    TextureRegion region;                            // zero extent: the whole mip level
    ResourceState before = ResourceState::Undefined; // Undefined discards the previous contents
    ResourceState after = ResourceState::ShaderRead;
};

struct DeviceDesc {
    // AdapterInfo::index of the GPU to use. Empty: the best supported adapter (discrete > integrated > virtual >
    // others, then the most device-local memory).
    std::optional<uint32_t> adapter_index;
    DeviceLimits limits;
    std::string_view debug_name = "Device";
};

// A logical GPU device: creates and owns GPU resources and queues. Created by Instance::create_device().
// Move-only. Must be destroyed before the Instance that created it.
class BYTEE_RHI_API Device {
private:
    detail::DeviceImpl* impl_ = nullptr;

public:
    Device() noexcept = default;
    ~Device();

    Device(Device&& other) noexcept
        : impl_(std::exchange(other.impl_, nullptr)) {}
    Device& operator=(Device&& other) noexcept;
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;

    [[nodiscard]] bool is_valid() const noexcept { return impl_ != nullptr; }
    [[nodiscard]] const AdapterInfo& adapter() const noexcept;
    // True if swapchains can be created (the instance supports presentation and so does the adapter).
    [[nodiscard]] bool is_presentation_supported() const noexcept;

    // True once the GPU reported the device as lost. Lost is permanent: every fallible call then fails with
    // ErrorCode::DeviceLost without touching the GPU. Recover by destroying and recreating the device.
    [[nodiscard]] bool is_lost() const noexcept;

    // True if the GPU has finished all work up to and including `point` (non-blocking). False for points that were
    // never submitted and on a lost device (use wait() or is_lost() to tell these apart).
    [[nodiscard]] bool is_complete(SyncPoint point) const;

    // Blocks until `point` is complete, then releases retired GPU objects whose work has completed.
    // Errors: Timeout (allocation-free, so polling with short timeouts is cheap), InvalidArgument for a point that
    // was never submitted (it could never complete), DeviceLost.
    [[nodiscard]] core::Result<void> wait(SyncPoint point,
                                          std::chrono::nanoseconds timeout = std::chrono::nanoseconds::max());

    // Blocks until the GPU has finished all submitted work, then releases every retired GPU object.
    [[nodiscard]] core::Result<void> wait_idle();

    // Resources. Thread-safe. Errors: InvalidArgument (invalid description), NotSupported (e.g. a format/usage
    // combination the GPU lacks), LimitExceeded (pool full), OutOfMemory, DeviceLost. Destroying the returned
    // resource retires it: the GPU object lives until the GPU has finished all work that could use it.
    [[nodiscard]] core::Result<Buffer> create_buffer(const BufferDesc& desc);
    [[nodiscard]] core::Result<Texture> create_texture(const TextureDesc& desc);
    // Creating pipelines compiles shaders for the GPU: do it at load time, not per frame.
    [[nodiscard]] core::Result<GraphicsPipeline> create_graphics_pipeline(const GraphicsPipelineDesc& desc);

    // Description of a live resource. The reference stays valid until the resource is destroyed.
    [[nodiscard]] const BufferInfo& info(BufferHandle buffer) const noexcept;
    [[nodiscard]] const TextureInfo& info(TextureHandle texture) const noexcept;

    // CPU access to Upload / Readback buffers (persistently mapped; flushes/invalidates as needed). The range must lie
    // inside the buffer.
    [[nodiscard]] core::Result<void> write(BufferHandle buffer, uint64_t offset, std::span<const std::byte> data);
    [[nodiscard]] core::Result<void> read(BufferHandle buffer, uint64_t offset, std::span<std::byte> data) const;

    // Commands. Thread-safe. A command list's resources must stay alive until it is submitted.
    [[nodiscard]] core::Result<CommandList> begin_commands(QueueType queue = QueueType::Graphics);
    // Consumes the list; the returned SyncPoint completes when the GPU has executed it.
    [[nodiscard]] core::Result<SyncPoint> submit(CommandList&& commands);

    // Upload helpers: copy CPU data into GPU-local resources through a staging buffer (freed automatically once the
    // copy has completed). The returned SyncPoint completes with the copy; later work on the same queue may use the
    // data without waiting. Errors: InvalidArgument (size mismatch), plus resource-creation errors.
    [[nodiscard]] core::Result<SyncPoint> upload(BufferHandle dst, uint64_t dst_offset,
                                                 std::span<const std::byte> data);
    [[nodiscard]] core::Result<SyncPoint> upload(TextureHandle dst, std::span<const std::byte> data,
                                                 const TextureUploadDesc& desc);

    // Reads a texture region back into `data` (tightly packed texels; its size must match the region). Blocks until
    // the GPU has finished all work submitted before it — for screenshots, tests and tools, not per-frame use.
    // The texture needs TransferSrc usage.
    [[nodiscard]] core::Result<void> download(TextureHandle src, std::span<std::byte> data,
                                              const TextureDownloadDesc& desc);

private:
    friend class Instance;
    // Backend access to the implementation (defined by the backend).
    friend detail::DeviceImpl* get_impl(const Device& device) noexcept;

    explicit Device(detail::DeviceImpl* impl) noexcept
        : impl_(impl) {}
};

} // namespace bytee::render::rhi
