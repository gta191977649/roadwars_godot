#include "game_src/unresolved/collision_audio.hpp"
#include <algorithm>
#include <cmath>

namespace roadwars {
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0044d760
// Crash/Land dependency subset of the original shared SFX loader.
Result load_collision_audio(ResourceStore& resources, CollisionAudioResources& output,
                            std::string& error) {
    CollisionAudioResources result;
    constexpr std::array<std::string_view, 2> paths{"Sfx/Crash.wav", "Sfx/Land.wav"};
    for (std::size_t i = 0; i < paths.size(); ++i) {
        std::vector<std::byte> bytes;
        auto status = resources.read(paths[i], bytes);
        if (status != Result::ok) {
            error = "Missing original collision sound: " + std::string(paths[i]);
            return status;
        }
        status = decode_wave_pcm(bytes, result.samples[i]);
        if (status != Result::ok) {
            error = "Unsupported original collision WAV: " + std::string(paths[i]);
            return status;
        }
    }
    Configuration config;
    if (config.load(resources, "Ini/RoadWars.ini") != Result::ok ||
        config.number("ROADWARS", "SFX", result.sfx_volume) != Result::ok ||
        !std::isfinite(result.sfx_volume) || result.sfx_volume < 0 || result.sfx_volume > 1) {
        error = "Missing or invalid original SFX volume";
        return Result::invalid_argument;
    }
    output = std::move(result);
    error.clear();
    return Result::ok;
}
// Numerical command contract from the one-shot 004dd8d4/004ddb75 path.
EngineAudioCommand collision_audio_command(const CollisionAudioEvent& event,
                                           const CollisionAudioResources& resources) {
    EngineAudioCommand command;
    command.action = EngineVoiceAction::start;
    command.set_frequency = command.set_volume = true;
    command.frequency_hz = resources.samples.at(static_cast<std::size_t>(event.sample)).sample_rate;
    command.qmdx_volume = static_cast<std::int32_t>(resources.sfx_volume * 32767.0f);
    command.linear_gain = static_cast<float>(command.qmdx_volume) / 32767.0f;
    command.pitch_scale = 1.0f;
    command.position = event.position;
    return command;
}
void CollisionAudio::reset() {
    wall_cooldown_ = vehicle_cooldown_ = 0;
    events_.clear();
}
Result CollisionAudio::advance(float dt) {
    if (!std::isfinite(dt) || dt < 0)
        return Result::invalid_argument;
    events_.clear();
    // 0047fff6 clamps only the wall timer; vehicle timer may cross below zero.
    wall_cooldown_ = std::max(wall_cooldown_ - dt, 0.0f);
    if (vehicle_cooldown_ > 0)
        vehicle_cooldown_ -= dt;
    return Result::ok;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0042e8f0
bool CollisionAudio::wall_contact(float cached_speed, Vec3 position) {
    if (!(cached_speed > 3.0f) || wall_cooldown_ > 0)
        return false;
    events_.push_back({CollisionSample::crash, position});
    wall_cooldown_ = 0.5f;
    return true;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0042a11c
bool CollisionAudio::wheel_landing(float prior_air_time, Vec3 position) {
    if (!(prior_air_time > 0.5f))
        return false;
    events_.push_back({CollisionSample::land, position});
    return true;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0041de2a
bool CollisionAudio::vehicle_contact(float strength, Vec3 position) {
    if (!(strength > 1000.0f) || vehicle_cooldown_ > 0)
        return false;
    events_.push_back({CollisionSample::crash, position});
    vehicle_cooldown_ = 0.20000000298023224f;
    return true;
}
} // namespace roadwars
