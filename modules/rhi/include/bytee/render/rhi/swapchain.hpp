#pragma once

#include <bytee/core/compiler.hpp>
#include <bytee/core/result.hpp>
#include <bytee/core/types.hpp>
#include <bytee/render/rhi/export.hpp>
#include <bytee/render/rhi/format.hpp>
#include <bytee/render/rhi/handles.hpp>
#include <bytee/render/rhi/native_window.hpp>

#include <cstdint>
#include <span>
#include <string_view>
#include <utility>

namespace bytee::render::rhi {

namespace detail {
struct BYTEE_HIDDEN SwapchainImpl; // backend-defined, opaque
} // namespace detail

enum class PresentMode : uint8_t {
    Fifo,      // vsync, never tears; always available
    Mailbox,   // vsync, lowest latency; falls back to Fifo where unsupported
    Immediate, // no vsync, may tear; falls back to Fifo where unsupported
};

struct SwapchainDesc {
    NativeWindow window;
    core::Extent2D extent;                     // in pixels (framebuffer size, not window points)
    std::span<const Format> preferred_formats; // first supported wins; empty: BGRA8Srgb, RGBA8Srgb, BGRA8Unorm
    PresentMode present_mode = PresentMode::Fifo;
    uint32_t frames_in_flight = 2; // CPU may record this many frames ahead of the GPU
    std::string_view debug_name = "Swapchain";
};

// The image to render this frame. Invalid when there is nothing to present to (e.g. a minimized window): skip the
// frame. The texture is in the Undefined state; transition it to ColorAttachment (or CopyDst), and to Present before
// Device::present().
struct SwapchainFrame {
    TextureHandle texture;
    uint32_t image_index = 0;

    [[nodiscard]] bool is_valid() const noexcept { return texture.is_valid(); }
};

// Presentable images for a window. Created by Device::create_swapchain(); must be destroyed before its Device.
// Move-only. Not thread-safe: acquire / present one frame at a time from one thread.
class BYTEE_RHI_API Swapchain {
private:
    detail::SwapchainImpl* impl_ = nullptr;

public:
    Swapchain() noexcept = default;
    ~Swapchain();

    Swapchain(Swapchain&& other) noexcept
        : impl_(std::exchange(other.impl_, nullptr)) {}
    Swapchain& operator=(Swapchain&& other) noexcept;
    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;

    [[nodiscard]] bool is_valid() const noexcept { return impl_ != nullptr; }
    [[nodiscard]] core::Extent2D extent() const noexcept;
    [[nodiscard]] Format format() const noexcept;
    [[nodiscard]] uint32_t image_count() const noexcept;

    // Records the new framebuffer size (e.g. from a window resize); the swapchain is recreated at the next acquire().
    // A zero size (minimized window) makes acquire() return invalid frames until the size is non-zero again.
    void resize(core::Extent2D extent) noexcept;

    // Waits until fewer than `frames_in_flight` frames are being rendered, then returns the next image.
    // Recreates the swapchain when needed (resize, out of date). Errors: DeviceLost, NotSupported (surface lost).
    [[nodiscard]] core::Result<SwapchainFrame> acquire();

private:
    friend class Device;
    friend detail::SwapchainImpl* get_impl(const Swapchain& swapchain) noexcept;

    explicit Swapchain(detail::SwapchainImpl* impl) noexcept
        : impl_(impl) {}
};

} // namespace bytee::render::rhi
