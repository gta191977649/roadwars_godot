#pragma once
#include "game_src/unresolved/engine_audio.hpp"
#include <map>

namespace roadwars {
struct TireAudioResources {
    std::map<std::string, WavePcm> samples;
    float sfx_volume{};
};
Result load_tire_audio(ResourceStore& resources, std::string_view track, TireAudioResources& output,
                       std::string& error);
struct TireAudioInput {
    bool any_skid{};
    float first_tire_longitudinal{};
    std::string sample_path;
    Vec3 position{};
};
class TireAudio {
  public:
    Result initialize(float sfx_volume);
    Result update(const TireAudioInput& input, EngineAudioCommand& output);
    void reset();
    bool playing() const { return playing_; }
    const std::string& active_sample_path() const { return active_sample_path_; }

  private:
    float sfx_volume_{};
    bool skid_active_{}, playing_{};
    std::string active_sample_path_;
};
} // namespace roadwars
