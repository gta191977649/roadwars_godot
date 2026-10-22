#pragma once
#include "game_src/unresolved/engine_audio.hpp"
#include <array>
#include <optional>

namespace roadwars {
enum class CombatSample : std::size_t {
    chain,
    bullet_impact,
    heat,
    boom,
    fire,
    eliminated,
    electron,
    electron_buzz,
    flame,
    mine_lay,
    plasma,
    oil,
    nitrous,
    jammer,
    vulcan,
    cannon,
    plasma_impact,
    vulcan_impact,
    count
};
std::string_view combat_sample_path(CombatSample);
struct CombatAudioResources {
    std::array<WavePcm, static_cast<std::size_t>(CombatSample::count)> samples;
    float sfx_volume{};
};
Result load_combat_audio(ResourceStore&, CombatAudioResources&, std::string& error);
enum class CombatAudioCue {
    accepted_fire,
    projectile_vehicle,
    projectile_world,
    grenade_explosion,
    mine_deployment,
    electron_contact,
    selected_player_eliminated
};
struct CombatAudioEvent {
    CombatSample sample{};
    Vec3 position{};
    float gain_scalar{1};
    // Exact PlayEx repeat argument:0 means once,4 means five total plays,-1 indefinite.
    std::int32_t original_play_count{};
    bool spatial{true}, notify_completion{};
};
// Known silent original branches return ok with an empty event. Unknown identities fail.
Result combat_audio_event(CombatAudioCue, std::string_view weapon, Vec3 position,
                          std::optional<CombatAudioEvent>&);
struct CombatAudioCommand {
    CombatSample sample{};
    EngineAudioCommand voice;
    std::int32_t original_play_count{};
    std::uint32_t priority{};
    bool spatial{true}, notify_completion{};
};
Result combat_audio_command(const CombatAudioEvent&, const CombatAudioResources&,
                            CombatAudioCommand&);
struct CombatAudioSource {
    std::size_t participant{};
    Vec3 position{}, velocity{};
    bool eligible{};
};
// Portable owner of the race's one shared Fire or Nitrous channel, not an original class name.
class CombatRaceLoopAudio {
  public:
    Result initialize(CombatSample);
    Result advance(float delta, Vec3 listener, std::span<const CombatAudioSource>,
                   bool backend_voice_alive, const CombatAudioResources&,
                   std::optional<CombatAudioCommand>&);
    void reset() {
        timer_ = 0;
        selected_.reset();
    }
    std::optional<std::size_t> selected_participant() const { return selected_; }

  private:
    CombatSample sample_{};
    float timer_{};
    bool initialized_{};
    std::optional<std::size_t> selected_;
};
// One Jammer channel per actual jammer object; backend completion owns handle liveness.
Result combat_jammer_audio(bool active, bool backend_voice_alive, Vec3 position, Vec3 velocity,
                           const CombatAudioResources&, std::optional<CombatAudioCommand>&);
} // namespace roadwars
