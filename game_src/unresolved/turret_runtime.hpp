#pragma once
#include "game_src/interfaces/host.hpp"
#include <optional>
#include <string>

namespace roadwars {
// Portable names for the object allocated at 004c7c80; original C++ identity is unknown.
struct TurretParameters {
    std::string weapon;
    float initial_yaw{}, left_limit{}, right_limit{};
    bool unrestricted_yaw{};
    float base_speed{60}, added_speed{}, fire_interval{};
    float ammunition_acceleration{}, barrel_length{}, reference_heading{};
    float maximum_range() const {
        return ammunition_acceleration * 2 + (base_speed + added_speed) * 2;
    }
};
Result parse_turret_parameters(std::string_view name, TurretParameters& output);
struct TurretTarget {
    std::size_t id{};
    Vec3 position{}, velocity{};
    float inactive_timer{};
    bool invulnerable{}, not_a_turret_target{}, computer_controlled{};
    int difficulty{3};
};
struct TurretFireEvent {
    std::size_t target{};
    Vec3 origin{}, predicted_target{};
    float heading{}, pitch{}, speed{};
};
class TurretRuntime {
  public:
    Result initialize(TurretParameters, Vec3 origin, float initial_pitch = 0);
    void reset();
    Result advance(float dt, std::span<const TurretTarget> targets, bool guns_enabled,
                   std::optional<TurretFireEvent>& fired);
    std::optional<std::size_t> target() const { return target_; }
    Vec3 predicted_target() const { return predicted_; }
    float heading() const { return yaw_; }
    float pitch() const { return pitch_; }
    float target_heading() const { return desired_yaw_; }
    float target_pitch() const { return desired_pitch_; }
    std::size_t velocity_samples() const { return samples_; }
    bool aligned() const;
    const TurretParameters& parameters() const { return parameters_; }

  private:
    const TurretTarget* find_target(std::span<const TurretTarget>) const;
    void select_target(std::span<const TurretTarget>);
    Vec3 predict(const TurretTarget&);
    TurretParameters parameters_;
    Vec3 origin_{}, predicted_{}, previous_velocity_{}, average_delta_{};
    std::array<Vec3, 5> velocity_deltas_{};
    std::size_t samples_{};
    std::optional<std::size_t> target_;
    float scan_timer_{}, fire_timer_{}, yaw_{}, pitch_{}, initial_pitch_{};
    float desired_yaw_{}, desired_pitch_{};
    bool initialized_{};
};
} // namespace roadwars
