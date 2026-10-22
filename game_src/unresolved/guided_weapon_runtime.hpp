#pragma once
#include "game_src/unresolved/race_route.hpp"
#include "game_src/unresolved/weapon_runtime.hpp"
#include <functional>

namespace roadwars {
struct GuidedWeaponShape {
    float width{}, height{};
};
struct GuidedMissileState {
    Vec3 position{}, previous_position{}, velocity{};
    float heading{}, jam_remaining{};
    float flare_roll{};
    bool impacted{}, visible{};
};
struct GuidedWeaponTarget {
    Vec3 position{};
    int route_node{-1}; // Required for The Excalibur's preceding-ranked target.
};
struct GuidedWeaponServices {
    std::function<std::uint32_t()> random;
    // The original query samples at current missile position plus Y=5.
    std::function<Result(Vec3, float& ground_height)> ground;
};
// Portable state for ctor0044e820 and derived00434b90; original C++ names are unknown.
class GuidedWeaponRuntime {
  public:
    Result initialize(std::string_view name, WeaponParameters, GuidedWeaponShape,
                      std::string& error);
    Result launch(const WeaponFireRequest&, bool& launched, std::string& error,
                  int owner_route_node = -1, bool owner_on_branch = false);
    // Two phases preserve motion -> caller's evidenced collision/explosion -> expiry ordering.
    Result begin_update(float dt, std::optional<GuidedWeaponTarget>, const RaceRoute*,
                        const GuidedWeaponServices&, std::string& error);
    Result record_impact(std::size_t missile);
    Result finish_update(std::span<const std::size_t> live_trail_particles);
    Result jam(float seconds);
    void cancel() { active_ = false; }
    bool active() const { return active_; }
    bool ready() const { return initialized_ && ready_ && !active_; }
    float age() const { return age_; }
    int route_node() const { return route_node_; }
    bool on_branch() const { return branch_; }
    std::optional<Vec3> tracking_target() const { return tracking_target_; }
    std::span<const GuidedMissileState> missiles() const { return missiles_; }

  private:
    Result excalibur_target(const GuidedWeaponTarget&, const RaceRoute&, Vec3&);
    enum class Kind { guided, back, swarmers, excalibur };
    Kind kind_{};
    WeaponParameters parameters_;
    GuidedWeaponShape shape_;
    std::vector<GuidedMissileState> missiles_;
    Vec3 initial_direction_{};
    std::optional<Vec3> tracking_target_;
    float horizontal_speed_{}, vertical_speed_{}, age_{}, tracking_timer_{};
    int route_node_{-1};
    bool branch_{}, initialized_{}, ready_{true}, active_{}, update_pending_{};
};
} // namespace roadwars
