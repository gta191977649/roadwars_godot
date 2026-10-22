#pragma once
#include "game_src/unresolved/track_collision.hpp"
#include <functional>
#include <optional>
#include <span>
#include <string>

namespace roadwars {
// Portable representations of the distinct original rear instance objects.
// These names do not establish original C++ class names or source paths.
enum class RearInstanceKind { grenade, land_mine, reactive_mine, arachnoid };
struct RearInstanceParameters {
    float hitpoints{}, acceleration{}, ammunition_mass_kg{};
};
struct RearWeaponInstance {
    RearInstanceKind kind{};
    Vec3 position{}, direction{};
    float heading{}, age{};
    bool active{}, callback_running{};
    float left_leg_heading{}, right_leg_heading{};
    bool leg_phase{};
};
struct RearInstanceTarget {
    std::size_t participant{};
    std::uint64_t chassis_identity{};
    Vec3 position{};
    std::array<Vec3, 4> footprint{};
    std::array<float, 4> air_time{};
};
struct RearInstanceImpact {
    std::size_t participant{};
    float internal_damage{};
    Vec3 force{}, application{};
    // Consume with impulse scalar1.0, braking=false and tilt fixed_radius=false.
    // Grenade additionally uses yaw fixed_lever=-1; these are not dt-scaled forces.
    bool apply_yaw{};
};
struct RearInstanceOutput {
    bool exploded{}, removed{}, extra_reactive_effect{};
    Vec3 explosion_position{};
    std::vector<RearInstanceImpact> impacts;
};
struct RearInstanceWorld {
    // Exact sampled original surface, with no invented vehicle-physics substitute.
    std::function<Result(Vec3, GroundContact&)> ground;
    // Arachnoid query004069bb: current/current, radius1, height check enabled.
    std::function<Result(Vec3, std::vector<WallRecord>&)> arachnoid_walls;
    // Empty optional explicitly stops unavailable/exhausted work; no guessed heading fallback.
    std::function<std::optional<std::uint32_t>()> random;
};
Result launch_rear_weapon_instance(RearWeaponInstance&, Vec3 original_mount_position,
                                   float original_owner_heading, bool& launched,
                                   std::string& error);
Result advance_rear_weapon_instance(RearWeaponInstance&, const RearInstanceParameters&,
                                    std::uint64_t owner_identity,
                                    std::span<const RearInstanceTarget>, const RearInstanceWorld&,
                                    float dt, RearInstanceOutput&, std::string& error);
void stop_rear_weapon_instance(RearWeaponInstance&);
} // namespace roadwars
