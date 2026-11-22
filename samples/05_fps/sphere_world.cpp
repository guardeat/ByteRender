#include "sphere_world.hpp"

#include <algorithm>
#include <cmath>

namespace bytee::samples {

namespace {

constexpr float k_gravity = 9.81f;
constexpr float k_restitution = 0.5f;      // bounciness of sphere-sphere and ground contacts
constexpr float k_rolling_friction = 1.2f; // horizontal slowdown on the ground, per second
constexpr int k_max_steps_per_update = 8;  // after a long stall, drop time instead of spiraling

} // namespace

SphereWorld::SphereWorld(uint32_t capacity)
    : capacity_(capacity) {
    spheres_.reserve(capacity);
}

void SphereWorld::spawn(const Sphere& sphere) {
    if (spheres_.size() < capacity_) {
        spheres_.push_back(sphere);
        return;
    }
    spheres_[oldest_] = sphere;
    oldest_ = (oldest_ + 1) % capacity_;
}

void SphereWorld::update(float seconds) {
    time_debt_ = std::min(time_debt_ + seconds, k_time_step * k_max_steps_per_update);
    while (time_debt_ >= k_time_step) {
        step(k_time_step);
        time_debt_ -= k_time_step;
    }
}

void SphereWorld::step(float seconds) {
    for (Sphere& sphere : spheres_) {
        sphere.velocity.y -= k_gravity * seconds;
        sphere.position += sphere.velocity * seconds;
        collide_with_ground(sphere, seconds);
    }
    for (std::size_t i = 0; i < spheres_.size(); ++i) {
        for (std::size_t j = i + 1; j < spheres_.size(); ++j) {
            collide(spheres_[i], spheres_[j]);
        }
    }
}

void SphereWorld::collide_with_ground(Sphere& sphere, float seconds) const {
    if (sphere.position.y >= sphere.radius) {
        return;
    }
    sphere.position.y = sphere.radius;
    if (sphere.velocity.y < 0.0f) {
        sphere.velocity.y = -sphere.velocity.y * k_restitution;
        if (sphere.velocity.y < 0.3f) {
            sphere.velocity.y = 0.0f; // settle instead of micro-bouncing
        }
    }
    const float slowdown = std::max(0.0f, 1.0f - k_rolling_friction * seconds);
    sphere.velocity.x *= slowdown;
    sphere.velocity.z *= slowdown;
}

// Pushes overlapping spheres apart and exchanges momentum along the contact normal (mass grows with volume).
void SphereWorld::collide(Sphere& a, Sphere& b) {
    const Vec3 offset = b.position - a.position;
    const float reach = a.radius + b.radius;
    const float distance_squared = dot(offset, offset);
    if (distance_squared >= reach * reach || distance_squared < 1e-12f) {
        return;
    }
    const float distance = std::sqrt(distance_squared);
    const Vec3 normal = offset * (1.0f / distance);
    const float inverse_mass_a = 1.0f / (a.radius * a.radius * a.radius);
    const float inverse_mass_b = 1.0f / (b.radius * b.radius * b.radius);
    const float inverse_mass_sum = inverse_mass_a + inverse_mass_b;

    const float overlap = reach - distance;
    a.position -= normal * (overlap * inverse_mass_a / inverse_mass_sum);
    b.position += normal * (overlap * inverse_mass_b / inverse_mass_sum);

    const float approach_speed = dot(b.velocity - a.velocity, normal);
    if (approach_speed < 0.0f) {
        const float impulse = -(1.0f + k_restitution) * approach_speed / inverse_mass_sum;
        a.velocity -= normal * (impulse * inverse_mass_a);
        b.velocity += normal * (impulse * inverse_mass_b);
    }
}

} // namespace bytee::samples
