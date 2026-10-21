#pragma once
#include "game_src/unresolved/engine_audio.hpp"
#include <array>

namespace roadwars {
enum class CollisionSample : std::uint32_t { crash, land };
struct CollisionAudioEvent {
    CollisionSample sample{};
    Vec3 position{};
};
struct CollisionAudioResources {
    std::array<WavePcm, 2> samples;
    float sfx_volume{};
};
Result load_collision_audio(ResourceStore& resources, CollisionAudioResources& output,
                            std::string& error);
EngineAudioCommand collision_audio_command(const CollisionAudioEvent& event,
                                           const CollisionAudioResources& resources);

// Original vehicle-owned cooldowns; each accepted event requests a new one-shot voice.
class CollisionAudio {
  public:
    void reset();
    Result advance(float delta_seconds);
    bool wall_contact(float cached_speed, Vec3 position);
    bool wheel_landing(float prior_air_time, Vec3 vehicle_position);
    // Original 0041de2a param9: supplied by the car/car solver, not relative speed.
    bool vehicle_contact(float original_impact_strength, Vec3 position);
    std::span<const CollisionAudioEvent> events() const { return events_; }

  private:
    float wall_cooldown_{}, vehicle_cooldown_{};
    std::vector<CollisionAudioEvent> events_;
};
} // namespace roadwars
