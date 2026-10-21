#include "game_src/unresolved/tire_audio.hpp"
#include "game_src/unresolved/binary_reader.hpp"
#include <cmath>

namespace roadwars {
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00436270
Result load_tire_audio(ResourceStore& resources, std::string_view track, TireAudioResources& output,
                       std::string& error) {
    if (track.empty() || track.find_first_of("/\\:") != std::string_view::npos || track == "." ||
        track == "..")
        return Result::invalid_argument;
    std::vector<std::byte> bytes;
    auto status = resources.read("Tracks/" + std::string(track) + "/World/FrictionGrid.bin", bytes);
    if (status != Result::ok) {
        error = "Missing tire audio friction materials";
        return status;
    }
    TireAudioResources result;
    try {
        BinaryReader reader(bytes);
        const auto count = reader.u32();
        if (!count || count > reader.remaining() / 552)
            return Result::invalid_argument;
        for (std::uint32_t i = 0; i < count; ++i) {
            BinaryReader material(reader.take(552));
            material.take(288);
            const auto filename = material.text(260);
            if (filename == "none")
                continue;
            const std::string path = "Sfx/" + filename;
            if (result.samples.contains(path))
                continue;
            std::vector<std::byte> wave;
            WavePcm sample;
            status = resources.read(path, wave);
            if (status != Result::ok || (status = decode_wave_pcm(wave, sample)) != Result::ok) {
                error = "Missing or unsupported tire sample: " + path;
                return status;
            }
            result.samples.emplace(path, std::move(sample));
        }
    } catch (const std::exception&) {
        error = "Invalid tire audio friction materials";
        return Result::invalid_argument;
    }
    Configuration config;
    if (config.load(resources, "Ini/RoadWars.ini") != Result::ok ||
        config.number("ROADWARS", "SFX", result.sfx_volume) != Result::ok ||
        result.sfx_volume < 0 || result.sfx_volume > 1) {
        error = "Invalid original SFX volume";
        return Result::invalid_argument;
    }
    output = std::move(result);
    error.clear();
    return Result::ok;
}
Result TireAudio::initialize(float volume) {
    if (!std::isfinite(volume) || volume < 0 || volume > 1)
        return Result::invalid_argument;
    sfx_volume_ = volume;
    reset();
    return Result::ok;
}
void TireAudio::reset() {
    skid_active_ = playing_ = false;
    active_sample_path_.clear();
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0041330f
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00413425
Result TireAudio::update(const TireAudioInput& input, EngineAudioCommand& output) {
    if (!std::isfinite(input.first_tire_longitudinal) || !std::isfinite(input.position.x) ||
        !std::isfinite(input.position.y) || !std::isfinite(input.position.z))
        return Result::invalid_argument;
    EngineAudioCommand command;
    if (!input.any_skid) {
        if (playing_)
            command.action = EngineVoiceAction::stop;
        skid_active_ = playing_ = false;
    } else {
        if (!skid_active_) {
            // Original sets this latch BEFORE the material threshold test.
            skid_active_ = true;
            if (input.first_tire_longitudinal > 0.6000000238418579f && !input.sample_path.empty()) {
                playing_ = true;
                active_sample_path_ = input.sample_path;
                command.action = EngineVoiceAction::start;
                command.loop = true;
                command.set_volume = true;
                command.qmdx_volume = static_cast<std::int32_t>(sfx_volume_ * 32767.0f);
                command.linear_gain = static_cast<float>(command.qmdx_volume) / 32767.0f;
            }
        }
        if (playing_) {
            if (command.action == EngineVoiceAction::none)
                command.action = EngineVoiceAction::update;
            command.position = input.position;
        }
    }
    output = command;
    return Result::ok;
}
} // namespace roadwars
