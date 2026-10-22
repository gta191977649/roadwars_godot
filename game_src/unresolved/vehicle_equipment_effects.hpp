#pragma once
#include "game_src/unresolved/resource_store.hpp"
#include <array>
#include <functional>
#include <span>
#include <string>
#include <utility>

namespace roadwars {
// Portable representation of vehicle-owned fields and its separately allocated nitrous object.
// Original C++ class names and source paths are not established.
struct VehicleEquipmentEffects {
    float internal_hp{}, maximum_internal_hp{};
    std::array<float, 4> base_armor_hp{}, maximum_armor_hp{}, armor_hp{};
    std::array<std::int32_t, 7> engine_upgrades{};
    std::string front_weapon;
    std::int32_t nitrous_ammo{};
    float nitrous_timer{}, nitrous_duration_scalar{1};
    float internal_repair_per_second{}, dying_timer{};
    float traction_multiplier{1};
};
struct VehicleDamagePermission {
    bool route_invulnerable{}, dying{}, local_authority{true};
};
struct NitrousStep {
    Vec3 velocity_delta{};
    bool emitting{}, stopped{};
};
Result drivetrain_upgrade(std::string_view category, std::string_view quality,
                          std::int32_t& output);
Result rescale_vehicle_armor(VehicleEquipmentEffects&, float hp_scale);
Result
load_vehicle_equipment_effects(ResourceStore&, std::string_view character, std::string_view vehicle,
                               std::string_view armor_type, std::string_view armor_quality,
                               std::span<const std::pair<std::string, std::string>> drivetrain,
                               std::string_view front_weapon, VehicleEquipmentEffects&,
                               std::string& error);
Result damage_vehicle_internal(VehicleEquipmentEffects&, float damage,
                               const VehicleDamagePermission& = {});
Result damage_vehicle_armor(VehicleEquipmentEffects&, std::size_t index, float damage,
                            const VehicleDamagePermission& = {});
// Contact sides are the original collision/weapon convention, not armor-array indices.
Result damage_vehicle_contact(VehicleEquipmentEffects&, std::size_t contact_side, float damage,
                              const VehicleDamagePermission& = {});
bool activate_vehicle_nitrous(VehicleEquipmentEffects&);
Result advance_vehicle_nitrous(VehicleEquipmentEffects&, float delta, Vec3 vehicle_forward,
                               std::size_t live_particles, NitrousStep&);
// Apply the velocity delta and advance the actual emitter before returning its live count.
Result advance_vehicle_nitrous(VehicleEquipmentEffects&, float delta, Vec3 vehicle_forward,
                               const std::function<Result(const NitrousStep&, std::size_t&)>&,
                               NitrousStep&);
void stop_vehicle_nitrous(VehicleEquipmentEffects&);
Result advance_vehicle_health(VehicleEquipmentEffects&, float delta_seconds);
} // namespace roadwars
