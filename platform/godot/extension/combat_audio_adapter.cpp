#include "platform/godot/extension/combat_audio_adapter.hpp"
namespace roadwars {
Result CombatAudioAdapter::submit_one_shot(const CombatAudioCommand& command) {
    const auto index = static_cast<std::size_t>(command.sample);
    if (index >= resources_.samples.size() || command.original_play_count < 0)
        return Result::invalid_argument;
    effects_.play_one_shot(resources_.samples[index], command.voice, "OriginalCombatVoice",
                           static_cast<std::uint32_t>(command.original_play_count),
                           command.spatial);
    return Result::ok;
}
Result CombatAudioAdapter::submit_loop(std::size_t key, const CombatAudioCommand& command) {
    const auto index = static_cast<std::size_t>(command.sample);
    if (index >= resources_.samples.size())
        return Result::invalid_argument;
    auto found = loops_.find(key);
    if (found == loops_.end() || found->second.sample != command.sample) {
        if (command.voice.action != EngineVoiceAction::start)
            return Result::ok;
        Voice voice{command.sample,
                    std::make_unique<EngineAudioAdapter>(
                        owner_, resources_.samples[index], true,
                        "OriginalCombatLoop_" + std::to_string(key), command.spatial)};
        found = loops_.insert_or_assign(key, std::move(voice)).first;
    }
    found->second.adapter->submit(command.voice);
    return Result::ok;
}
bool CombatAudioAdapter::loop_alive(std::size_t key) const {
    const auto found = loops_.find(key);
    return found != loops_.end() && found->second.adapter->playing();
}
void CombatAudioAdapter::set_paused(bool paused) {
    effects_.set_paused(paused);
    for (auto& [key, voice] : loops_)
        voice.adapter->set_paused(paused);
}
void CombatAudioAdapter::reset() {
    effects_.reset();
    loops_.clear();
}
} // namespace roadwars
