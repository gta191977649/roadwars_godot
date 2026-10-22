#include "platform/godot/extension/frontend_video_audio.hpp"
#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace roadwars;
int main(int argc, char** argv) {
    FrontendVideoAudio source;
    WavePcm output;
    std::string error;
    output.data = {std::byte{12}};
    assert(decode_frontend_video_audio(source, output, error) == Result::invalid_argument);
    assert(output.data == std::vector<std::byte>{std::byte{12}});
    source.format_tag = 0x31;
    source.channels = 1;
    source.sample_rate = 22050;
    source.block_alignment = 65;
    source.encoded.resize(64);
    assert(decode_frontend_video_audio(source, output, error) == Result::invalid_argument);
    if (argc > 1) {
        for (const auto name : {"Avi1.avi", "Avi2.avi"}) {
            std::ifstream file(std::filesystem::path(argv[1]) / "Screens" / "Publisher Intro" /
                                   name,
                               std::ios::binary | std::ios::ate);
            assert(file);
            const auto size = file.tellg();
            assert(size > 0);
            std::vector<std::byte> bytes(static_cast<std::size_t>(size));
            file.seekg(0);
            file.read(reinterpret_cast<char*>(bytes.data()), size);
            assert(file);
            FrontendVideo video;
            assert(video.open(bytes, error) == Result::ok);
#ifdef _WIN32
            assert(decode_frontend_video_audio(video.audio(), output, error) == Result::ok);
            assert(output.channels == 1 && output.bits_per_sample == 16 &&
                   output.sample_rate == 22050);
            assert(output.data.size() == video.audio().encoded.size() / 65 * 640);
            int peak = 0;
            for (std::size_t i = 0; i < output.data.size(); i += 2) {
                const auto value = std::to_integer<int>(output.data[i]) |
                                   (std::to_integer<int>(output.data[i + 1]) << 8);
                const int signed_value = value < 32768 ? value : value - 65536;
                peak = std::max(peak, std::abs(signed_value));
            }
            assert(peak > 1000 && peak <= 32768);
            WavePcm repeat;
            assert(decode_frontend_video_audio(video.audio(), repeat, error) == Result::ok);
            assert(repeat.data == output.data);
            std::cerr << name << ": " << output.data.size() / 2 << " PCM frames, peak " << peak
                      << "; repeat decode matched\n";
#else
            assert(decode_frontend_video_audio(video.audio(), output, error) ==
                   Result::unavailable);
#endif
        }
    }
    std::cerr << "Frontend video audio tests passed\n";
}
