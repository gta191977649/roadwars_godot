#include "game_src/unresolved/combat_audio.hpp"
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
using namespace roadwars;
namespace {
class DirectoryReader final : public ResourceReader {
  public:
    explicit DirectoryReader(std::filesystem::path root) : root_(std::move(root)) {}
    Result read(std::string_view name, std::vector<std::byte>& output) override {
        std::ifstream file(root_ / std::filesystem::path(name), std::ios::binary | std::ios::ate);
        if (!file)
            return Result::unavailable;
        const auto size = file.tellg();
        output.resize(static_cast<std::size_t>(size));
        file.seekg(0);
        file.read(reinterpret_cast<char*>(output.data()), size);
        return file ? Result::ok : Result::unavailable;
    }

  private:
    std::filesystem::path root_;
};
} // namespace
int main(int argc, char** argv) {
    CombatAudioResources resources;
    resources.sfx_volume = .7f;
    for (auto& sample : resources.samples)
        sample.sample_rate = 22050;
    std::optional<CombatAudioEvent> event;
    assert(combat_audio_event(CombatAudioCue::accepted_fire, "Machine Gun", {1, 2, 3}, event) ==
           Result::ok);
    assert(event && event->sample == CombatSample::chain);
    CombatAudioCommand command;
    assert(combat_audio_command(*event, resources, command) == Result::ok);
    assert(command.voice.frequency_hz == 22050 && command.voice.pitch_scale == 1);
    assert(command.voice.qmdx_volume == 22936 && command.voice.position.z == 3);
    assert(command.voice.velocity.x == 0 && command.original_play_count == 0 &&
           !command.voice.loop);
    assert(combat_audio_event(CombatAudioCue::projectile_world, "20mm Cannon", {}, event) ==
           Result::ok);
    assert(event->sample == CombatSample::bullet_impact && event->gain_scalar == .5f);
    assert(combat_audio_command(*event, resources, command) == Result::ok &&
           command.voice.qmdx_volume == 11468);
    assert(combat_audio_event(CombatAudioCue::projectile_vehicle, "Plasma Blaster", {}, event) ==
           Result::ok);
    assert(event->sample == CombatSample::plasma_impact && event->gain_scalar == 1);
    assert(combat_audio_event(CombatAudioCue::projectile_world, "Rocket Launcher", {}, event) ==
           Result::ok);
    assert(event->sample == CombatSample::boom && event->gain_scalar == 1);
    assert(event->position.y == .1f);
    assert(combat_audio_event(CombatAudioCue::accepted_fire, "Oil Slick", {}, event) == Result::ok);
    assert(event->original_play_count == 4);
    assert(combat_audio_event(CombatAudioCue::accepted_fire, "Missile Jammer", {}, event) ==
           Result::ok);
    assert(event->original_play_count == -1 && event->notify_completion);
    for (const auto name :
         {"Smoke Screen", "Battering Spikes", "Hydraulic Ram", "Grenades", "Land Mines"}) {
        assert(combat_audio_event(CombatAudioCue::accepted_fire, name, {}, event) == Result::ok &&
               !event);
    }
    assert(combat_audio_event(CombatAudioCue::mine_deployment, "Land Mines", {}, event) ==
           Result::ok);
    assert(event->sample == CombatSample::mine_lay);
    assert(combat_audio_event(CombatAudioCue::selected_player_eliminated, "", {}, event) ==
           Result::ok);
    assert(event->sample == CombatSample::eliminated && !event->spatial);
    assert(combat_audio_event(CombatAudioCue::accepted_fire, "unresolved", {}, event) ==
           Result::invalid_argument);
    CombatRaceLoopAudio race;
    assert(race.initialize(CombatSample::nitrous) == Result::ok);
    std::array<CombatAudioSource, 3> sources{{{7, {5, 0, 0}, {1, 0, 0}, true},
                                              {8, {1, 0, 0}, {2, 3, 4}, true},
                                              {9, {0, 0, 0}, {}, false}}};
    std::optional<CombatAudioCommand> loop;
    assert(race.advance(.05f, {}, sources, false, resources, loop) == Result::ok && !loop);
    assert(race.advance(.001f, {}, sources, false, resources, loop) == Result::ok && loop);
    assert(race.selected_participant() == 8 && loop->voice.action == EngineVoiceAction::start);
    assert(loop->voice.loop && loop->voice.velocity.y == 3 && loop->notify_completion);
    sources[0].position = {1, 0, 0};
    assert(race.advance(.051f, {}, sources, true, resources, loop) == Result::ok && loop);
    assert(race.selected_participant() == 7); // Stable first participant on equal distance.
    assert(loop->voice.action == EngineVoiceAction::update && !loop->voice.set_frequency);
    for (auto& source : sources)
        source.eligible = false;
    assert(race.advance(.051f, {}, sources, true, resources, loop) == Result::ok && loop);
    assert(loop->voice.action == EngineVoiceAction::stop && !race.selected_participant());
    assert(race.advance(-1, {}, sources, false, resources, loop) == Result::invalid_argument);
    assert(combat_jammer_audio(true, false, {1, 2, 3}, {4, 5, 6}, resources, loop) == Result::ok);
    assert(loop->voice.action == EngineVoiceAction::start && loop->voice.loop);
    assert(combat_jammer_audio(true, true, {}, {}, resources, loop) == Result::ok);
    assert(loop->voice.action == EngineVoiceAction::update);
    assert(combat_jammer_audio(false, true, {}, {}, resources, loop) == Result::ok);
    assert(loop->voice.action == EngineVoiceAction::stop);
    if (argc > 1) {
        DirectoryReader reader(argv[1]);
        ResourceStore originals(reader);
        assert(originals.open() == Result::ok);
        std::string error;
        if (load_combat_audio(originals, resources, error) != Result::ok)
            throw std::runtime_error(error);
        for (const auto& sample : resources.samples)
            assert(sample.sample_rate > 0 && sample.frame_count() > 0);
    }
}
