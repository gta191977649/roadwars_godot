#include "platform/godot/extension/sfx_audio_adapter.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace roadwars {
void SfxAudioAdapter::set_tire_sample(const WavePcm& sample) {
    tire_.reset();
    tire_ =
        std::make_unique<EngineAudioAdapter>(owner_, sample, true, "OriginalTireSkidVoice", true);
}
void SfxAudioAdapter::submit_tire(const EngineAudioCommand& command) {
    if (tire_)
        tire_->submit(command);
}
void SfxAudioAdapter::play_one_shot(const WavePcm& sample, const EngineAudioCommand& command,
                                    std::string_view name, std::uint32_t extra_repeats,
                                    bool spatial) {
    if (command.action != EngineVoiceAction::start)
        return;
    WavePcm repeated;
    const WavePcm* output = &sample;
    if (extra_repeats) {
        const auto plays = static_cast<std::size_t>(extra_repeats) + 1;
        if (plays > 33 || sample.data.size() > std::numeric_limits<std::size_t>::max() / plays)
            throw std::runtime_error("Unsupported finite original sound repeat count");
        repeated = sample;
        repeated.data.reserve(sample.data.size() * plays);
        for (std::uint32_t i = 0; i < extra_repeats; ++i)
            repeated.data.insert(repeated.data.end(), sample.data.begin(), sample.data.end());
        output = &repeated;
    }
    auto voice = std::make_unique<EngineAudioAdapter>(
        owner_, *output, false, std::string(name) + "_" + std::to_string(one_shots_started_),
        spatial);
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
