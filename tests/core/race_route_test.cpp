#include "game_src/unresolved/race_route.hpp"
#include <bit>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace roadwars;
namespace {
void u32(std::vector<std::byte>& output, std::uint32_t value) {
    for (int i = 0; i < 4; ++i)
        output.push_back(static_cast<std::byte>((value >> (i * 8)) & 255));
}
void f32(std::vector<std::byte>& output, float value) {
    u32(output, std::bit_cast<std::uint32_t>(value));
}
void vector(std::vector<std::byte>& output, Vec3 point) {
    f32(output, point.x);
    f32(output, point.y);
    f32(output, point.z);
}
Vec3 center(int i) {
    const float angle = static_cast<float>(i) * 6.28318530718f / 12;
    return {std::sin(angle) * 100, 0, std::cos(angle) * 100};
}
Vec3 crossing(int i, float amount) {
    const auto p = center(i), before = center((i + 11) % 12), after = center((i + 1) % 12);
    return {p.x + (after.x - before.x) * amount, 0, p.z + (after.z - before.z) * amount};
}
std::vector<std::byte> fixture(int version = 4, bool branch = false) {
    std::vector<std::byte> bytes;
    u32(bytes, version);
    u32(bytes, 12);
    for (int index = 0; index < 12; ++index) {
        const auto p = center(index);
        auto point = [&](bool alternate) {
            if (version != 4)
                bytes.resize(bytes.size() + 32);
            u32(bytes, index + 1);
            const Vec3 lateral{p.x / 100, 0, p.z / 100};
            vector(bytes, {p.x - lateral.x * 10, 0, p.z - lateral.z * 10});
            vector(bytes, {p.x + lateral.x * 10, 0, p.z + lateral.z * 10});
            vector(bytes, p);
            f32(bytes, 10);
            vector(bytes, lateral);
            f32(bytes, 1);
            f32(bytes, 2);
            f32(bytes, 4);
            f32(bytes, -4);
            f32(bytes, 20);
            u32(bytes, 0);
            u32(bytes, 0);
            u32(bytes, 0);
            f32(bytes, static_cast<float>(index) * 51.763809f);
            u32(bytes, alternate ? 2 : 0);
            u32(bytes, 0xdeadbeef); // A serialized pointer is ignored, never dereferenced.
        };
        point(false);
        u32(bytes, branch && index == 3 ? 1 : 0);
        if (branch && index == 3)
            point(true);
    }
    return bytes;
}
class Files final : public ResourceReader {
  public:
    explicit Files(std::filesystem::path root) : root_(std::move(root)) {}
    Result read(std::string_view path, std::vector<std::byte>& output) override {
        std::ifstream stream(root_ / std::filesystem::path(path), std::ios::binary | std::ios::ate);
        if (!stream)
            return Result::not_found;
        const auto size = stream.tellg();
        if (size < 0)
            return Result::invalid_argument;
        output.resize(static_cast<std::size_t>(size));
        stream.seekg(0);
        stream.read(reinterpret_cast<char*>(output.data()), size);
        return stream ? Result::ok : Result::invalid_argument;
    }

