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
Vec3 vehicle_contact_velocity(const VehicleState&, Vec3 world_point);
VehiclePairContact
solve_vehicle_collision(const VehicleParameters& a_parameters, VehicleState& a,
                        const VehicleState& previous_a, const VehicleParameters& b_parameters,
                        VehicleState& b, const VehicleState& previous_b, float delta_seconds,
                        const std::function<void(Vec3, Vec3, bool)>& damage = {},
                        const std::function<void(Vec3, Vec3, bool)>& effect = {});
Result apply_vehicle_collision_damage(VehicleDynamics&, Vec3 force, Vec3 point, bool invulnerable,
                                      bool player, int difficulty);
struct VehiclePairDamageContext {
    bool a_invulnerable{}, b_invulnerable{}, a_player{}, b_player{};
    int difficulty{3};
    Result* damage_visual_result{};
};
bool resolve_vehicle_collision(VehicleDynamics& a, VehicleDynamics& b,
                               const VehicleState& previous_a, const VehicleState& previous_b,
                               float delta_seconds, std::vector<RaceDebugLine>* debug = nullptr,
                               float* remaining_seconds = nullptr,
                               const VehiclePairDamageContext* damage_context = nullptr);
} // namespace roadwars
