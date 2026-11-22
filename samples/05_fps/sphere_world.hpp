#pragma once

// Spheres thrown into the scene: gravity, bounces on the ground, rolling friction and sphere-sphere collisions
// (fixed time step, all pairs: fine for the sample's sphere count).

#include "sample_math.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace bytee::samples {

struct Sphere {
    Vec3 position;
    float radius = 0.5f;
    Vec3 velocity;
    Vec3 color; // linear RGB
};

class SphereWorld {
private:
    std::vector<Sphere> spheres_;
    uint32_t capacity_ = 0;
    uint32_t oldest_ = 0; // replaced next once full
    float time_debt_ = 0.0f;

public:
    static constexpr float k_time_step = 1.0f / 120.0f;

    explicit SphereWorld(uint32_t capacity);

    // Adds a sphere; once full, the oldest one is replaced.
    void spawn(const Sphere& sphere);
    // Advances the simulation by `seconds` in fixed steps.
    void update(float seconds);

    [[nodiscard]] std::span<const Sphere> spheres() const noexcept { return spheres_; }
    [[nodiscard]] uint32_t capacity() const noexcept { return capacity_; }

private:
    void step(float seconds);
    void collide_with_ground(Sphere& sphere, float seconds) const;
    static void collide(Sphere& a, Sphere& b);
};

} // namespace bytee::samples
