#pragma once
#include "game_src/unresolved/engine_audio.hpp"

namespace roadwars {
// Portable host codec replacement for the original QMDX Microsoft ADPCM path.
Result decode_ms_adpcm_wave(std::span<const std::byte>, WavePcm&);

// New portable owner for the evidenced no-CD menu/race music paths; original class unknown.
class RaceMusic {
  public:
    Result load(ResourceStore&, std::string& error);
    Result load_menu(ResourceStore&, std::string& error);
    Result load_character(ResourceStore&, std::string_view character, std::string& error);
    EngineAudioCommand gain(float scale) const;
    void set_volume(float volume);
    Result start(EngineAudioCommand&);
    EngineAudioCommand stop();
    const WavePcm& sample() const { return sample_; }
    bool playing() const { return playing_; }

  private:
    Result load_track(ResourceStore&, std::string_view path, std::string_view volume_key,
                      std::string& error);
    WavePcm sample_;
    float volume_{};
    bool playing_{};
};
} // namespace roadwars
