#pragma once

#include <byte/core/version.hpp>
#include <byte/render/rhi/adapter.hpp>
#include <byte/render/rhi/export.hpp>

#include <span>
#include <string_view>
#include <utility>

namespace byte::render::rhi {

struct InstanceDesc {
    std::string_view app_name = "ByteRender Application";
    core::Version app_version;
    // Enables the backend's validation/debug layer if it is installed. Messages go to the logger.
    bool enable_validation = false;
};

// Root object of the RHI: connection to the graphics API and the list of available adapters.
// Created by InstanceFactory. Move-only; one live Instance per process.
class BYTE_RENDER_RHI_API Instance {
public:
    Instance() noexcept = default;
    ~Instance();

    Instance(Instance&& other) noexcept : m_impl(std::exchange(other.m_impl, nullptr)) {}
    Instance& operator=(Instance&& other) noexcept;
    Instance(const Instance&) = delete;
    Instance& operator=(const Instance&) = delete;

    [[nodiscard]] bool is_valid() const noexcept { return m_impl != nullptr; }
    [[nodiscard]] bool is_validation_enabled() const noexcept;
    [[nodiscard]] std::span<const AdapterInfo> adapters() const noexcept;

private:
    friend class InstanceFactory;
    struct Impl;

    explicit Instance(Impl* impl) noexcept : m_impl(impl) {}

    Impl* m_impl = nullptr;
};

} // namespace byte::render::rhi
