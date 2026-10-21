#pragma once
#include "game_src/unresolved/engine_audio.hpp"
#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/node.hpp>

namespace roadwars {
// Native Godot-owned PCM voice, shared by engine loops and original sound effects.
// QMDX spatial attenuation and Doppler are not claimed by this nonspatial adapter.
class EngineAudioAdapter {
  public:
    EngineAudioAdapter(godot::Node& owner, const WavePcm& sample, bool loop = true,
                       std::string_view voice_name = "OriginalEngineVoice");
    ~EngineAudioAdapter();
    EngineAudioAdapter(const EngineAudioAdapter&) = delete;
    EngineAudioAdapter& operator=(const EngineAudioAdapter&) = delete;
    void submit(const EngineAudioCommand& command);
    bool playing() const;
    double playback_position() const;
    void seek(double seconds);
    std::uint32_t frequency_hz() const { return frequency_hz_; }
    float linear_gain() const { return linear_gain_; }
    std::uint64_t sample_frames() const { return sample_frames_; }

  private:
    godot::Node& owner_;
    godot::AudioStreamPlayer* player_{};
    godot::Ref<godot::AudioStreamWAV> stream_;
    std::uint32_t frequency_hz_{};
    std::uint64_t sample_frames_{};
    float linear_gain_{};
};
} // namespace roadwars
