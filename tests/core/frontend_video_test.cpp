#include "game_src/unresolved/frontend_video.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>

using namespace roadwars;
namespace {
using Bytes = std::vector<std::byte>;
void number(Bytes& bytes, std::uint32_t value, unsigned count, bool big = false) {
    for (unsigned i = 0; i < count; ++i)
        bytes.push_back(static_cast<std::byte>(value >> (8 * (big ? count - i - 1 : i))));
}
void text(Bytes& bytes, std::string_view value) {
    for (unsigned char c : value)
        bytes.push_back(static_cast<std::byte>(c));
}
void append(Bytes& target, const Bytes& source) {
    target.insert(target.end(), source.begin(), source.end());
}
Bytes riff_chunk(std::string_view id, const Bytes& data) {
    Bytes result;
    text(result, id);
    number(result, static_cast<std::uint32_t>(data.size()), 4);
    append(result, data);
    if (data.size() & 1)
        result.push_back(std::byte{});
    return result;
}
Bytes cinepak(bool second) {
    Bytes payload;
    // A single full V1 codebook vector: four Y values, neutral chroma.
    number(payload, 0x22, 1);
    number(payload, 10, 3, true);
    for (unsigned y : {10u, 20u, 30u, 40u, 0u, 0u})
        number(payload, y + (second && y ? 1 : 0), 1);
    number(payload, 0x32, 1);
    number(payload, 5, 3, true);
    number(payload, 0, 1);
    Bytes strip;
    number(strip, 0x10, 1);
    number(strip, static_cast<std::uint32_t>(12 + payload.size()), 3, true);
    for (unsigned value : {0u, 0u, 4u, 4u})
        number(strip, value, 2, true);
    append(strip, payload);
    Bytes frame;
    number(frame, 0, 1);
    number(frame, static_cast<std::uint32_t>(10 + strip.size()), 3, true);
    for (unsigned value : {4u, 4u, 1u})
        number(frame, value, 2, true);
    append(frame, strip);
    return frame;
}
Bytes avi() {
    Bytes header;
    text(header, "vidscvid");
    header.resize(20);
    number(header, 1, 4);
    number(header, 2, 4);
    header.resize(56);
    Bytes format;
    number(format, 40, 4);
    number(format, 4, 4);
    number(format, 4, 4);
    number(format, 1, 2);
    number(format, 24, 2);
    text(format, "cvid");
    format.resize(40);
    Bytes stream;
    text(stream, "strl");
    append(stream, riff_chunk("strh", header));
    append(stream, riff_chunk("strf", format));
    Bytes hdrl;
    text(hdrl, "hdrl");
    append(hdrl, riff_chunk("LIST", stream));
    Bytes movie;
    text(movie, "movi");
    append(movie, riff_chunk("00dc", cinepak(false)));
    append(movie, riff_chunk("00dc", cinepak(true)));
    Bytes contents;
    text(contents, "AVI ");
    append(contents, riff_chunk("LIST", hdrl));
    append(contents, riff_chunk("LIST", movie));
    return riff_chunk("RIFF", contents);
}
Bytes file_bytes(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    assert(file);
    const auto size = file.tellg();
    assert(size >= 0);
    Bytes result(static_cast<std::size_t>(size));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(result.data()), size);
    assert(file);
    return result;
}
} // namespace
int main(int argc, char** argv) {
    FrontendVideo video;
    std::string error;
    Bitmap frame;
    assert(video.frame_at(0, frame) == Result::unavailable);
    const auto fixture = avi();
    assert(video.open(fixture, error) == Result::ok);
    assert(video.frame_count() == 2 && video.width() == 4 && video.height() == 4);
    assert(video.duration() == 1);
    assert(video.frame_at(0, frame) == Result::ok);
    assert(frame.rgba[0] == std::byte{10});
    assert(frame.rgba[8] == std::byte{20});
    assert(frame.rgba[32] == std::byte{30});
    assert(frame.rgba[40] == std::byte{40});
    assert(frame.rgba[3] == std::byte{255});
    const auto first = frame.rgba;
    assert(video.frame_at(0.5, frame) == Result::ok && frame.rgba[0] == std::byte{11});
    assert(video.frame_at(100, frame) == Result::ok && frame.rgba[0] == std::byte{11});
    assert(video.frame_at(0, frame) == Result::ok && frame.rgba == first);
    assert(video.frame_at(-1, frame) == Result::invalid_argument);
    assert(video.frame_at(std::numeric_limits<double>::quiet_NaN(), frame) ==
           Result::invalid_argument);
    // All truncations must fail without committing a partially opened movie.
    for (std::size_t n = 0; n < fixture.size(); ++n) {
        assert(video.open(std::span(fixture).first(n), error) == Result::invalid_argument);
        assert(video.frame_count() == 0);
    }
    auto damaged_packet = fixture;
    Bytes packet_id;
    text(packet_id, "00dc");
    const auto first_packet = std::search(damaged_packet.begin(), damaged_packet.end(),
                                          packet_id.begin(), packet_id.end());
    assert(first_packet != damaged_packet.end());
    // The RIFF structure is valid; the inner big-endian Cinepak frame length is not.
    *(first_packet + 9) = std::byte{255};
    assert(video.open(damaged_packet, error) == Result::ok);
    const auto unchanged = frame.rgba;
    assert(video.frame_at(0, frame) == Result::invalid_argument);
    assert(frame.rgba == unchanged);
    if (argc > 1) {
        for (const auto name : {"Avi1.avi", "Avi2.avi"}) {
            const auto bytes =
                file_bytes(std::filesystem::path(argv[1]) / "Screens" / "Publisher Intro" / name);
            assert(video.open(bytes, error) == Result::ok);
            assert(video.width() == 320 && video.height() == 240);
            assert(video.audio().format_tag == 0x31 && video.audio().channels == 1 &&
                   video.audio().sample_rate == 22050 && !video.audio().encoded.empty());
            for (std::size_t i = 0; i < video.frame_count(); ++i) {
                const auto result = video.frame_at(static_cast<double>(i) / 15.0 + 0.000001, frame);
                if (result != Result::ok) {
                    std::cerr << name << " invalid frame " << i << '\n';
                    return 1;
                }
                assert(frame.rgba.size() == 320 * 240 * 4);
                for (std::size_t pixel = 3; pixel < frame.rgba.size(); pixel += 4)
                    assert(frame.rgba[pixel] == std::byte{255});
            }
            const auto last = frame.rgba;
            assert(video.frame_at(0, frame) == Result::ok);
            assert(video.frame_at(video.duration(), frame) == Result::ok && frame.rgba == last);
            std::cout << name << ": " << video.frame_count() << " frames, " << video.duration()
                      << " seconds; every frame decoded and reverse seek reproduced final frame\n";
        }
    }
    std::cout << "Frontend video tests passed\n";
}
