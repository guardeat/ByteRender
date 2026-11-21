#pragma once

#include <bytee/render/rhi/export.hpp>

#include <cstdint>
#include <string_view>

namespace bytee::render::rhi {

enum class BackendType : uint8_t {
    Vulkan,
};

// The backend compiled into this build (BYTEE_RHI_BACKEND).
[[nodiscard]] BYTEE_RHI_API BackendType backend_type() noexcept;
[[nodiscard]] BYTEE_RHI_API std::string_view backend_name() noexcept;

} // namespace bytee::render::rhi
