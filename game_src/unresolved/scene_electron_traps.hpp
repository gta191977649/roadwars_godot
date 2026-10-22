#pragma once
#include "game_src/unresolved/vehicle_combat.hpp"

namespace roadwars {
struct SceneElectronTrapPose {
    std::string name;
    Vec3 origin{};
    float heading{}, range{}, timer{};
    bool active{}, beam_visible{};
    Transform beam_pose;
};
// Track-owned Electron endpoints004b7439, separate from vehicle ammunition/mount ownership.
class SceneElectronTraps {
  public:
    Result load(ResourceStore&, const MeshObject& world, std::string& error);
    // Direct004ba95f phase, after the ordinary turret controller loop and RotObjects.
    Result advance_world(float delta, std::string& error);
    void observe_callbacks(std::function<void(std::uint64_t, bool)>);
    std::vector<std::uint64_t> callback_tokens() const;
    Result dispatch_callback(std::uint64_t, float, std::span<const VehicleCombatTarget>,
                             const VehicleCombatServices&, std::string& error);
    std::span<const SceneElectronTrapPose> poses() const { return poses_; }
    const SkeletalMeshRuntime* geometry(std::size_t index) const;
    std::vector<CombatAudioEvent> drain_audio_events();

  private:
    struct Item {
        ElectronCannonState state;
        SkeletalMeshRuntime mesh;
        bool registered{};
    };
    std::vector<Item> items_;
    std::vector<SceneElectronTrapPose> poses_;
    std::function<void(std::uint64_t, bool)> observer_;
    std::vector<CombatAudioEvent> audio_events_;
};
} // namespace roadwars
