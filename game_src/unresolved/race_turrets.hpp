#pragma once
#include "game_src/unresolved/mesh_scene.hpp"
#include "game_src/unresolved/turret_runtime.hpp"
#include "game_src/unresolved/weapon_runtime.hpp"

namespace roadwars {
struct RaceTurretPose {
    std::string name;
    std::vector<std::size_t> node_path;
    Vec3 local_position{}, world_position{};
    float heading{}, pitch{};
    std::optional<std::size_t> target;
};
struct RaceTurretFireEvent {
    std::size_t turret_index{};
    const WeaponDescriptor* weapon{};
    WeaponParameters parameters;
    TurretFireEvent firing;
};
Result load_turret_target_exemption(ResourceStore&, std::string_view character, bool& output,
                                    std::string& error);
// New portable collection for track+79c0/+79c4; original class/module name is unresolved.
class RaceTurrets {
  public:
    Result load(ResourceStore&, std::string_view track, const MeshObject& world,
                std::string& error);
    Result advance(float dt, std::span<const TurretTarget>, std::vector<RaceTurretFireEvent>&,
                   std::string& error, bool weapons_unlocked = true);
    Result advance_one(std::size_t, float dt, std::span<const TurretTarget>,
                       std::optional<RaceTurretFireEvent>&, std::string& error,
                       bool weapons_unlocked = true);
    void reset();
    std::span<const RaceTurretPose> poses() const { return poses_; }
    bool guns_enabled() const { return guns_enabled_; }

  private:
    struct Item {
        TurretRuntime runtime;
        const WeaponDescriptor* descriptor{};
        WeaponParameters parameters;
    };
    std::vector<Item> items_;
    std::vector<RaceTurretPose> poses_;
    bool guns_enabled_{};
};
} // namespace roadwars
