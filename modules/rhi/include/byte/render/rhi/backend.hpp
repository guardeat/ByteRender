#pragma once

#include <byte/render/rhi/export.hpp>

#include <cstdint>
#include <string_view>

namespace byte::render::rhi {

enum class BackendType : uint8_t {
    Vulkan,
};

// The backend compiled into this build (BYTE_RENDER_RHI_BACKEND).
[[nodiscard]] BYTE_RENDER_RHI_API BackendType backend_type() noexcept;
[[nodiscard]] BYTE_RENDER_RHI_API std::string_view backend_name() noexcept;

} // namespace byte::render::rhi
