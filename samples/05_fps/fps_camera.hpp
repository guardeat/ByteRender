#pragma once

// First-person camera: walks on the ground at eye height, looks around with yaw / pitch, jumps.

#include "sample_math.hpp"

#include <algorithm>
#include <cmath>

namespace bytee::samples {

class FpsCamera {
private:
    Vec3 position_{0.0f, 1.7f, 8.0f};
    float yaw_ = 0.0f;   // radians; 0 looks along -Z, positive turns right
    float pitch_ = 0.0f; // radians; positive looks up
    float vertical_speed_ = 0.0f;

public:
    static constexpr float k_eye_height = 1.7f;
    static constexpr float k_gravity = 9.81f;
    static constexpr float k_jump_speed = 4.5f;

    [[nodiscard]] Vec3 position() const noexcept { return position_; }

    [[nodiscard]] Vec3 forward() const noexcept {
        return {std::sin(yaw_) * std::cos(pitch_), std::sin(pitch_), -std::cos(yaw_) * std::cos(pitch_)};
    }

    [[nodiscard]] Mat4 view() const noexcept { return look_to(position_, forward()); }

    void look(float yaw_delta, float pitch_delta) noexcept {
        yaw_ += yaw_delta;
        pitch_ = std::clamp(pitch_ + pitch_delta, -1.5f, 1.5f); // stop just short of straight up / down
    }

    // Moves on the ground plane: `forward_amount` along the view direction, `right_amount` sideways (meters).
    void walk(float forward_amount, float right_amount) noexcept {
        const Vec3 ahead{std::sin(yaw_), 0.0f, -std::cos(yaw_)};
        const Vec3 right{std::cos(yaw_), 0.0f, std::sin(yaw_)};
        position_ += ahead * forward_amount + right * right_amount;
    }

    void jump() noexcept {
        if (is_on_ground()) {
            vertical_speed_ = k_jump_speed;
        }
    }

    void update(float seconds) noexcept {
        vertical_speed_ -= k_gravity * seconds;
        position_.y += vertical_speed_ * seconds;
        if (position_.y <= k_eye_height) {
            position_.y = k_eye_height;
            vertical_speed_ = 0.0f;
        }
    }

    [[nodiscard]] bool is_on_ground() const noexcept { return position_.y <= k_eye_height; }
};

} // namespace bytee::samples
