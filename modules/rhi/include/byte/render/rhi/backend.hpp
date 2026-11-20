#pragma once

#include <byte/render/rhi/export.hpp>

#include <cstdint>

namespace byte::render::rhi {

enum class BackendType : uint8_t {
    Vulkan,
};

// The backend selected at compile time (BYTE_RENDERER_RHI_BACKEND).
BYTE_RENDERER_RHI_API BackendType backend_type();
BYTE_RENDERER_RHI_API const char* backend_name();

} // namespace byte::render::rhi
