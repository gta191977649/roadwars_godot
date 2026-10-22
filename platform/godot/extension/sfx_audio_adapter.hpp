#pragma once
#include "platform/godot/extension/engine_audio_adapter.hpp"
#include <memory>

namespace roadwars {
// Native playback only. Portable reconstruction decides all sound triggers and levels.
class SfxAudioAdapter {
  public:
    explicit SfxAudioAdapter(godot::Node& owner) : owner_(owner) {}
    void set_tire_sample(const WavePcm& sample);
    void submit_tire(const EngineAudioCommand& command);
    void play_one_shot(const WavePcm& sample, const EngineAudioCommand& command,
                       std::string_view name);
    void reap_finished();
    void reset();
    void set_paused(bool paused);
    bool tire_playing() const;
    double tire_playback_position() const;
    std::size_t playing_one_shots() const;
    std::uint64_t one_shots_started() const { return one_shots_started_; }

  private:
    godot::Node& owner_;
    std::unique_ptr<EngineAudioAdapter> tire_;
    std::vector<std::unique_ptr<EngineAudioAdapter>> one_shots_;
    std::uint64_t one_shots_started_{};
};
} // namespace roadwars
