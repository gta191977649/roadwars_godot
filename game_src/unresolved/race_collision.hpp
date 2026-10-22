#pragma once
#include "game_src/interfaces/race_debug.hpp"
#include "game_src/unresolved/vehicle_dynamics.hpp"

namespace roadwars {
// Portable records of the original vehicle-owned pair collision phase.
// Original source/class names remain unconfirmed.
struct VehiclePairContact {
    bool hit{}, fallback{}, sound_on_b{};
    Vec3 point{}, normal{};
    float impact_strength{}, removed_seconds{};
    unsigned rollback_iterations{};
};
std::array<Vec3, 4> vehicle_pair_footprint(const VehicleParameters&, const VehicleState&);
VehiclePairContact solve_vehicle_collision(const VehicleParameters& a_parameters, VehicleState& a,
                                           const VehicleState& previous_a,
                                           const VehicleParameters& b_parameters, VehicleState& b,
                                           const VehicleState& previous_b, float delta_seconds);
bool resolve_vehicle_collision(VehicleDynamics& a, VehicleDynamics& b,
                               const VehicleState& previous_a, const VehicleState& previous_b,
                               float delta_seconds, std::vector<RaceDebugLine>* debug = nullptr,
                               float* remaining_seconds = nullptr);
} // namespace roadwars
