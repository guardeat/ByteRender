#pragma once

// Minimal 3D math for the samples (Bytee itself has no math library yet; see the roadmap backlog). Right-handed,
// Y-up, column vectors; matrices are column-major like GLSL, so they go into push constants unchanged.

#include <array>
#include <cmath>
#include <cstddef>

namespace bytee::samples {

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    friend constexpr Vec3 operator+(Vec3 a, Vec3 b) noexcept { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
    friend constexpr Vec3 operator-(Vec3 a, Vec3 b) noexcept { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
    friend constexpr Vec3 operator*(Vec3 v, float s) noexcept { return {v.x * s, v.y * s, v.z * s}; }
    friend constexpr Vec3 operator*(float s, Vec3 v) noexcept { return v * s; }
    constexpr Vec3& operator+=(Vec3 v) noexcept { return *this = *this + v; }
    constexpr Vec3& operator-=(Vec3 v) noexcept { return *this = *this - v; }
    constexpr Vec3& operator*=(float s) noexcept { return *this = *this * s; }
};

[[nodiscard]] constexpr float dot(Vec3 a, Vec3 b) noexcept {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

[[nodiscard]] constexpr Vec3 cross(Vec3 a, Vec3 b) noexcept {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

[[nodiscard]] inline float length(Vec3 v) noexcept {
    return std::sqrt(dot(v, v));
}

[[nodiscard]] inline Vec3 normalize(Vec3 v) noexcept {
    const float size = length(v);
    return size > 0.0f ? v * (1.0f / size) : Vec3{};
}

struct Mat4 {
    std::array<float, 16> m{}; // column-major: m[column * 4 + row]

    friend constexpr Mat4 operator*(const Mat4& a, const Mat4& b) noexcept {
        Mat4 result;
        for (std::size_t column = 0; column < 4; ++column) {
            for (std::size_t row = 0; row < 4; ++row) {
                float sum = 0.0f;
                for (std::size_t k = 0; k < 4; ++k) {
                    sum += a.m[k * 4 + row] * b.m[column * 4 + k];
                }
                result.m[column * 4 + row] = sum;
            }
        }
        return result;
    }
};

// View matrix of a camera at `eye` looking along `forward` (normalized; not parallel to `up`).
[[nodiscard]] inline Mat4 look_to(Vec3 eye, Vec3 forward, Vec3 up = {0.0f, 1.0f, 0.0f}) noexcept {
    const Vec3 right = normalize(cross(forward, up));
    const Vec3 camera_up = cross(right, forward);
    return {{right.x, camera_up.x, -forward.x, 0.0f, //
             right.y, camera_up.y, -forward.y, 0.0f, //
             right.z, camera_up.z, -forward.z, 0.0f, //
             -dot(right, eye), -dot(camera_up, eye), dot(forward, eye), 1.0f}};
}

// Perspective projection with reverse-Z and an infinite far plane (Bytee's convention: depth 1 at `near`, 0 at
// infinity, clear to 0, GREATER test). Y-up clip space: the RHI takes care of Vulkan's flipped Y.
[[nodiscard]] inline Mat4 perspective_reverse_z(float vertical_fov_radians, float aspect, float near) noexcept {
    const float focal = 1.0f / std::tan(vertical_fov_radians * 0.5f);
    return {{focal / aspect, 0.0f, 0.0f, 0.0f, //
             0.0f, focal, 0.0f, 0.0f,          //
             0.0f, 0.0f, 0.0f, -1.0f,          //
             0.0f, 0.0f, near, 0.0f}};
}

} // namespace bytee::samples
