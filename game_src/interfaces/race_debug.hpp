#pragma once
#include "game_src/interfaces/host.hpp"
#include <optional>
#include <string>

namespace roadwars {
// New read-only diagnostic contract. These types are not original object layouts.
enum class RaceDebugKind { route, checkpoint, collision, target, vehicle_state };
struct RaceDebugCamera {
    Vec3 eye{}, right{1, 0, 0}, up{0, 1, 0}, forward{0, 0, 1};
    float vertical_fov_degrees{50}, near_distance{0.1f};
};
struct RaceAiDebugState {
    int ai_index{}, state{}, substate{}, completed_laps{}, rank{};
    int route_node{-1}, target_node{-1};
    std::string character, state_name, substate_name;
    Vec3 position{}, target{};
    float speed{}, target_speed{}, steering{}, throttle{}, brake{}, recovery_time{};
};
struct RaceDebugLine {
    Vec3 from{}, to{};
    RaceDebugKind kind{RaceDebugKind::route};
    int ai_index{-1};
    bool branch{}, highlighted{};
};
struct RaceDebugLabel {
    Vec3 position{};
    std::string text;
    RaceDebugKind kind{RaceDebugKind::route};
    int ai_index{-1};
};
struct RaceEquipmentDebugState {
    struct Weapon {
        std::string name;
        int ammunition{};
        bool present{}, active{}, ready{}, pending{};
        float deployment_roll{};
        std::optional<float> elapsed;
        int target{-1}, query_target{-1};
    };
    int participant{};
    std::string character, tires, armor;
    std::array<std::string, 3> selected_weapons;
    std::vector<std::string> drivetrain;
    float internal_hp{}, maximum_internal_hp{};
    std::array<float, 4> armor_hp{};
    int nitrous_ammo{};
    std::array<Weapon, 3> weapons;
    float nitrous_timer{}, dying_timer{};
    float ai_side_delay{}, ai_rear_timer{};
};
struct RaceTurretDebugState {
    std::string name;
    Vec3 position{};
    float heading{}, pitch{};
    int target{-1};
};
struct RaceDebugSnapshot {
    std::string track, phase, player_character;
    std::vector<std::string> available_tracks;
    int selected_track{-1};
    int total_laps{}, participant_count{};
    std::uint64_t simulation_ticks{};
    double race_time{};
    RaceDebugCamera camera;
    std::vector<RaceAiDebugState> ai;
    std::vector<RaceDebugLine> lines;
    std::vector<RaceDebugLabel> labels;
    std::vector<RaceEquipmentDebugState> equipment;
    std::string combat_status;
    std::vector<RaceTurretDebugState> turrets;
    std::uint64_t turret_launched{}, turret_impacts{};
    int selected_hud_item{}, diagnostic_weapon_lock_seconds{};
};
struct RaceDebugOptions {
    std::string requested_track; // Deferred diagnostic UI command; consumed after drawing.
    bool show_ai_states{}, show_route{}, show_checkpoints{}, show_collision_shapes{};
    bool auto_drive{};         // Explicit diagnostic control, not a visualization toggle.
    int selected_ai{-1};       // -1 follows the player; nonnegative values follow an AI.
    float route_distance{200}; // Diagnostic visibility range in original world metres.
};
} // namespace roadwars
