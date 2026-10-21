#include "game_src/unresolved/contact_orientation.hpp"
#include "game_src/unresolved/vehicle_dynamics.hpp"
#include <cassert>
#include <cmath>
using namespace roadwars;
int main() {
    float angle = 0, target = 2, history = 0;
    update_body_spring(angle, target, history, 0.1f);
    assert(angle == 4.5f && target == 0 && history == 2); // Original overshoots.
    update_body_spring(angle, target, history, 0.2f);
    assert(angle == -4.5f && target == 1 && history == 2);
    angle = -0.5f;
    target = 0;
    history = 0.5f;
    update_body_spring(angle, target, history, 0.1f);
    assert(angle == 0 && target == 0 && history == 0);
    const auto t = original_vehicle_transform({1, 2, 3}, 90, 0, 0);
    assert(std::abs(t.basis[0].x - std::cos(90 * 0.017455555498600006f)) < 1e-6f);
    assert(t.basis[0].z < -0.99f && t.basis[2].x > 0.99f);
    std::array<Vec3, 4> wheels{{{-1, 0, 1}, {1, 0, 1}, {-1, 0, -1}, {1, 0, -1}}};
    std::array<GroundContact, 4> contacts;
    for (std::size_t i = 0; i < 4; ++i) {
        contacts[i].hit = true;
        contacts[i].point = wheels[i];
    }
    contacts[0].point.y = 1;
    contacts[1].point.y = 1;
    VehicleState v;
    v.pitch_rate = 3;
    v.roll_rate = 4;
    ContactOrientationState body;
    assert(update_contact_orientation(v, body, wheels, contacts, 0.01f) == Result::ok);
    assert(v.position.y == 0.5f && v.pitch_degrees > 26 && v.pitch_degrees < 27);
    assert(v.roll_degrees == 0 && v.pitch_rate == 0 && v.roll_rate == 0);
    v.air_time = {1, 1, 1, 1};
    v.pitch_degrees = 17;
    v.roll_degrees = -8;
    assert(update_contact_orientation(v, body, wheels, contacts, 0.01f) == Result::ok);
    assert(v.pitch_degrees == 17 && v.roll_degrees == -8);
    VehicleParameters parameters;
    parameters.wheel_ground_anchors = wheels;
    std::array<float, 4> heights{};
    v.velocity = {0, -10, 10};
    v.air_time = {0.1f, 0, 0, 0};
    contacts[0].normal = {0, 1, 0};
    for (std::size_t i = 0; i < 4; ++i)
        contacts[i].point = wheels[i];
    update_wheel_suspension(parameters, v, body, heights, wheels, contacts);
    assert(v.air_time[0] == 0 && body.pitch_target == 10 && body.roll_target == -10);
    assert(v.velocity.y > -10 && v.velocity.y < 0);
    v.velocity = {0, -10, 0};
    v.air_time[0] = 0.1f;
    update_wheel_suspension(parameters, v, body, heights, wheels, contacts);
    assert(v.velocity.y > -2.51f && v.velocity.y < -2.49f); // COS projection, not SIN.
    v.air_time[0] = 0.5f;
    contacts[0].point.y = -2;
    update_wheel_suspension(parameters, v, body, heights, wheels, contacts);
    assert(v.air_time[0] == 0.5f && heights[0] == -0.1f);
    contacts[0].point.y = -99999;
    assert(update_contact_orientation(v, body, wheels, contacts, 1) == Result::unavailable);
}
