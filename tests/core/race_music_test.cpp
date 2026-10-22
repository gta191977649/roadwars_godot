#include "game_src/unresolved/race_music.hpp"
#include <bit>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace roadwars;
namespace {
void u16(std::vector<std::byte>& bytes, unsigned value) {
    bytes.push_back(std::byte(value & 255));
    bytes.push_back(std::byte((value >> 8) & 255));
}
void u32(std::vector<std::byte>& bytes, unsigned value) {
    u16(bytes, value);
    u16(bytes, value >> 16);
}
void tag(std::vector<std::byte>& bytes, std::string_view value) {
    for (char c : value)
        bytes.push_back(std::byte(c));
}
std::vector<std::byte> fixture(unsigned channels) {
    std::vector<std::byte> bytes;
    tag(bytes, "RIFF");
    u32(bytes, 0);
    tag(bytes, "WAVEfmt ");
    u32(bytes, 26);
    u16(bytes, 2);
    u16(bytes, channels);
    u32(bytes, 22050);
    u32(bytes, 44100);
    u16(bytes, channels * 8);
    u16(bytes, 4);
    u16(bytes, 8);
    u16(bytes, 4);
    u16(bytes, 1);
    u16(bytes, 256);
    u16(bytes, 0);
    tag(bytes, "fact");
    u32(bytes, 4);
    u32(bytes, 4);
    tag(bytes, "data");
    u32(bytes, channels * 8);
    for (unsigned c = 0; c < channels; ++c)
        bytes.push_back(std::byte{});
    for (unsigned c = 0; c < channels; ++c)
        u16(bytes, 16);
    for (unsigned c = 0; c < channels; ++c)
        u16(bytes, c ? -200 : 200);
    for (unsigned c = 0; c < channels; ++c)
        u16(bytes, c ? -100 : 100);
    bytes.push_back(std::byte{0x1f});
    if (channels == 2)
        bytes.push_back(std::byte{0});
    const auto size = static_cast<unsigned>(bytes.size() - 8);
    for (int i = 0; i < 4; ++i)
        bytes[4 + i] = std::byte((size >> (i * 8)) & 255);
    return bytes;
}
std::int16_t sample(const WavePcm& pcm, unsigned i) {
    return std::bit_cast<std::int16_t>(
        static_cast<std::uint16_t>(std::to_integer<unsigned>(pcm.data[i * 2]) |
                                   (std::to_integer<unsigned>(pcm.data[i * 2 + 1]) << 8)));
}
class Files final : public ResourceReader {
  public:
    explicit Files(std::filesystem::path root) : root_(std::move(root)) {}
    Result read(std::string_view path, std::vector<std::byte>& out) override {
        std::ifstream file(root_ / std::filesystem::path(path), std::ios::binary | std::ios::ate);
        if (!file)
            return Result::not_found;
        const auto size = file.tellg();
        if (size < 0)
            return Result::invalid_argument;
        out.resize(static_cast<std::size_t>(size));
        file.seekg(0);
        file.read(reinterpret_cast<char*>(out.data()), size);
        return file ? Result::ok : Result::invalid_argument;
    }

