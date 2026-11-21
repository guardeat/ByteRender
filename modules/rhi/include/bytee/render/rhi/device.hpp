#pragma once

#include <bytee/core/compiler.hpp>
#include <bytee/core/result.hpp>
#include <bytee/render/rhi/adapter.hpp>
#include <bytee/render/rhi/export.hpp>
#include <bytee/render/rhi/queue.hpp>

#include <chrono>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

namespace bytee::render::rhi {

struct DeviceDesc {
    // AdapterInfo::index of the GPU to use. Empty: the best supported adapter (discrete > integrated > virtual >
    // others, then the most device-local memory).
    std::optional<uint32_t> adapter_index;
    std::string_view debug_name = "Device";
};

// A logical GPU device: creates and owns GPU resources and queues. Created by Instance::create_device().
// Move-only. Must be destroyed before the Instance that created it.
class BYTEE_RHI_API Device {
public:
    struct BYTEE_HIDDEN Impl; // backend-defined, opaque

private:
    Impl* impl_ = nullptr;

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

private:
    friend class Instance;
    // Backend access to the implementation (defined by the backend).
    friend Impl* get_impl(const Device& device) noexcept;

    explicit Device(Impl* impl) noexcept
        : impl_(impl) {}
};

} // namespace bytee::render::rhi
