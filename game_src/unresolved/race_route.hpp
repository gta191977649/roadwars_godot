#pragma once
#include "game_src/unresolved/resource_store.hpp"
#include <array>
#include <optional>

namespace roadwars {
// Portable representations of track+7a44 and vehicle+441c fields. Original class names
// and source ownership remain unresolved; these are not native object layouts.
struct RaceRoutePoint {
    int id{};
    Vec3 left{}, right{}, center{}, lateral{};
    float half_width{}, optimal_offset{}, alternate_offset{}, right_bound{}, left_bound{};
    float speed_hint{}, cumulative_distance{};
    std::array<int, 3> flags{};
    int branch_kind{};
};
struct RaceRouteNode {
    RaceRoutePoint main;
    std::optional<RaceRoutePoint> branch;
};
struct RaceRouteProgress {
    int completed_laps{}, current_node{}, branch_recent{-1};
    bool on_branch{}, finished{};
    std::array<bool, 5> checkpoints{};
    float route_percent{}, previous_route_percent{};
};
class RaceRoute {
  public:
    Result load(ResourceStore&, std::string_view track, std::string& error, bool reverse = false);
    Result decode(std::span<const std::byte>, std::string& error);
    std::span<const RaceRouteNode> nodes() const { return nodes_; }
    const std::array<int, 5>& checkpoints() const { return checkpoints_; }
    float length() const { return length_; }
    int wrap(int index) const;
    const RaceRoutePoint& point(int index, bool branch = false) const;
    Vec3 sample(int base, int relative, bool on_branch, int branch_recent, int behavior,
                float& lateral_offset, bool& branch_latch) const;
    Vec3 respawn(int index, float fraction = 0, bool branch = false) const;
    Result initialize_progress(RaceRouteProgress&, Vec3 position, std::string& error) const;
    int initial_node(Vec3 position) const;
    Result update(RaceRouteProgress&, Vec3 previous, Vec3 current, int total_laps,
                  std::string& error) const;
    float ranking_progress(const RaceRouteProgress&) const;

  private:
    bool alternate_path(int base, bool on_branch, int recent, int behavior) const;
    void update_percentage(RaceRouteProgress&, Vec3 current) const;
    std::vector<RaceRouteNode> nodes_;
    std::array<int, 5> checkpoints_{};
    float length_{};
};
} // namespace roadwars
