#include "game_src/unresolved/engine_audio.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace roadwars;
namespace {
void require(bool condition) {
    if (!condition)
        throw std::runtime_error("Recovered engine audio contract failed");
}
void u32(std::vector<std::byte>& out, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8)
        out.push_back(std::byte((value >> shift) & 255));
}
void tag(std::vector<std::byte>& out, std::string_view value) {
    for (unsigned char character : value)
        out.push_back(std::byte(character));
}
std::vector<std::byte> wave_fixture() {
    std::vector<std::byte> bytes;
    tag(bytes, "RIFF");
    u32(bytes, 40);
    tag(bytes, "WAVE");
    tag(bytes, "fmt ");
    u32(bytes, 16);
    u32(bytes, 0x10001);
    u32(bytes, 22050);
    u32(bytes, 44100);
    u32(bytes, 0x100002);
    tag(bytes, "data");
    u32(bytes, 4);
    bytes.insert(bytes.end(), {std::byte{0}, std::byte{0}, std::byte{255}, std::byte{127}});
    return bytes;
}
class DirectoryReader final : public ResourceReader {
  public:
    explicit DirectoryReader(std::filesystem::path root) : root_(std::move(root)) {}
    Result read(std::string_view path, std::vector<std::byte>& output) override {
        std::ifstream file(root_ / std::filesystem::path(path), std::ios::binary | std::ios::ate);
        if (!file)
            return Result::not_found;
        const auto length = file.tellg();
        if (length < 0)
            return Result::invalid_argument;
        output.resize(static_cast<std::size_t>(length));
        file.seekg(0);
        file.read(reinterpret_cast<char*>(output.data()), length);
        return file ? Result::ok : Result::invalid_argument;
    }

  private:
    std::filesystem::path root_;
};
} // namespace
int main(int argc, char** argv) {
    auto bytes = wave_fixture();
    WavePcm sample;
    require(decode_wave_pcm(bytes, sample) == Result::ok);
    require(sample.sample_rate == 22050 && sample.channels == 1 && sample.bits_per_sample == 16 &&
            sample.frame_count() == 2);
    bytes.pop_back();
    require(decode_wave_pcm(bytes, sample) == Result::invalid_argument &&
            sample.frame_count() == 2);
    bytes = wave_fixture();
    bytes[20] = std::byte{3};
    require(decode_wave_pcm(bytes, sample) == Result::unavailable);
    bytes = wave_fixture();
    bytes[32] = std::byte{4};
    require(decode_wave_pcm(bytes, sample) == Result::invalid_argument);
    require(original_engine_frequency(0, false) == 1033);
    require(original_engine_frequency(0, true) == 2083);
    require(original_engine_frequency(3000, false) == 15533);
    require(original_engine_frequency(3000, true) == 18083);
    require(original_engine_frequency(20000, true) == 73333);
    require(engine_audio_audible({0, 0, 0}, {30, 1000, 44}));
    require(!engine_audio_audible({0, 0, 0}, {30, 0, 45}));
    EngineAudio engine;
    require(engine.initialize(22050, .9f) == Result::ok);
    EngineAudioCommand command;
    require(engine.update(0, 0, {1, 2, 3}, {0, 0, 0}, true, command) == Result::ok);
    require(command.action == EngineVoiceAction::start && command.loop &&
            command.frequency_hz == 22050);
    require(engine.update(.01f, 0, {1, 2, 3}, {0, 0, 0}, true, command) == Result::ok);
    require(command.action == EngineVoiceAction::update && command.frequency_hz == 1033 &&
            command.qmdx_volume == 22117);
    require(std::abs(command.pitch_scale - 1033.0f / 22050) < .00001f);
    require(engine.update(.025f, 3000, {1, 2, 3}, {0, 0, 200}, true, command, true) == Result::ok);
    require(command.action == EngineVoiceAction::none); // Strictly greater than 25ms.
    require(engine.update(.001f, 3000, {1, 2, 3}, {0, 0, 200}, true, command, true, 200) ==
            Result::ok);
    require(command.frequency_hz == 18083 && command.set_volume &&
            std::abs(command.velocity.z - 111.75f) < .0001f);
    require(engine.update(.1f, 3000, {1, 2, 3}, {}, false, command) == Result::ok);
    require(command.action == EngineVoiceAction::stop && !engine.playing());
    require(engine.update(.1f, 3000, {1, 2, 3}, {}, false, command) == Result::ok &&
            command.action == EngineVoiceAction::none);
    require(engine.update(.01f, 3000, {1, 2, 3}, {}, true, command) == Result::ok &&
            command.action == EngineVoiceAction::start);
    engine.reset();
    require(!engine.playing());
    if (argc == 2) {
        DirectoryReader reader(argv[1]);
        ResourceStore resources(reader);
        EngineAudioResources original;
        std::string error;
        require(load_engine_audio(resources, "Animal", original, error) == Result::ok);
        require(original.sample.sample_rate == 22050 && original.sample.channels == 1 &&
                original.sample.bits_per_sample == 16 && original.sample.frame_count() == 68173);
        std::cout << "Original engine WAV: " << original.sample.frame_count() << " frames; "
                  << original.sample.sample_rate << " Hz; SFX=" << original.sfx_volume << '\n';
    }
}
