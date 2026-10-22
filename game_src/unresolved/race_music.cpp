#include "game_src/unresolved/race_music.hpp"
#include "game_src/unresolved/binary_reader.hpp"
#include <algorithm>
#include <bit>
#include <cmath>

namespace roadwars {
Result decode_ms_adpcm_wave(std::span<const std::byte> bytes, WavePcm& output) {
    try {
        BinaryReader reader(bytes);
        if (reader.text(4) != "RIFF")
            return Result::invalid_argument;
        const auto size = reader.u32();
        if (size < 4 || size > bytes.size() - 8 || reader.text(4) != "WAVE")
            return Result::invalid_argument;
        const auto end = static_cast<std::size_t>(size) + 8;
        std::span<const std::byte> format_bytes, data;
        std::uint32_t frames{};
        bool fact_found{};
        while (reader.offset() < end) {
            if (end - reader.offset() < 8)
                return Result::invalid_argument;
            const auto name = reader.text(4);
            const auto count = reader.u32();
            if (count > end - reader.offset())
                return Result::invalid_argument;
            const auto chunk = reader.take(count);
            if (name == "fmt ") {
                if (!format_bytes.empty())
                    return Result::invalid_argument;
                format_bytes = chunk;
            } else if (name == "data") {
                if (!data.empty())
                    return Result::invalid_argument;
                data = chunk;
            } else if (name == "fact") {
                if (fact_found || count < 4)
                    return Result::invalid_argument;
                BinaryReader fact(chunk);
                frames = fact.u32();
                fact_found = true;
            }
            if (count & 1) {
                if (reader.offset() == end)
                    return Result::invalid_argument;
                reader.take(1);
            }
        }
        if (format_bytes.size() < 22 || data.empty() || !frames || !fact_found)
            return Result::invalid_argument;
        BinaryReader format(format_bytes);
        if (format.u16() != 2)
            return Result::unavailable;
        WavePcm result;
        result.channels = format.u16();
        result.sample_rate = format.u32();
        format.u32(); // Encoded average byte rate may be rounded by the encoder.
        const auto alignment = format.u16();
        const auto bits = format.u16();
        const auto extra = format.u16();
        const auto samples_per_block = format.u16();
        const auto coefficient_count = format.u16();
        if ((result.channels != 1 && result.channels != 2) || !result.sample_rate ||
            result.sample_rate > 384000 || bits != 4 || alignment < 7 * result.channels ||
            samples_per_block != (alignment - 7 * result.channels) * 2 / result.channels + 2 ||
            !coefficient_count || coefficient_count > 256 || extra != 4 + coefficient_count * 4 ||
            format_bytes.size() != 18u + extra)
            return Result::invalid_argument;
        std::vector<std::array<std::int16_t, 2>> coefficients;
        for (unsigned i = 0; i < coefficient_count; ++i)
            coefficients.push_back({std::bit_cast<std::int16_t>(format.u16()),
                                    std::bit_cast<std::int16_t>(format.u16())});
        const auto blocks = data.size() / alignment;
        const auto remainder = data.size() % alignment;
        if (remainder && remainder < 7u * result.channels)
            return Result::invalid_argument;
        const auto available =
            blocks * samples_per_block +
            (remainder ? (remainder - 7 * result.channels) * 2 / result.channels + 2 : 0);
        // Several shipped mono themes overstate the partial tail by one sample.
        // Decode only the samples represented by bytes; never synthesize a missing nibble.
        if (frames > available) {
            if (result.channels == 1 && remainder >= 7 && frames == available + 1)
                frames = static_cast<std::uint32_t>(available);
            else
                return Result::invalid_argument;
        }
        // Bound decoded allocation to 64 MiB; sufficient for the original fallback song.
        if (static_cast<std::uint64_t>(frames) * result.channels * 2 > 64 * 1024 * 1024)
            return Result::invalid_argument;
        result.bits_per_sample = 16;
        result.data.reserve(static_cast<std::size_t>(frames) * result.channels * 2);
        struct Channel {
            std::array<std::int16_t, 2> coefficient;
            std::int64_t delta{}, previous{}, older{};
        };
        constexpr std::array<int, 16> adaptation{230, 230, 230, 230, 307, 409, 512, 614,
                                                 768, 614, 512, 409, 307, 230, 230, 230};
        const auto emit = [&](std::int64_t value) {
            if (result.data.size() >= static_cast<std::size_t>(frames) * result.channels * 2)
                return;
            const auto sample = static_cast<std::uint16_t>(value);
            result.data.push_back(std::byte(sample & 255));
            result.data.push_back(std::byte(sample >> 8));
        };
        const auto expand = [&](Channel& channel, unsigned nibble) {
            const auto residual =
                nibble < 8 ? static_cast<int>(nibble) : static_cast<int>(nibble) - 16;
            // Microsoft ACM uses an arithmetic shift, including negative predictions.
            const auto prediction = (channel.previous * channel.coefficient[0] +
                                     channel.older * channel.coefficient[1]) >>
                                    8;
            const auto value = std::clamp(prediction + residual * channel.delta,
                                          std::int64_t{-32768}, std::int64_t{32767});
            channel.older = channel.previous;
            channel.previous = value;
            channel.delta = std::max(std::int64_t{16}, channel.delta * adaptation[nibble] / 256);
            // Prevent corrupt blocks from overflowing arithmetic after repeated large steps.
            if (channel.delta > 0x10000000)
                throw std::runtime_error("Invalid ADPCM step");
            emit(value);
        };
        for (std::size_t offset = 0; offset < data.size(); offset += alignment) {
            BinaryReader block(
                data.subspan(offset, std::min<std::size_t>(alignment, data.size() - offset)));
            std::array<Channel, 2> channels{};
            for (unsigned c = 0; c < result.channels; ++c) {
                const auto predictor = std::to_integer<unsigned>(block.take(1)[0]);
                if (predictor >= coefficients.size())
                    return Result::invalid_argument;
                channels[c].coefficient = coefficients[predictor];
            }
            for (unsigned c = 0; c < result.channels; ++c) {
                channels[c].delta = std::bit_cast<std::int16_t>(block.u16());
                if (channels[c].delta < 16)
                    return Result::invalid_argument;
            }
            for (unsigned c = 0; c < result.channels; ++c)
                channels[c].previous = std::bit_cast<std::int16_t>(block.u16());
            for (unsigned c = 0; c < result.channels; ++c)
                channels[c].older = std::bit_cast<std::int16_t>(block.u16());
            for (unsigned c = 0; c < result.channels; ++c)
                emit(channels[c].older);
            for (unsigned c = 0; c < result.channels; ++c)
                emit(channels[c].previous);
            while (block.remaining()) {
                const auto packed = std::to_integer<unsigned>(block.take(1)[0]);
                expand(channels[0], packed >> 4);
                expand(channels[result.channels - 1], packed & 15);
            }
        }
        output = std::move(result);
        return Result::ok;
    } catch (const std::exception&) {
        return Result::invalid_argument;
    }
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00462cbc
Result RaceMusic::load(ResourceStore& resources, std::string& error) {
    return load_track(resources, "Sfx/RaceMusic.wav", "INGAMEMUSIC", error);
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00462bed
Result RaceMusic::load_menu(ResourceStore& resources, std::string& error) {
    return load_track(resources, "Sfx/SoundBackgroundMusic.wav", "MUSIC", error);
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0046305d
Result RaceMusic::load_character(ResourceStore& resources, std::string_view character,
                                 std::string& error) {
    if (character.empty() || character.find_first_of("/\\:\r\n") != std::string_view::npos ||
        character.find("..") != std::string_view::npos)
        return Result::invalid_argument;
    return load_track(resources, "Characters/" + std::string(character) + "/ThemeMusic.wav",
                      "MUSIC", error);
}
EngineAudioCommand RaceMusic::gain(float scale) const {
    EngineAudioCommand command;
    command.action = EngineVoiceAction::update;
    command.set_volume = true;
    command.qmdx_volume = static_cast<std::int32_t>(std::clamp(scale, 0.f, 1.f) * volume_ * 32767);
    command.linear_gain = static_cast<float>(command.qmdx_volume) / 32767;
    return command;
}
Result RaceMusic::load_track(ResourceStore& resources, std::string_view path,
                             std::string_view volume_key, std::string& error) {
    playing_ = false;
    sample_ = {};
    Configuration config;
    if (config.load(resources, "Ini/RoadWars.ini") != Result::ok ||
        config.number("ROADWARS", volume_key, volume_) != Result::ok || !std::isfinite(volume_) ||
        volume_ < 0 || volume_ > 1) {
        error = "Missing or invalid original music volume: " + std::string(volume_key);
        return Result::invalid_argument;
    }
    // No host CD transport is available, so take the original failed-CD fallback.
    std::vector<std::byte> bytes;
    auto status = resources.read(path, bytes);
    if (status == Result::ok) {
        status = decode_wave_pcm(bytes, sample_);
        if (status == Result::unavailable)
            status = decode_ms_adpcm_wave(bytes, sample_);
    }
    if (status != Result::ok)
        error = "Original music unavailable or invalid: " + std::string(path);
    return status;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00480b40
Result RaceMusic::start(EngineAudioCommand& command) {
    command = {};
    if (!sample_.frame_count())
        return Result::unavailable;
    if (playing_)
        return Result::ok;
    command.action = EngineVoiceAction::start;
    command.loop = command.set_volume = command.set_frequency = true;
    command.frequency_hz = sample_.sample_rate;
    command.qmdx_volume = static_cast<std::int32_t>(volume_ * 32767);
    command.linear_gain = static_cast<float>(command.qmdx_volume) / 32767;
    command.pitch_scale = 1;
    playing_ = true;
    return Result::ok;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00462ab9
EngineAudioCommand RaceMusic::stop() {
    EngineAudioCommand command;
    if (playing_)
        command.action = EngineVoiceAction::stop;
    playing_ = false;
    return command;
}
} // namespace roadwars
