#include "game_src/unresolved/combat_audio.hpp"
#include "game_src/unresolved/weapon_runtime.hpp"
#include <cmath>

namespace roadwars {
namespace {
constexpr std::array<std::string_view, 18> kPaths{
    "Sfx/wa-chain.wav",     "Sfx/MGImpact1.wav",   "Sfx/wa-heat.wav",     "Sfx/Boom.wav",
    "Sfx/fire.wav",         "Sfx/eliminated2.wav", "Sfx/electrongun.wav", "Sfx/electronBuzz.wav",
    "Sfx/FlameThrower.wav", "Sfx/MineLay.wav",     "Sfx/Plasma.wav",      "Sfx/Oil.wav",
    "Sfx/nitrousboost.wav", "Sfx/jammer.wav",      "Sfx/vulc.wav",        "Sfx/cannon.wav",
    "Sfx/PlasmaHit.wav",    "Sfx/VulcHit.wav"};
bool finite(Vec3 p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }
bool guided(std::string_view name) {
    return name == "Guided Missile" || name == "Back Missile" || name == "Swarmers" ||
           name == "The Excalibur";
}
} // namespace
std::string_view combat_sample_path(CombatSample sample) {
    const auto index = static_cast<std::size_t>(sample);
    return index < kPaths.size() ? kPaths[index] : std::string_view{};
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0044d760
// Combat dependency subset; Crash/Land remain owned by collision audio.
Result load_combat_audio(ResourceStore& resources, CombatAudioResources& output,
                         std::string& error) {
    CombatAudioResources next;
    for (std::size_t index = 0; index < kPaths.size(); ++index) {
        std::vector<std::byte> bytes;
        auto result = resources.read(kPaths[index], bytes);
        if (result == Result::ok)
            result = decode_wave_pcm(bytes, next.samples[index]);
        if (result != Result::ok) {
            error = "Original combat WAV unavailable or unsupported: " + std::string(kPaths[index]);
            return result;
        }
    }
    Configuration config;
    if (config.load(resources, "Ini/RoadWars.ini") != Result::ok ||
        config.number("ROADWARS", "SFX", next.sfx_volume) != Result::ok ||
        !std::isfinite(next.sfx_volume) || next.sfx_volume < 0 || next.sfx_volume > 1) {
        error = "Original SFX volume unavailable or invalid";
        return Result::invalid_argument;
    }
    output = std::move(next);
    error.clear();
    return Result::ok;
}
Result combat_audio_event(CombatAudioCue cue, std::string_view weapon, Vec3 position,
                          std::optional<CombatAudioEvent>& output) {
    output.reset();
    if (!finite(position))
        return Result::invalid_argument;
    const auto* descriptor = find_weapon_descriptor(weapon);
    if (!descriptor && cue != CombatAudioCue::selected_player_eliminated)
        return Result::invalid_argument;
    if (descriptor)
        weapon = descriptor->name;
    CombatAudioEvent event;
    event.position = position;
    switch (cue) {
    case CombatAudioCue::accepted_fire:
        if (weapon == "Machine Gun" || weapon == "Gatling Gun")
            event.sample = CombatSample::chain;
        else if (weapon == "20mm Cannon")
            event.sample = CombatSample::cannon;
        else if (weapon == "Plasma Blaster")
            event.sample = CombatSample::plasma;
        else if (weapon == "Vulcanized Gun")
            event.sample = CombatSample::vulcan;
        else if (weapon == "Rocket Launcher" || guided(weapon))
            event.sample = CombatSample::heat;
        else if (weapon == "Electron Cannon")
            event.sample = CombatSample::electron;
        else if (weapon == "Flame Thrower")
            event.sample = CombatSample::flame;
        else if (weapon == "Missile Jammer") {
            event.sample = CombatSample::jammer;
            event.original_play_count = -1;
            event.notify_completion = true;
        } else if (weapon == "Oil Slick") {
            event.sample = CombatSample::oil;
            event.original_play_count = 4;
        } else
            return Result::ok;
        break;
    case CombatAudioCue::projectile_vehicle:
    case CombatAudioCue::projectile_world:
        if (weapon == "Machine Gun" || weapon == "20mm Cannon" || weapon == "Gatling Gun")
            event.sample = CombatSample::bullet_impact;
        else if (weapon == "Plasma Blaster")
            event.sample = CombatSample::plasma_impact;
        else if (weapon == "Vulcanized Gun")
            event.sample = CombatSample::vulcan_impact;
        else if (weapon == "Rocket Launcher" || guided(weapon))
            event.sample = CombatSample::boom;
        else
            return Result::unavailable;
        if (cue == CombatAudioCue::projectile_world && event.sample != CombatSample::boom)
            event.gain_scalar = .5f;
        if (event.sample == CombatSample::boom)
            event.position.y += .1f;
        break;
    case CombatAudioCue::grenade_explosion:
        if (weapon != "Grenades")
            return Result::invalid_argument;
        event.sample = CombatSample::boom;
        break;
    case CombatAudioCue::mine_deployment:
        if (weapon != "Land Mines" && weapon != "Reactive Mines" && weapon != "Arachnoids")
            return Result::invalid_argument;
        event.sample = CombatSample::mine_lay;
        break;
    case CombatAudioCue::electron_contact:
        if (weapon != "Electron Cannon")
            return Result::invalid_argument;
        event.sample = CombatSample::electron_buzz;
        break;
    case CombatAudioCue::selected_player_eliminated:
        event.sample = CombatSample::eliminated;
        event.spatial = false;
        break;
    default:
        return Result::invalid_argument;
    }
    output = event;
    return Result::ok;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004dd8d4
// Numeric/spatial command contract; QMDX channel allocation and cone backend remain external.
Result combat_audio_command(const CombatAudioEvent& event, const CombatAudioResources& resources,
                            CombatAudioCommand& output) {
    const auto index = static_cast<std::size_t>(event.sample);
    if (index >= resources.samples.size() || !resources.samples[index].sample_rate ||
        !finite(event.position) || !std::isfinite(event.gain_scalar) || event.gain_scalar < 0 ||
        event.gain_scalar > 1 || !std::isfinite(resources.sfx_volume) || resources.sfx_volume < 0 ||
        resources.sfx_volume > 1)
        return Result::invalid_argument;
    CombatAudioCommand next;
    next.sample = event.sample;
    next.spatial = event.spatial;
    next.original_play_count = event.original_play_count;
    next.notify_completion = event.notify_completion;
    next.voice.action = EngineVoiceAction::start;
    next.voice.loop = event.original_play_count == -1;
    next.voice.set_frequency = next.voice.set_volume = true;
    next.voice.frequency_hz = resources.samples[index].sample_rate;
    next.voice.pitch_scale = 1;
    next.voice.qmdx_volume =
        static_cast<std::int32_t>(resources.sfx_volume * event.gain_scalar * 32767);
    next.voice.linear_gain = static_cast<float>(next.voice.qmdx_volume) / 32767;
    next.voice.position = event.position;
    output = next;
    return Result::ok;
}
Result CombatRaceLoopAudio::initialize(CombatSample sample) {
    if (sample != CombatSample::nitrous && sample != CombatSample::fire)
        return Result::invalid_argument;
    sample_ = sample;
    initialized_ = true;
    reset();
    return Result::ok;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00484f6d
// Eligibility is supplied from original race-owned state; QMDX voice liveness remains backend-owned.
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00484ecb
// Loop start command only; backend handles channel allocation/completion.
Result CombatRaceLoopAudio::advance(float dt, Vec3 listener,
                                    std::span<const CombatAudioSource> sources, bool alive,
                                    const CombatAudioResources& resources,
                                    std::optional<CombatAudioCommand>& output) {
    output.reset();
    if (!initialized_)
        return Result::unavailable;
    if (!std::isfinite(dt) || dt < 0 || !finite(listener))
        return Result::invalid_argument;
    for (const auto& source : sources)
        if (!finite(source.position) || !finite(source.velocity))
            return Result::invalid_argument;
    timer_ += dt;
    if (timer_ <= .05f)
        return Result::ok;
    timer_ = 0;
    const CombatAudioSource* selected{};
    float nearest{};
    for (const auto& source : sources) {
        if (!source.eligible)
            continue;
        const auto x = source.position.x - listener.x, y = source.position.y - listener.y,
                   z = source.position.z - listener.z;
        const auto distance = std::sqrt(x * x + y * y + z * z);
        if (!selected || distance < nearest) {
            selected = &source;
            nearest = distance;
        }
    }
    if (!selected) {
        selected_.reset();
        if (alive) {
            CombatAudioCommand stop;
            stop.sample = sample_;
            stop.voice.action = EngineVoiceAction::stop;
            output = stop;
        }
        return Result::ok;
    }
    selected_ = selected->participant;
    CombatAudioCommand command;
    const auto result =
        combat_audio_command({sample_, selected->position, 1, -1, true, true}, resources, command);
    if (result != Result::ok)
        return result;
    command.voice.action = alive ? EngineVoiceAction::update : EngineVoiceAction::start;
    command.voice.velocity = selected->velocity;
    if (alive)
        command.voice.set_frequency = command.voice.set_volume = false;
    output = command;
    return Result::ok;
}
Result combat_jammer_audio(bool active, bool alive, Vec3 position, Vec3 velocity,
                           const CombatAudioResources& resources,
                           std::optional<CombatAudioCommand>& output) {
    output.reset();
    if (!finite(position) || !finite(velocity))
        return Result::invalid_argument;
    if (!active) {
        if (alive) {
            CombatAudioCommand stop;
            stop.sample = CombatSample::jammer;
            stop.voice.action = EngineVoiceAction::stop;
            output = stop;
        }
        return Result::ok;
    }
    CombatAudioCommand command;
    const auto result = combat_audio_command({CombatSample::jammer, position, 1, -1, true, true},
                                             resources, command);
    if (result != Result::ok)
        return result;
    command.voice.action = alive ? EngineVoiceAction::update : EngineVoiceAction::start;
    command.voice.velocity = velocity;
    if (alive)
        command.voice.set_frequency = command.voice.set_volume = false;
    output = command;
    return Result::ok;
}
} // namespace roadwars
