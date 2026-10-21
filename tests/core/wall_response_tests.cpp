#include "game_src/unresolved/wall_response.hpp"
#include <cmath>
#include <iostream>
using namespace roadwars;
int main() {
    VehicleParameters parameters;
    parameters.mass_kg = 1000;
    parameters.inverse_mass = 0.001f;
    parameters.yaw_inertia = 1000;
    WallContact contact;
    contact.hit = true;
    contact.normal = {-1, 0, 0};
    contact.removed_fraction = 0.5f;
    VehicleState state;
    state.velocity = {10, 0, 0};
    state.cached_speed = 10;
    apply_wall_response(parameters, state, contact, 0.1f);
    if (std::abs(state.velocity.x) > 0.001f || std::abs(state.position.x) > 0.001f)
        return 1;
    state = {};
    state.velocity = {10, 0, 10};
    state.cached_speed = std::sqrt(200.0f);
    apply_wall_response(parameters, state, contact, 0.1f);
    std::cout << "Glancing velocity=" << state.velocity.x << ',' << state.velocity.z
              << " position=" << state.position.x << ',' << state.position.z << '\n';
    if (std::abs(state.velocity.x + 2.92893f) > 0.005f ||
        std::abs(state.velocity.z - 2.92893f) > 0.005f)
        return 2;
    if (std::abs(state.position.x - state.velocity.x * 0.05f) > 0.0001f)
        return 3;
    state = {};
    state.velocity = {10, 0, 10};
    state.cached_speed = std::sqrt(50.0f);
    apply_wall_response(parameters, state, contact, 0.1f);
    if (std::abs(state.velocity.x) > 0.005f || std::abs(state.velocity.z - 2.92893f) > 0.005f)
        return 7; // Original cached speed can differ from post-landing velocity magnitude.
    state = {};
    apply_wall_response(parameters, state, contact, 0.1f);
    if (std::abs(state.velocity.x + 3) > 0.0001f || std::abs(state.position.x + 0.15f) > 0.0001f)
        return 4;
    state = {};
    state.velocity = {-10, 0, 0};
    apply_wall_response(parameters, state, contact, 0.1f);
    if (std::abs(state.velocity.x + 13) > 0.0001f)
        return 5;
    contact.hit = false;
    contact.restore_previous = true;
    contact.position = {1, 2, 3};
    contact.heading_degrees = 45;
    apply_wall_response(parameters, state, contact, 0.1f);
    if (state.velocity.x != 0 || state.position.z != 3 || state.heading_degrees != 45)
        return 6;
    return 0;
}
