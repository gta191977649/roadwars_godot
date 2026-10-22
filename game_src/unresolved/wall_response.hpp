#pragma once
#include "game_src/unresolved/vehicle_dynamics.hpp"

namespace roadwars {
// Physics-only contact response from 0042e8f0; caller owns repeat-contact dispatch.
// Original audiovisual, damage and network side effects are outside this function.
void apply_wall_response(const VehicleParameters& parameters, VehicleState& state,
                         const WallContact& contact, float delta_seconds,
                         Vec3* response_force = nullptr);
} // namespace roadwars
