#pragma once
#include "game_src/interfaces/host.hpp"
#include <array>
#include <functional>
#include <span>
#include <string>
#include <string_view>

namespace roadwars {
struct MeshObject;
struct WallRecord;
class ResourceStore;
struct OriginalWeaponKeys {
    int current{-1}, front{-1}, side{-1}, rear{-1}, nitrous{-1}, cycle{-1};
};
Result load_original_weapon_keys(ResourceStore&, OriginalWeaponKeys&, std::string& error);
// Portable field views of the vehicle used by 0041a1cd and descendants.
// Original C++ class/source names remain unresolved. Indices are never native pointers.
struct RaceAiCombatWeaponView {
    bool present{}, guided_kind_28{}, active_270{};
    int target_index{-1}; // Original weapon+08, resolved into the participant span.
    std::string_view name;
};
struct RaceAiCombatVehicleView {
    std::uint64_t chassis_identity{}; // Original vehicle+4494 identity comparison.
    float disabled_134{};
    Vec3 position{}, route_lateral{}; // Lap.bin main record+2c.
    int route_node{};
    RaceAiCombatWeaponView side_weapon;
    Vec3 route_sample{}; // 004185fe(current node, fraction 0, actual branch).
};
struct RaceAiCombatFrame {
    std::span<const RaceAiCombatVehicleView> vehicles;
    std::size_t self_index{};
    int route_node_count{};
    bool enabled_4de0{}, direct_fire_4aec{};
    // The exact 0041a7e7 reference: wheel cache[0] - wheel cache[2] + position.
    // This is intentionally not replaced with a guessed heading vector.
    Vec3 rear_reference{};
};
struct RaceAiCombatState {
    float side_delay_4ad4{}, rear_timer_0c{};
};
struct RaceWeaponInput {
    std::array<bool, 3> weapon_down{}, weapon_pressed{};
    bool current_down{}, current_pressed{}, nitrous_down{}, nitrous_pressed{};
    bool cycle_down{}, cycle_pressed{};
};
struct RaceWeaponInputState {
    std::array<bool, 6> previous_down{};
};
struct RaceWeaponInputServices {
    std::function<Result(std::size_t)> activate;
    std::function<Result()> nitrous;
    std::function<Result(int&)> cycle; // Updates the selected HUD item before activation.
};
struct RaceAiCombatServices {
    // Slots 0/1/2 are the original front/side/rear weapon slots.
    std::function<Result(std::size_t, RaceAiCombatWeaponView&)> weapon;
    std::function<Result(RaceAiCombatWeaponView&)> refresh_side_target;
    // 0041a1cd gathers walls around the player position with radius 3, then tests
    // its original three-unit heading+90 ray. The integration must supply that
    // recovered query, not a line of sight to the target vehicle.
    std::function<Result(bool&)> side_wall_blocked;
    // Called synchronously before the following AI random draw: activation may
    // itself consume the shared original RNG. A rejected shot is still an attempt.
    std::function<Result(std::size_t)> activate;
    std::function<std::uint32_t()> random;
};
struct RaceAiCombatTrace {
    std::array<unsigned, 3> activation_attempts{};
    int side_target{-1};
    std::string_view side_branch{"Disabled"};
};

Vec3 race_ai_rear_reference(Vec3 wheel_0, Vec3 wheel_2, Vec3 position);
float race_ai_combat_angle(Vec3 first, Vec3 second);
unsigned race_weapon_mount_count(const MeshObject&);
bool race_ai_side_wall_blocked(Vec3 position, float heading, std::span<const WallRecord>);
bool race_world_weapons_unlocked(float countdown, float race_time, int locked_seconds);
bool race_weapon_activation_allowed(float countdown, float race_time, int locked_seconds,
                                    float disabled_134 = 0);
// Supply the original reset draws at their actual points in the shared RNG stream.
// Other reset draws/callbacks occur between these two assignments.
void reset_race_ai_side_delay(RaceAiCombatState&, std::uint32_t random_value);
void reset_race_ai_rear_timer(RaceAiCombatState&, std::uint32_t random_value);
// Positive fixed simulation ticks replace the legacy held-seconds == 0 test
// with its equivalent edge. Explicit pressed pulses must be latched across
// zero-tick render frames by GameEntry and cleared after the first catch-up tick.
Result dispatch_race_weapon_input(const RaceWeaponInput&, RaceWeaponInputState&,
                                  std::array<bool, 3> weapon_present, int selected_hud_item,
                                  const RaceWeaponInputServices&, std::string& error);
bool race_missile_owner_wrong_way(Vec3 original_forward, Vec3 position, Vec3 next_main_center);
Result select_race_missile_target(std::span<const RaceAiCombatVehicleView>, std::size_t owner,
                                  int route_node_count, bool back_missile, bool owner_wrong_way,
                                  int& target, std::string& error);
Result advance_race_ai_combat(const RaceAiCombatFrame&, RaceAiCombatState&,
                              const RaceAiCombatServices&, float delta_seconds, RaceAiCombatTrace&,
                              std::string& error);
} // namespace roadwars
