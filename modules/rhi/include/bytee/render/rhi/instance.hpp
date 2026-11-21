#pragma once

#include <bytee/core/compiler.hpp>
#include <bytee/core/result.hpp>
#include <bytee/core/version.hpp>
#include <bytee/render/rhi/adapter.hpp>
#include <bytee/render/rhi/device.hpp>
#include <bytee/render/rhi/export.hpp>

#include <span>
#include <string_view>
#include <utility>

namespace bytee::render::rhi {

struct InstanceDesc {
    std::string_view app_name = "Bytee Application";
    core::Version app_version;
    // Enables the graphics API's validation layer if it is installed (messages go to the logger). Debug tool:
    // it slows everything down considerably.
    bool enable_validation = false;
};

// Root of the RHI: the connection to the graphics API and the list of adapters (GPUs).
// Created by InstanceFactory. Move-only. Only one Instance may be alive per process (architecture §10).
// Every Device created from it must be destroyed first.
class BYTEE_RHI_API Instance {
public:
    struct BYTEE_HIDDEN Impl; // backend-defined, opaque

private:
    Impl* impl_ = nullptr;

public:
    Instance() noexcept = default;
    ~Instance();

    Instance(Instance&& other) noexcept
        : impl_(std::exchange(other.impl_, nullptr)) {}
    Instance& operator=(Instance&& other) noexcept;
    Instance(const Instance&) = delete;
    Instance& operator=(const Instance&) = delete;

    [[nodiscard]] bool is_valid() const noexcept { return impl_ != nullptr; }
    [[nodiscard]] bool is_validation_enabled() const noexcept;
    // True if window-system presentation is available (swapchains can be created on supporting devices).
    [[nodiscard]] bool is_presentation_supported() const noexcept;
    [[nodiscard]] std::span<const AdapterInfo> adapters() const noexcept;

    // Fails with ErrorCode::InvalidArgument for an out-of-range adapter index and ErrorCode::NotSupported if the
    // requested adapter (or, without an index, every adapter) does not meet Bytee's requirements.
    [[nodiscard]] core::Result<Device> create_device(const DeviceDesc& desc);

private:
    friend class InstanceFactory;
    // Backend access to the implementation (defined by the backend).
    friend Impl* get_impl(const Instance& instance) noexcept;

    explicit Instance(Impl* impl) noexcept
        : impl_(impl) {}
};

} // namespace bytee::render::rhi
