#pragma once

#include <cstdint>

namespace byte::core {

struct Extent2D {
    uint32_t width = 0;
    uint32_t height = 0;

    friend constexpr bool operator==(const Extent2D&, const Extent2D&) noexcept = default;
};

struct Extent3D {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t depth = 1;

    friend constexpr bool operator==(const Extent3D&, const Extent3D&) noexcept = default;
};

} // namespace byte::core
