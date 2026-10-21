#include "game_src/unresolved/wheel_pose.hpp"
#include <cmath>
#include <stdexcept>
using namespace roadwars;
namespace {
void require(bool condition) {
    if (!condition)
        throw std::runtime_error("Wheel pose contract failed");
}
} // namespace
int main() {
    std::array<float, 4> spins{};
    const std::array<float, 4> grounded{};
    require(advance_wheel_spin(spins, 10, 0, 2, 0, grounded, .1f) == Result::ok);
    require(spins[0] == -180 && spins[3] == -180);
    require(advance_wheel_spin(spins, 10, 90, 2, 0, grounded, .1f) == Result::ok);
    require(spins[0] == -180);
    require(advance_wheel_spin(spins, 10, 180, 2, 1, grounded, .1f) == Result::ok);
    require(spins[0] == -180);
    std::array<float, 4> airborne{1, 0, 0, 0};
    require(advance_wheel_spin(spins, 10, 180, 2, 0, airborne, .1f) == Result::ok);
    require(spins[0] == -180 && std::abs(spins[1]) < .001f);
    require(advance_wheel_spin(spins, 10, 0, 2, 0, grounded, .1f, false) == Result::ok);
    require(spins[0] == -180);
    require(advance_wheel_spin(spins, 10, 0, 0, 0, grounded, .1f) == Result::invalid_argument);
    const auto left = original_wheel_transform(0, {1, 2, 3}, 0, 0, 1.4f);
    const auto right = original_wheel_transform(1, {1, 2, 3}, 0, 0, 1.4f);
    require(left.origin.x == 1 && left.origin.y == 2 && left.origin.z == 3);
    require(left.basis[0].x == 1.4f && left.basis[1].y == 1 && left.basis[2].z == 1);
    require(right.basis[0].x == -1.4f && right.basis[1].y == 1 && right.basis[2].z == -1);
    const auto front = original_wheel_transform(0, {}, 30, 0, 1);
    const auto rear = original_wheel_transform(2, {}, 30, 0, 1);
    require(front.basis[2].x > .49f && rear.basis[2].x == 0);
}
