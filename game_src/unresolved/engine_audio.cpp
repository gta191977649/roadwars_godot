#include "game_src/unresolved/engine_audio.hpp"
#include "game_src/unresolved/binary_reader.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace roadwars {
std::uint64_t WavePcm::frame_count() const {
    const auto alignment = static_cast<std::uint32_t>(channels) * bits_per_sample / 8;
    return alignment ? data.size() / alignment : 0;
}

Result decode_wave_pcm(std::span<const std::byte> bytes, WavePcm& output) {
    try {
        BinaryReader reader(bytes);
        if (reader.text(4) != "RIFF")
            return Result::invalid_argument;
        const auto riff_size = reader.u32();
        if (riff_size < 4 || riff_size > bytes.size() - 8 || reader.text(4) != "WAVE")
            return Result::invalid_argument;
        const auto end = static_cast<std::size_t>(riff_size) + 8;
        WavePcm result;
        bool format_found{}, data_found{};
        std::uint16_t alignment{};
        while (reader.offset() < end) {
            if (end - reader.offset() < 8)
                return Result::invalid_argument;
            const auto kind = reader.text(4);
            const auto size = reader.u32();
            if (size > end - reader.offset())
                return Result::invalid_argument;
            const auto chunk = reader.take(size);
            if (kind == "fmt ") {
                if (format_found || size < 16)
                    return Result::invalid_argument;
                BinaryReader format(chunk);
                if (format.u16() != 1)
                    return Result::unavailable;
                result.channels = format.u16();
                result.sample_rate = format.u32();
                const auto byte_rate = format.u32();
                alignment = format.u16();
                result.bits_per_sample = format.u16();
                if ((result.channels != 1 && result.channels != 2) ||
                    (result.bits_per_sample != 8 && result.bits_per_sample != 16) ||
                    !result.sample_rate || result.sample_rate > 384000 ||
                    alignment != result.channels * result.bits_per_sample / 8 ||
                    byte_rate != result.sample_rate * alignment)
                    return Result::invalid_argument;
                format_found = true;
            } else if (kind == "data") {
                if (data_found)
                    return Result::invalid_argument;
                result.data.assign(chunk.begin(), chunk.end());
                data_found = true;
            }
            if (size & 1) {
                if (reader.offset() == end)
                    return Result::invalid_argument;
                reader.take(1);
            }
        }
        if (!format_found || !data_found || result.data.empty() ||
            result.data.size() % alignment != 0)
            return Result::invalid_argument;
        output = std::move(result);
        return Result::ok;
    } catch (const std::exception&) {
        return Result::invalid_argument;
    }
}

// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0040d4cb
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004dd876
Result load_engine_audio(ResourceStore& resources, std::string_view vehicle,
                         EngineAudioResources& output, std::string& error) {
    if (vehicle.empty() || vehicle.find_first_of("/\\:") != std::string_view::npos ||
        vehicle == "." || vehicle == "..")
        return Result::invalid_argument;
    EngineAudioResources result;
    result.sample_path = "Cars/" + std::string(vehicle) + "/Engines/Sound.wav";
    std::vector<std::byte> bytes;
    auto status = resources.read(result.sample_path, bytes);
    if (status != Result::ok) {
        error = "Missing engine sample: " + result.sample_path;
        return status;
    }
    status = decode_wave_pcm(bytes, result.sample);
    if (status != Result::ok) {
        error = "Unsupported engine WAV: " + result.sample_path;
        return status;
    }
    Configuration config;
    status = config.load(resources, "Ini/RoadWars.ini");
    if (status != Result::ok || config.number("ROADWARS", "SFX", result.sfx_volume) != Result::ok ||
        result.sfx_volume < 0 || result.sfx_volume > 1) {
        error = "Missing or invalid original SFX volume";
        return Result::invalid_argument;
    }
    output = std::move(result);
    error.clear();
    return Result::ok;
}

// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00484cae
bool engine_audio_audible(Vec3 source, Vec3 listener) {
    return std::abs(source.x - listener.x) + std::abs(source.z - listener.z) < 75.0f;
}

// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004137a7
std::uint32_t original_engine_frequency(float rpm, bool loaded) {
    // The observed caller supplies nonnegative RPM; preserve integer truncation before the curve.
    const auto n = static_cast<std::int32_t>(std::min(rpm + 500.0f, 10000.0f));
    const auto linear = loaded ? n * 4 : n * 4 - (n / 2 + 800);
    return static_cast<std::uint32_t>(static_cast<double>(linear) +
                                      static_cast<double>(n * n) / 3000.0);
}

Result EngineAudio::initialize(std::uint32_t source_sample_rate, float sfx_volume) {
    if (!source_sample_rate || source_sample_rate > 384000 || !std::isfinite(sfx_volume) ||
        sfx_volume < 0 || sfx_volume > 1)
        return Result::invalid_argument;
    sample_rate_ = source_sample_rate;
    sfx_volume_ = sfx_volume;
    reset();
    return Result::ok;
}
void EngineAudio::reset() {
    timer_ = 0;
    playing_ = false;
    previous_loaded_ = true;
}

// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0041373a
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004137a7
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004ddb75
Result EngineAudio::update(float dt, float rpm, Vec3 position, Vec3 velocity, bool audible,
                           EngineAudioCommand& output, bool loaded, float cached_speed) {
    if (!sample_rate_ || !std::isfinite(dt) || dt < 0 || !std::isfinite(rpm) || rpm < 0 ||
        !std::isfinite(cached_speed) || cached_speed < 0)
        return Result::invalid_argument;
    EngineAudioCommand command;
    if (!audible) {
        if (playing_) {
            command.action = EngineVoiceAction::stop;
            playing_ = false;
        }
        output = command;
        return Result::ok;
    }
    if (!playing_) {
        playing_ = true;
        timer_ = 0.02500000037252903f;
        command.action = EngineVoiceAction::start;
        command.loop = true;
        command.set_frequency = command.set_volume = true;
        command.frequency_hz = sample_rate_;
        command.qmdx_volume = static_cast<std::int32_t>(32767.0f * sfx_volume_);
    }
    timer_ = dt + timer_;
    if (timer_ > 0.02500000037252903f) {
        timer_ = 0;
        if (command.action != EngineVoiceAction::start)
            command.action = EngineVoiceAction::update;
        command.set_frequency = true;
        command.frequency_hz = original_engine_frequency(rpm, loaded);
        if (loaded != previous_loaded_) {
            command.set_volume = true;
            command.qmdx_volume =
                static_cast<std::int32_t>(32767.0f * ((loaded ? 1.0f : 0.75f) * sfx_volume_));
        }
        const auto speed =
            std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z);
        command.position = position;
        command.velocity =
            cached_speed > 111.75f && speed > 0
                ? Vec3{velocity.x * (111.75f / speed), velocity.y * (111.75f / speed),
                       velocity.z * (111.75f / speed)}
                : velocity;
        previous_loaded_ = loaded;
    }
    if (command.set_frequency)
        command.pitch_scale = static_cast<float>(command.frequency_hz) / sample_rate_;
    if (command.set_volume)
        command.linear_gain = static_cast<float>(command.qmdx_volume) / 32767.0f;
    output = command;
    return Result::ok;
}
} // namespace roadwars
