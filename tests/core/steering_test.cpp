#include "game_src/unresolved/contact_orientation.hpp"
#include "game_src/unresolved/vehicle_input.hpp"
#include <cmath>
#include <stdexcept>

using namespace roadwars;

namespace {
void require(bool condition) {
    if (!condition)
        throw std::runtime_error("Original steering sign contract failed");
}
} // namespace

int main() {
    // 004831c3 binds RIGHT to +0x3e8 and LEFT to +0x3ec. 00424d3e
    // stores +1 and -1 respectively, so a presentation fix must not invert these.
    constexpr float kMaximum = 35.0f;
    constexpr float kDelta = 1.0f / 60.0f;
    const float right = advance_keyboard_steering(0, kMaximum, 1, kDelta);
    const float left = advance_keyboard_steering(0, kMaximum, -1, kDelta);
    require(std::abs(right - 2.3333333f) < 0.00001f);
    require(left == -right);
    require(advance_keyboard_steering(right, kMaximum, 0, kDelta) == 0);
    require(advance_keyboard_steering(left, kMaximum, 0, kDelta) == 0);

    float sustained_right = 0, sustained_left = 0;
    for (int frame = 0; frame < 120; ++frame) {
        sustained_right = advance_keyboard_steering(sustained_right, kMaximum, 1, kDelta);
        sustained_left = advance_keyboard_steering(sustained_left, kMaximum, -1, kDelta);
    }
    require(sustained_right == kMaximum && sustained_left == -kMaximum);

    // 004fdde7's yaw matrix has row 2=(sin(h),0,cos(h)). A positive
    // heading rotates original forward +Z toward original right +X.
    const Vec3 right_forward = original_vehicle_transform({}, 45, 0, 0).basis[2];
    const Vec3 left_forward = original_vehicle_transform({}, -45, 0, 0).basis[2];
    require(right_forward.x > 0.7f && right_forward.z > 0.7f);
    require(left_forward.x < -0.7f && left_forward.z > 0.7f);
    require(std::abs(right_forward.x + left_forward.x) < 0.00001f);
    require(right_forward.z == left_forward.z);
}
