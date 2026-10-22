#pragma once
#include "game_src/unresolved/original_camera.hpp"
#include "game_src/unresolved/race_music.hpp"

namespace roadwars {
// Inferred portable presentation owner; original class identity is unresolved.
class RaceStart {
  public:
    Result load(ResourceStore&, const MeshObject&, std::string_view car,
                std::span<const TrackSpawn>, const TrackCollision&, std::string& error);
    void reset();
    Result update(float remaining, float dt, const VehicleDynamics&, const TrackCollision&);
    const CameraFrame& camera() const { return camera_; }
    std::span<const int> sounds() const { return pending_sounds_; }
    void clear_sounds() { pending_sounds_.clear(); }
    const WavePcm& sample(int index) const { return samples_.at(index); }
    float volume() const { return volume_; }
    int cue_count() const { return sounded_[0] + sounded_[1] + sounded_[2] + sounded_[3]; }
    float displayed_remaining() const { return remaining_; }
    static int digit(float remaining);

  private:
    std::array<Vec3, 3> cameras_{};
    Vec3 center_{}, eye_{}, velocity_{};
    CameraFrame camera_{};
    OriginalChaseCamera chase_;
    std::array<WavePcm, 4> samples_;
    std::array<bool, 4> sounded_{};
    std::vector<int> pending_sounds_;
    float volume_{}, remaining_{};
    bool acquired_{};
};
} // namespace roadwars
