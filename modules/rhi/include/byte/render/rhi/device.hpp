#pragma once

#include <byte/core/result.hpp>
#include <byte/render/rhi/adapter.hpp>
#include <byte/render/rhi/export.hpp>
#include <byte/render/rhi/queue.hpp>

#include <chrono>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

namespace byte::render::rhi {

struct DeviceDesc {
    // Adapter to use (AdapterInfo::index). Empty: the best supported adapter (discrete > integrated > others).
    std::optional<uint32_t> adapter_index;
    std::string_view debug_name = "Device";
};

// A logical GPU device: creates and owns all GPU resources. Created by Instance::create_device().
// Move-only. Must be destroyed before the Instance that created it.
class BYTE_RENDER_RHI_API Device {
public:
    struct Impl; // backend-defined, opaque to users

    Device() noexcept = default;
    ~Device();

    Device(Device&& other) noexcept : m_impl(std::exchange(other.m_impl, nullptr)) {}
    Device& operator=(Device&& other) noexcept;
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;

    [[nodiscard]] bool is_valid() const noexcept { return m_impl != nullptr; }
    [[nodiscard]] const AdapterInfo& adapter() const noexcept;
    // True if the device can present to window surfaces (swapchains).
    [[nodiscard]] bool supports_presentation() const noexcept;

    // True if the GPU has finished all work up to and including `point` (non-blocking).
    [[nodiscard]] bool is_complete(SyncPoint point) const;

    // Blocks until `point` is complete. Fails with ErrorCode::Timeout if `timeout` expires first, and with
    // ErrorCode::InvalidArgument if `point` was never submitted (it could never complete).
    [[nodiscard]] core::Result<void> wait(SyncPoint point,
                                          std::chrono::nanoseconds timeout = std::chrono::nanoseconds::max());

    // Blocks until the GPU has finished all submitted work, then releases every retired GPU object.
    [[nodiscard]] core::Result<void> wait_idle();

private:
    friend class Instance;
    // Backend access to the implementation (declared and defined by the backend).
    friend Impl* get_impl(const Device& device) noexcept;

    explicit Device(Impl* impl) noexcept : m_impl(impl) {}

    Impl* m_impl = nullptr;
};

} // namespace byte::render::rhi