  private:
    std::filesystem::path root_;
};
} // namespace
int main(int argc, char** argv) {
    WavePcm pcm;
    auto mono = fixture(1);
    assert(decode_ms_adpcm_wave(mono, pcm) == Result::ok);
    assert(pcm.channels == 1 && pcm.frame_count() == 4);
    assert(sample(pcm, 0) == 100 && sample(pcm, 1) == 200);
    assert(sample(pcm, 2) == 216 && sample(pcm, 3) == 200);
    auto negative = fixture(1);
    negative[42] = std::byte{128}; // Predictor coefficient 128/256.
    negative[43] = std::byte{};
    negative[negative.size() - 5] = std::byte{55}; // sample1 = -201 (0xff37).
    negative[negative.size() - 4] = std::byte{255};
    assert(decode_ms_adpcm_wave(negative, pcm) == Result::ok);
    assert(sample(pcm, 2) == -85 && sample(pcm, 3) == -59);
    const auto stereo = fixture(2);
    assert(decode_ms_adpcm_wave(stereo, pcm) == Result::ok);
    assert(pcm.channels == 2 && pcm.frame_count() == 4);
    assert(sample(pcm, 0) == 100 && sample(pcm, 1) == -100);
    assert(sample(pcm, 4) == 216 && sample(pcm, 5) == -216);
    assert(sample(pcm, 6) == 216 && sample(pcm, 7) == -216);
    mono[mono.size() - 8] = std::byte{1};
    assert(decode_ms_adpcm_wave(mono, pcm) == Result::invalid_argument);
    mono = fixture(1);
    mono.pop_back();
    assert(decode_ms_adpcm_wave(mono, pcm) == Result::invalid_argument);
    auto partial = fixture(1);
    partial[32] = std::byte{16}; // A short final block of a 16-byte-alignment stream.
    partial[38] = std::byte{20};
    partial[54] = std::byte{5}; // Shipped theme metadata has one unavailable tail sample.
    assert(decode_ms_adpcm_wave(partial, pcm) == Result::ok && pcm.frame_count() == 4);
    partial[54] = std::byte{6};
    assert(decode_ms_adpcm_wave(partial, pcm) == Result::invalid_argument);
    RaceMusic music;
    EngineAudioCommand command;
    assert(music.gain(0).action == EngineVoiceAction::update);
    assert(music.gain(1).action == EngineVoiceAction::update);
    assert(music.start(command) == Result::unavailable);
    if (argc < 2)
        return 0;
    Files files(argv[1]);
    ResourceStore store(files);
    assert(store.open() == Result::ok);
    std::string error;
    if (music.load(store, error) != Result::ok) {
        std::cerr << error << '\n';
        return 1;
    }
    assert(music.sample().sample_rate == 22050 && music.sample().channels == 1);
    assert(music.sample().frame_count() == 1857152);
    assert(music.start(command) == Result::ok);
    assert(command.action == EngineVoiceAction::start && command.loop);
    assert(command.qmdx_volume == 19660 && command.pitch_scale == 1);
    assert(music.start(command) == Result::ok && command.action == EngineVoiceAction::none);
    assert(music.stop().action == EngineVoiceAction::stop);
    assert(music.stop().action == EngineVoiceAction::none);
    if (argc > 2) {
        std::ofstream file(argv[2], std::ios::binary);
        file.write(reinterpret_cast<const char*>(music.sample().data.data()),
                   music.sample().data.size());
        assert(file.good());
    }
    RaceMusic menu;
    assert(menu.load_menu(store, error) == Result::ok);
    assert(menu.sample().frame_count() > 0);
    assert(menu.start(command) == Result::ok && command.loop);
    assert(command.qmdx_volume == 26213);
    assert(menu.start(command) == Result::ok && command.action == EngineVoiceAction::none);
    assert(menu.stop().action == EngineVoiceAction::stop);
    Configuration characters;
    assert(characters.load(store, "Screens/SelectCharacter/Data.ini") == Result::ok);
    float count{};
    assert(characters.number("INFO", "NUMSELECTIONS", count) == Result::ok);
    for (int index = 0; index < static_cast<int>(count); ++index) {
        const auto name = characters.value("SELECTION" + std::to_string(index), "VALUE");
        RaceMusic theme;
        assert(theme.load_character(store, name, error) == Result::ok &&
               theme.sample().frame_count() > 0);
        assert(theme.start(command) == Result::ok && command.loop);
        assert(theme.gain(0).linear_gain == 0 && theme.gain(1).qmdx_volume == 26213);
    }
    std::cout
        << "ROADWARS_MUSIC_CORE_OK: original fallback decoded, 1857152 frames, loop/volume/stop\n";
}
