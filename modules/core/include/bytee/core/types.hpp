#pragma once

#include <cstdint>

namespace bytee::core {

struct Offset2D {
    int32_t x = 0;
    int32_t y = 0;

    friend constexpr bool operator==(const Offset2D&, const Offset2D&) noexcept = default;
};

struct Offset3D {
    int32_t x = 0;
    int32_t y = 0;
    int32_t z = 0;

    friend constexpr bool operator==(const Offset3D&, const Offset3D&) noexcept = default;
};

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

struct Rect2D {
    Offset2D offset;
    Extent2D extent;

    friend constexpr bool operator==(const Rect2D&, const Rect2D&) noexcept = default;
};

} // namespace bytee::core
