#pragma once
#include "game_src/unresolved/mesh_scene.hpp"
#include "game_src/unresolved/resource_store.hpp"

namespace roadwars {
struct GroundContact {
    bool hit{};
    Vec3 point{0, -99999.0f, 0};
    Vec3 normal{};
    std::uint32_t material_kind{};
    float longitudinal_grip{}, lateral_grip{}, rolling_resistance{};
    std::uint32_t particle_dust{}, particle_color_argb{}, skid_color_argb{};
    std::string skid_sound_path;
};
struct TrackSpawn {
    Vec3 position{};
    float heading_degrees{};
};
struct WallRecord {
    Vec3 lower{}, upper{}, normal{};
    float parameter{};
};
struct CollisionBody {
    Vec3 position{};
    float heading_degrees{}, diagonal_radius{}, corner_angle_degrees{}, front_extension{}, radius{};
    // Original vehicle basis at 5b1c,5b34,5b10,5b40: backward,left,forward,right.
    std::array<Vec3, 4> edge_normals{};
};
struct WallContact {
    bool hit{}, restore_previous{};
    Vec3 position{}, contact_point{}, normal{};
    float heading_degrees{}, removed_fraction{};
};
std::array<Vec3, 4> collision_footprint(const CollisionBody& body);
bool body_intersects_walls(const CollisionBody& body, std::span<const WallRecord> walls,
                           bool& enclosed_wall);
WallContact solve_wall_contact(const CollisionBody& previous, const CollisionBody& current,
                               std::span<const WallRecord> walls);
class TrackCollision {
  public:
    Result load(ResourceStore& resources, std::string_view track, const MeshObject& scene,
                std::string& error);
    GroundContact sample(Vec3 point) const;
    const TrackSpawn& spawn() const { return spawn_; }
    std::vector<WallRecord> nearby_walls(Vec3 current, Vec3 previous, float radius,
                                         bool check_height = true) const;
    std::size_t surface_record_count() const;

  private:
    struct Grid {
        std::uint32_t width{}, depth{}, stride{};
        std::int32_t origin_x{}, origin_z{}, right{}, bottom{}, cell_width{}, cell_depth{};
        std::vector<std::vector<std::byte>> cells;
        Result decode(std::span<const std::byte> bytes, std::uint32_t expected_stride);
        std::span<const std::byte> cell(Vec3 point) const;
    };
    struct Friction {
        std::string skid_sound_path;
        std::uint32_t kind{};
        float longitudinal{}, lateral{}, rolling{};
        std::uint32_t particle_dust{}, particle_color_argb{}, skid_color_argb{};
    };
    Grid surfaces_, walls_, friction_grid_;
    std::vector<Friction> friction_;
    TrackSpawn spawn_{};
};
} // namespace roadwars
