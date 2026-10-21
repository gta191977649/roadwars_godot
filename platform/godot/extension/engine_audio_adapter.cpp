#include "platform/godot/extension/engine_audio_adapter.hpp"
#include <cstring>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <stdexcept>

namespace roadwars {
EngineAudioAdapter::EngineAudioAdapter(godot::Node& owner, const WavePcm& sample, bool loop,
                                       std::string_view voice_name)
    : owner_(owner) {
    if (!sample.sample_rate || !sample.frame_count() ||
        (sample.bits_per_sample != 8 && sample.bits_per_sample != 16) ||
        (sample.channels != 1 && sample.channels != 2))
        throw std::runtime_error("Original PCM voice is incomplete");
    stream_.instantiate();
    stream_->set_format(sample.bits_per_sample == 16 ? godot::AudioStreamWAV::FORMAT_16_BITS
                                                     : godot::AudioStreamWAV::FORMAT_8_BITS);
    stream_->set_mix_rate(static_cast<int>(sample.sample_rate));
    stream_->set_stereo(sample.channels == 2);
    godot::PackedByteArray data;
    data.resize(static_cast<std::int64_t>(sample.data.size()));
    std::memcpy(data.ptrw(), sample.data.data(), sample.data.size());
    if (sample.bits_per_sample == 8)
        for (std::int64_t i = 0; i < data.size(); ++i)
            data[i] ^= 0x80; // WAV unsigned -> Godot signed PCM8.
    stream_->set_data(data);
    stream_->set_loop_mode(loop ? godot::AudioStreamWAV::LOOP_FORWARD
                                : godot::AudioStreamWAV::LOOP_DISABLED);
    stream_->set_loop_begin(0);
    stream_->set_loop_end(static_cast<int>(sample.frame_count()));
    sample_frames_ = sample.frame_count();
    player_ = memnew(godot::AudioStreamPlayer);
    player_->set_name(godot::String::utf8(std::string(voice_name).c_str()));
    player_->set_stream(stream_);
    player_->set_max_polyphony(1);
    owner_.add_child(player_);
}
EngineAudioAdapter::~EngineAudioAdapter() {
    if (!player_)
        return;
    player_->stop();
    owner_.remove_child(player_);
    memdelete(player_);
}
void EngineAudioAdapter::submit(const EngineAudioCommand& command) {
    if (command.action == EngineVoiceAction::stop) {
        player_->stop();
        return;
    }
    if (command.action == EngineVoiceAction::none)
        return;
    if (command.set_frequency) {
        player_->set_pitch_scale(command.pitch_scale);
        frequency_hz_ = command.frequency_hz;
    }
    if (command.set_volume) {
        player_->set_volume_linear(command.linear_gain);
        linear_gain_ = command.linear_gain;
    }
    if (command.action == EngineVoiceAction::start)
        player_->play();
}
bool EngineAudioAdapter::playing() const { return player_ && player_->is_playing(); }
double EngineAudioAdapter::playback_position() const {
    return player_ ? player_->get_playback_position() : 0;
}
void EngineAudioAdapter::seek(double seconds) { player_->seek(seconds); }
} // namespace roadwars
