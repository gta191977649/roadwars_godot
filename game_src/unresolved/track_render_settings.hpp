#pragma once
#include "game_src/unresolved/resource_store.hpp"
#include <optional>

namespace roadwars {
// Portable presentation state; original class/source ownership remains unresolved.
struct TrackRenderSettings {
    float configured_view_distance{};
    float view_distance_scalar{};
    float near_distance{1.0f};
    Vec3 fog_color{};
    float ambient_light{99.0f / 255.0f};
    Vec3 directional_light{};
    bool use_sky{};
    std::string sky_path;
};
Result load_track_render_settings(ResourceStore& resources, std::string_view track,
                                  TrackRenderSettings& settings, std::string& error);

class ViewDistanceMap {
  public:
    Result load(ResourceStore& resources, std::string_view track, std::string& error);
    Result decode(std::span<const std::byte> bytes, std::string& error);
    float sample(Vec3 camera_position);
    float maximum_distance() const { return maximum_distance_; }
    std::size_t region_count() const { return regions_.size(); }

  private:
    struct Region {
        std::vector<std::array<Vec3, 3>> triangles;
        float distance{};
    };
    std::vector<Region> regions_;
    std::optional<std::size_t> cached_region_;
    float maximum_distance_{};
};

// Sky root translation replaces its serialized root position; children retain local positions.
Transform original_sky_transform(Vec3 camera_position, float far_distance);
} // namespace roadwars
