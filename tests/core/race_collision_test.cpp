#include "game_src/unresolved/race_collision.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
using namespace roadwars;
namespace {
VehicleParameters parameters() {
    VehicleParameters result;
    result.length = 4;
    result.width = 2;
    result.height = 1.5f;
    result.mass_kg = 1000;
    result.inverse_mass = .001f;
    result.yaw_inertia = 1500;
    return result;
}
bool finite(const VehicleState& state) {
    return std::isfinite(state.position.x) && std::isfinite(state.position.y) &&
           std::isfinite(state.position.z) && std::isfinite(state.velocity.x) &&
           std::isfinite(state.velocity.y) && std::isfinite(state.velocity.z) &&
           std::isfinite(state.heading_degrees) && std::isfinite(state.yaw_rate);
}
} // namespace
int main() {
    //0042ba96: angular contact motion grows linearly with the complete 3D radius,
    // including the native XZ intersection's y0 versus an elevated chassis.
    VehicleState elevated;
    elevated.position = {10, 15, -20};
    elevated.velocity = {1, 2, 3};
    elevated.yaw_rate = 90;
    const auto contact_a = vehicle_contact_velocity(elevated, {11, 0, -18});
    const auto contact_b = vehicle_contact_velocity(elevated, {12, -15, -16});
    const auto magnitude = [](Vec3 v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); };
    const Vec3 delta_a{contact_a.x - 1, contact_a.y - 2, contact_a.z - 3};
    const Vec3 delta_b{contact_b.x - 1, contact_b.y - 2, contact_b.z - 3};
    const auto radius = std::sqrt(1.f + 225.f + 4.f);
    assert(std::abs(magnitude(delta_a) - radius * 90 * .017455555498600006f) < .0001f);
    assert(std::abs(delta_b.x - 2 * delta_a.x) < .0001f &&
           std::abs(delta_b.y - 2 * delta_a.y) < .0001f &&
           std::abs(delta_b.z - 2 * delta_a.z) < .0001f);
    const auto at_origin = vehicle_contact_velocity(elevated, elevated.position);
    assert(at_origin.x == 1 && at_origin.y == 2 && at_origin.z == 3);
    const auto p = parameters();
    VehicleState a, b;
    auto corners = vehicle_pair_footprint(p, a);
    assert(corners[0].x == -1 && corners[0].z == 2);
    assert(corners[2].x == 1 && corners[2].z == -2);
    a.heading_degrees = 90;
    corners = vehicle_pair_footprint(p, a);
    assert(std::abs(corners[0].x - 2) < .001f && std::abs(corners[0].z - 1) < .001f);
    a = {};
    b.position.z = 20;
    const auto distant = b;
    assert(!solve_vehicle_collision(p, a, a, p, b, b, 1.0f / 60).hit);
    assert(b.position.z == distant.position.z);

    // A straight rear impact transfers equal/opposite original .65 impulse.
    VehicleState old_a, old_b;
    old_b.position.z = 4.1f;
    old_b.position.x = .25f;
    b = old_b;
    a = old_a;
    a.position.z = .2f;
    a.velocity.z = 12;
    const auto contact = solve_vehicle_collision(p, a, old_a, p, b, old_b, 1.0f / 60);
    assert(contact.hit && !contact.fallback && contact.rollback_iterations <= 20);
    assert(contact.impact_strength > 0 && contact.removed_seconds > 0);
    assert(a.velocity.z < 12 && b.velocity.z > 0);
    assert(std::abs(a.velocity.z + b.velocity.z - 12) < .001f);
    assert(contact.normal.z < 0 && finite(a) && finite(b));

    // A vertical separation rejects an overpass pair before XZ narrow phase.
    a = {};
    b = {};
    a.position.z = .2f;
    b.position = {0, 10, 4};
    assert(!solve_vehicle_collision(p, a, old_a, p, b, old_b, 1.0f / 60).hit);
    assert(!solve_vehicle_collision(p, a, old_a, p, b, old_b, -1).hit);
    assert(
        !solve_vehicle_collision(p, a, old_a, p, b, old_b, std::numeric_limits<float>::quiet_NaN())
             .hit);

    // Already-crossing rectangles exhaust the original bounded rollback path.
    a = {};
    b = {};
    b.position = {.7f, 0, 1};
    const auto fallback = solve_vehicle_collision(p, a, a, p, b, b, 1.0f / 60);
    assert(fallback.hit && fallback.fallback && fallback.rollback_iterations == 20);
    assert(finite(a) && finite(b));

    // Angular contact motion and heading wrap remain finite and deterministic.
    old_a.heading_degrees = 359.5f;
    old_b.position = {.8f, 0, 4};
    a = old_a;
    a.heading_degrees = .5f;
    a.position.z = .5f;
    a.velocity.z = 8;
    a.yaw_rate = 15;
    b = old_b;
    const auto copy_a = a, copy_b = b;
    const auto turn = solve_vehicle_collision(p, a, old_a, p, b, old_b, 1.0f / 60);
    auto again_a = copy_a, again_b = copy_b;
    const auto again = solve_vehicle_collision(p, again_a, old_a, p, again_b, old_b, 1.0f / 60);
    assert(turn.hit && again.hit && finite(a) && finite(b));
    assert(a.position.x == again_a.position.x && a.position.z == again_a.position.z &&
           a.yaw_rate == again_a.yaw_rate && b.velocity.z == again_b.velocity.z);
    std::cout << "RACE_COLLISION_CONTRACT_OK\n";
}
