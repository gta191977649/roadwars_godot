#pragma once
#include "game_src/unresolved/track_collision.hpp"
#include <functional>

namespace roadwars {
struct SceneWeatherCamera {
    Vec3 position{};
    float heading{}, pitch{}, roll{};
};
struct SceneWeatherParticle {
    Vec3 position{}, spawn_position{};
    float heading{}, pitch{}, age{}, ground_height{};
    bool visible{}, positive_x{true}, positive_z{true};
};
// Portable state of ctor004d0bf0 and callback004d1999. Original class name unresolved.
class SceneWeather {
  public:
    Result load(ResourceStore&, std::string_view track, std::string& error);
    // Explicit zero-delta history seed: native constructor leaves these camera fields uninitialized.
    void seed_camera_history(const SceneWeatherCamera& camera) {
        previous_ = camera;
        seeded_ = true;
    }
    void observe_callbacks(std::function<void(std::uint64_t, bool)> observer);
    Result advance(float, const SceneWeatherCamera&, const TrackCollision&,
                   const std::function<std::uint32_t()>& shared_random, std::string& error);
    bool enabled() const { return enabled_; }
    const MeshObject& mesh() const { return mesh_; }
    std::span<const SceneWeatherParticle> particles() const { return particles_; }
    float emission_timer() const { return elapsed_; }

  private:
    std::array<SceneWeatherParticle, 64> particles_;
    MeshObject mesh_;
    SceneWeatherCamera previous_;
    float elapsed_{};
    bool enabled_{}, seeded_{};
};
} // namespace roadwars
