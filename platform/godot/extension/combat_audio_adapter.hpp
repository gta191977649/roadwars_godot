#pragma once
#include "game_src/unresolved/combat_audio.hpp"
#include "platform/godot/extension/sfx_audio_adapter.hpp"
#include <map>
namespace roadwars {
// Native PCM output; spatial commands use Godot distance attenuation and stereo panning.
class CombatAudioAdapter {
  public:
    CombatAudioAdapter(godot::Node& owner, const CombatAudioResources& resources)
        : owner_(owner), resources_(resources), effects_(owner) {}
    Result submit_one_shot(const CombatAudioCommand&);
    Result submit_loop(std::size_t key, const CombatAudioCommand&);
    bool loop_alive(std::size_t key) const;
    void reap_finished() { effects_.reap_finished(); }
    void set_paused(bool);
    void reset();

  private:
    struct Voice {
        CombatSample sample;
        std::unique_ptr<EngineAudioAdapter> adapter;
    };
    godot::Node& owner_;
    const CombatAudioResources& resources_;
    SfxAudioAdapter effects_;
    std::map<std::size_t, Voice> loops_;
};
} // namespace roadwars