  private:
    std::filesystem::path root_;
};
} // namespace
int main(int argc, char** argv) {
    RaceRoute route;
    std::string error;
    const auto bytes = fixture();
    for (std::size_t size = 0; size < bytes.size(); ++size)
        assert(route.decode(std::span(bytes).first(size), error) != Result::ok);
    auto trailing = bytes;
    trailing.push_back(std::byte{});
    assert(route.decode(trailing, error) != Result::ok);
    assert(route.decode(fixture(5), error) != Result::ok);
    assert(route.decode(fixture(3, true), error) == Result::ok);
    assert(route.nodes()[3].branch && route.nodes()[3].branch->branch_kind == 2);
    assert(route.decode(bytes, error) == Result::ok);
    assert(route.nodes().size() == 12 && route.wrap(-1) == 11 && route.wrap(25) == 1);
    assert((route.checkpoints() == std::array<int, 5>{0, 1, 2, 4, 5}));
    float offset = 100;
    bool branch_latch = false;
    auto sampled = route.sample(0, 0, false, -1, 0, offset, branch_latch);
    assert(offset == 3 && std::abs(sampled.z - 104) < .001f);
    const auto rescue = route.respawn(0, 0);
    assert(std::abs(rescue.z - 101) < .001f);
    RaceRouteProgress progress;
    assert(route.initialize_progress(progress, crossing(0, -.1f), error) == Result::ok);
    assert(progress.current_node == 11);
    assert(route.initialize_progress(progress, {0, 0, 0}, error) != Result::ok);
    assert(route.initialize_progress(progress, crossing(0, -.1f), error) == Result::ok);
    for (int lap = 0; lap < 2; ++lap) {
        for (int node = 0; node < 12; ++node) {
            assert(route.update(progress, crossing(node, -.01f), crossing(node, .01f), 2, error) ==
                   Result::ok);
            assert(progress.current_node == node);
            assert(progress.completed_laps == lap);
        }
    }
    assert(route.update(progress, crossing(0, -.01f), crossing(0, .01f), 2, error) == Result::ok);
    assert(progress.completed_laps == 2 && progress.finished);
    const auto laps = progress.completed_laps;
    assert(route.update(progress, crossing(0, -.01f), crossing(0, .01f), 2, error) == Result::ok);
    assert(progress.completed_laps == laps);
    progress = {};
    progress.current_node = 11;
    assert(route.update(progress, crossing(0, -.01f), crossing(0, .01f), 1, error) == Result::ok);
    assert(progress.completed_laps == 0 && progress.checkpoints[0]);
    progress.checkpoints = {true, true, true, true, true};
    progress.current_node = 3;
    assert(route.update(progress, crossing(2, .01f), crossing(2, -.01f), 1, error) == Result::ok);
    assert(progress.current_node == 2 && !progress.checkpoints[3] && !progress.checkpoints[4]);
    assert(route.update(progress, {}, {}, 0, error) != Result::ok);
    assert(route.update(progress, {}, {}, 11, error) != Result::ok);
    progress = {};
    progress.current_node = 0;
    progress.checkpoints[0] = true;
    progress.route_percent = 12;
    assert(route.ranking_progress(progress) == 0);
    progress.checkpoints[4] = true;
    assert(route.ranking_progress(progress) == 12);
    if (argc > 1) {
        Files files(argv[1]);
        ResourceStore resources(files);
        assert(resources.open() == Result::ok);
        int tested{};
        for (const auto& [name, entry] : resources.entries()) {
            (void)entry;
            if (!name.starts_with("TRACKS/") || !name.ends_with("/WORLD/LAP.BIN"))
                continue;
            const auto track =
                name.substr(7, name.size() - 7 - std::string("/WORLD/LAP.BIN").size());
            for (bool reverse : {false, true}) {
                if (route.load(resources, track, error, reverse) != Result::ok) {
                    std::cerr << track << " reverse=" << reverse << ": " << error << '\n';
                    return 1;
                }
                assert(route.nodes().size() >= 6 && route.length() > 0);
                if (!reverse && track == "DARK FOREST") {
                    auto near_line = [&](int index, float fraction) {
                        const auto& p = route.point(index).center;
                        const auto& a = route.point(index - 1).center;
                        const auto& b = route.point(index + 1).center;
                        return Vec3{p.x + (b.x - a.x) * fraction, 0, p.z + (b.z - a.z) * fraction};
                    };
                    RaceRouteProgress actual;
                    assert(route.initialize_progress(actual, near_line(0, -.01f), error) ==
                           Result::ok);
                    assert(actual.current_node == static_cast<int>(route.nodes().size()) - 1);
                    for (int index = 0; index <= static_cast<int>(route.nodes().size()); ++index) {
                        assert(route.update(actual, near_line(index, -.001f),
                                            near_line(index, .001f), 1, error) == Result::ok);
                        assert(actual.current_node == route.wrap(index));
                    }
                    assert(actual.completed_laps == 1 && actual.finished);
                }
                std::cout << "ROUTE_ASSET_OK " << track << " reverse=" << reverse
                          << " nodes=" << route.nodes().size() << " length=" << route.length()
                          << '\n';
                ++tested;
            }
        }
        assert(tested > 1);
    }
    std::cout << "RACE_ROUTE_OK\n";
}
