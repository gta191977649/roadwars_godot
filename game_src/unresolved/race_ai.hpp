#pragma once
#include "game_src/unresolved/race_route.hpp"
#include "game_src/unresolved/vehicle_dynamics.hpp"
#include <span>
#include <string_view>

namespace roadwars {
// Portable names for fields of the vehicle object used by 0041aa67. Original
// class/source identity is unresolved. This is not the original native layout.
struct RaceAiObstacle {
    Vec3 position{}, velocity{};
    std::array<Vec3, 4> footprint{};
    bool active{true};
};
struct RaceAiState {
    int behavior{1}; // Original vehicle+4bac, not a newly invented behavior enum.
    float recovery_timer{}, wall_clear_time{}, recovery_accumulation{};
    float desired_offset{}, lateral_offset{}, offset_cooldown{};
    bool moving_backward{}, front{}, rear{}, left{}, right{};
    float front_strength{}, rear_strength{}, left_strength{}, right_strength{};
    float front_speed{};
};
struct RaceAiOutput {
    float engine_throttle{}, brake_amount{}, steering_degrees{};
    Vec3 target{};
    float target_speed{}, heading_error{};
    int target_node{};
    bool relocate{};
    Vec3 relocation{};
    std::string_view branch{"Normal"}; // Diagnostic branch name, not an original enum.
};
float race_ai_heading_delta(float heading, Vec3 from, Vec3 to);
float race_ai_corner_speed(float distance, float degrees, float longitudinal_grip);
float race_ai_braking_speed(float distance, float terminal_speed, float longitudinal_grip);
int race_ai_behavior(float health, float maximum_health, bool front, bool rear);
class RaceAi {
  public:
    Result update(const RaceRoute&, RaceRouteProgress&, const VehicleState&,
                  const VehicleParameters&, std::span<const RaceAiObstacle>,
                  float longitudinal_grip, float race_time, bool refresh_obstacles,
                  float delta_seconds, std::string& error, float internal_hp = 1,
                  float maximum_internal_hp = 1);
    void reset() {
        state_ = {};
        output_ = {};
    }
    // Called after original wall handling; iterations count actual contacts.
    void wall_update(unsigned iterations, float engine_throttle, float delta_seconds);
    const RaceAiState& state() const { return state_; }
    const RaceAiOutput& output() const { return output_; }

  private:
    void scan(const VehicleState&, std::span<const RaceAiObstacle>);
    void advance_offset(const RaceRoute&, const RaceRouteProgress&, float delta_seconds);
    RaceAiState state_;
    RaceAiOutput output_;
};
} // namespace roadwars
