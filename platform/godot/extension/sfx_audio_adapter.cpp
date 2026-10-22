#include "platform/godot/extension/sfx_audio_adapter.hpp"
#include <algorithm>

namespace roadwars {
void SfxAudioAdapter::set_tire_sample(const WavePcm& sample) {
    tire_.reset();
    tire_ = std::make_unique<EngineAudioAdapter>(owner_, sample, true, "OriginalTireSkidVoice");
}
void SfxAudioAdapter::submit_tire(const EngineAudioCommand& command) {
    if (tire_)
        tire_->submit(command);
}
void SfxAudioAdapter::play_one_shot(const WavePcm& sample, const EngineAudioCommand& command,
                                    std::string_view name) {
    if (command.action != EngineVoiceAction::start)
        return;
    auto voice = std::make_unique<EngineAudioAdapter>(
        owner_, sample, false, std::string(name) + "_" + std::to_string(one_shots_started_));
    voice->submit(command);
    one_shots_.push_back(std::move(voice));
    ++one_shots_started_;
}
void SfxAudioAdapter::reap_finished() {
    std::erase_if(one_shots_, [](const auto& voice) { return !voice->playing(); });
}
void SfxAudioAdapter::set_paused(bool paused) {
    if (tire_)
        tire_->set_paused(paused);
    for (auto& voice : one_shots_)
        voice->set_paused(paused);
}
void SfxAudioAdapter::reset() {
    EngineAudioCommand stop;
    stop.action = EngineVoiceAction::stop;
    if (tire_)
        tire_->submit(stop);
    one_shots_.clear();
    one_shots_started_ = 0;
}
bool SfxAudioAdapter::tire_playing() const { return tire_ && tire_->playing(); }
double SfxAudioAdapter::tire_playback_position() const {
    return tire_ ? tire_->playback_position() : 0;
}
std::size_t SfxAudioAdapter::playing_one_shots() const {
    return static_cast<std::size_t>(std::count_if(
        one_shots_.begin(), one_shots_.end(), [](const auto& voice) { return voice->playing(); }));
}
} // namespace roadwars
