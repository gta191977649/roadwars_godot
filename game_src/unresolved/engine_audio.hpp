#pragma once
#include "game_src/unresolved/resource_store.hpp"

namespace roadwars {
struct WavePcm {
    std::uint32_t sample_rate{};
    std::uint16_t channels{}, bits_per_sample{};
    std::vector<std::byte> data;
    std::uint64_t frame_count() const;
};
Result decode_wave_pcm(std::span<const std::byte> bytes, WavePcm& output);

struct EngineAudioResources {
    std::string sample_path;
    WavePcm sample;
    float sfx_volume{};
};
Result load_engine_audio(ResourceStore& resources, std::string_view vehicle,
                         EngineAudioResources& output, std::string& error);

enum class EngineVoiceAction { none, start, update, stop };
struct EngineAudioCommand {
    EngineVoiceAction action{EngineVoiceAction::none};
    bool set_frequency{}, set_volume{}, loop{};
    std::uint32_t frequency_hz{};
    std::int32_t qmdx_volume{};
    float linear_gain{}, pitch_scale{1};
    Vec3 position{}, velocity{};
};
bool engine_audio_audible(Vec3 source, Vec3 listener);
std::uint32_t original_engine_frequency(float rpm, bool loaded);

// Portable voice-command state for the undamaged human-driver engine path.
// It does not own or call an audio backend. Missing/other sounds remain separate.
class EngineAudio {
  public:
    Result initialize(std::uint32_t source_sample_rate, float sfx_volume);
    Result update(float delta_seconds, float rpm, Vec3 position, Vec3 velocity, bool audible,
                  EngineAudioCommand& output, bool loaded = false, float cached_speed = 0);
    void reset();
    bool playing() const { return playing_; }

  private:
    std::uint32_t sample_rate_{};
    float sfx_volume_{}, timer_{};
    bool playing_{}, previous_loaded_{true};
};
} // namespace roadwars
