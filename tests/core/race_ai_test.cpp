#include "game_src/unresolved/race_ai.hpp"
#include <bit>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>

using namespace roadwars;
namespace {
void word(std::vector<std::byte>& data, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i)
        data.push_back(static_cast<std::byte>(value >> (8 * i)));
}
void number(std::vector<std::byte>& data, float value) {
    word(data, std::bit_cast<std::uint32_t>(value));
}
void vector(std::vector<std::byte>& data, Vec3 value) {
    number(data, value.x);
    number(data, value.y);
    number(data, value.z);
}
RaceRoute fixture(float height = 0) {
    std::vector<std::byte> bytes;
    word(bytes, 4);
    word(bytes, 8);
    for (int i = 0; i < 8; ++i) {
        word(bytes, static_cast<unsigned>(i + 1));
        vector(bytes, {-10, height, static_cast<float>(i * 10)});
        vector(bytes, {10, height, static_cast<float>(i * 10)});
        vector(bytes, {0, height, static_cast<float>(i * 10)});
        number(bytes, 10);
        vector(bytes, {1, 0, 0});
        number(bytes, 0);
        number(bytes, 0);
        number(bytes, 10);
        number(bytes, -10);
        number(bytes, 20);
        word(bytes, 0);
        word(bytes, 0);
        word(bytes, 0);
        number(bytes, static_cast<float>(i * 10));
        word(bytes, 0);
        word(bytes, 0);
        word(bytes, 0); // No branch record for this point.
    }
    RaceRoute route;
    std::string error;
    assert(route.decode(bytes, error) == Result::ok);
    return route;
}
class Files : public ResourceReader {
  public:
    explicit Files(std::filesystem::path path) : root(std::move(path)) {}
    Result read(std::string_view path, std::vector<std::byte>& out) override {
        std::ifstream file(root / std::filesystem::path(path), std::ios::binary | std::ios::ate);
        if (!file)
            return Result::not_found;
        auto count = file.tellg();
        out.resize(static_cast<std::size_t>(count));
        file.seekg(0);
        file.read(reinterpret_cast<char*>(out.data()), count);
        return file ? Result::ok : Result::invalid_argument;
    }
    std::filesystem::path root;
};
bool close(float a, float b, float tolerance = .001f) { return std::abs(a - b) < tolerance; }
} // namespace
int main(int argc, char** argv) {
    assert(close(race_ai_heading_delta(350, {}, {1, 0, 0}), 100, .03f));
    assert(close(race_ai_heading_delta(10, {}, {-1, 0, 0}), -100, .03f));
    assert(race_ai_corner_speed(100, 0, 2) == 10000);
    assert(close(race_ai_corner_speed(10, 30, 2), 19.346f, .01f));
    assert(race_ai_braking_speed(100, 10, 1) == 10); // Negative radicand branch.
    assert(close(race_ai_braking_speed(1, 10, 1), 18.9666f));
    assert(race_ai_behavior(100, 100, false, false) == 1);
    assert(race_ai_behavior(50, 100, false, false) == 2);
    assert(race_ai_behavior(99, 100, false, true) == 3);
    assert(race_ai_behavior(100, 100, false, true) == 4);
    assert(race_ai_behavior(100, 100, true, true) == 5);
    assert(race_ai_behavior(100, 100, true, false) == 6);
    auto route = fixture();
    RaceRouteProgress progress;
    std::string error;
    assert(route.initialize_progress(progress, {0, 0, 15}, error) == Result::ok);
    progress.current_node = 1;
    VehicleState vehicle;
    vehicle.position = {0, 0, 15};
    VehicleParameters parameters;
    parameters.max_wheel_angle_degrees = 30;
    RaceAi ai;
    assert(ai.state().behavior == 1);
    assert(ai.update(route, progress, vehicle, parameters, {}, 2, 0, true, 1.0f / 60, error) ==
           Result::ok);
    assert(ai.output().engine_throttle == 1 && ai.output().brake_amount == 0);
    assert(ai.update(route, progress, vehicle, parameters, {}, 2, .25f, true, 1.0f / 60, error) ==
           Result::ok);
    assert(ai.output().target.z >= 20 && ai.output().target_speed <= 20);
    assert(ai.output().engine_throttle == 1 && std::isfinite(ai.output().steering_degrees));
    // The original small-angle curve deliberately maps zero error to +2 degrees.
    assert(close(ai.output().steering_degrees, 2.0f * 7.0f / 60));
    // 0047c820 ignores resource height during lookahead-distance interpolation.
    auto elevated = fixture(-200);
    RaceAi elevated_ai;
    auto elevated_progress = progress;
    assert(elevated_ai.update(elevated, elevated_progress, vehicle, parameters, {}, 2, .25f, true,
                              1.0f / 60, error) == Result::ok);
    assert(elevated_ai.output().target.x == ai.output().target.x &&
           elevated_ai.output().target.z == ai.output().target.z);
    ai.wall_update(1, 1, 1.1f);
    vehicle.cached_speed = 10;
    vehicle.velocity = {0, 0, 10};
    assert(ai.update(route, progress, vehicle, parameters, {}, 2, 1, true, 1.0f / 60, error) ==
           Result::ok);
    assert(ai.output().branch == "Wall recovery" && ai.output().engine_throttle == -1);
    ai.wall_update(0, 1, 2);
    assert(ai.state().recovery_timer == 0);
    RaceAiObstacle obstacle;
    obstacle.position = {0, 0, 20};
    obstacle.footprint = {{{-.5f, 0, 19}, {.5f, 0, 19}, {.5f, 0, 21}, {-.5f, 0, 21}}};
    const std::array obstacles{obstacle};
    assert(ai.update(route, progress, vehicle, parameters, obstacles, 2, 1, true, 1.0f / 60,
                     error) == Result::ok);
    assert(ai.state().front && !ai.state().rear && ai.state().behavior == 6);
    assert(ai.output().brake_amount == .99f);
    assert(ai.update(route, progress, vehicle, parameters, {}, 2, 1, false, 1.0f / 60, error) ==
           Result::ok);
    assert(ai.state().front); // Round-robin refresh preserves unsampled state.
    progress.finished = true;
    assert(ai.update(route, progress, vehicle, parameters, {}, 2, 1, true, 1.0f / 60, error) ==
           Result::ok);
    assert(ai.output().engine_throttle == 0 && ai.output().brake_amount == 0);
    assert(ai.update(route, progress, vehicle, parameters, {}, 2, 1, true, -1, error) ==
           Result::invalid_argument);
    assert(ai.update(route, progress, vehicle, parameters, {},
                     std::numeric_limits<float>::infinity(), 1, true, 0,
                     error) == Result::invalid_argument);
    if (argc > 1) {
        Files files(argv[1]);
        ResourceStore resources(files);
        assert(resources.open() == Result::ok);
        for (const auto track : {"Dark Forest", "RWA Speedway"}) {
            RaceRoute original;
            assert(original.load(resources, track, error) == Result::ok);
            RaceRouteProgress cursor;
            ai.reset();
            for (int node = 0; node < static_cast<int>(original.nodes().size()); ++node) {
                cursor.current_node = node;
                vehicle.position = original.respawn(node, .5f);
                vehicle.heading_degrees =
                    race_ai_heading_delta(0, original.respawn(node), original.respawn(node + 1));
                vehicle.cached_speed = 10;
                assert(ai.update(original, cursor, vehicle, parameters, {}, 2, 1, true, 1.0f / 60,
                                 error) == Result::ok);
                assert(std::isfinite(ai.output().target_speed) &&
                       std::isfinite(ai.output().steering_degrees));
            }
        }
        std::cout << "RACE_AI_ORIGINAL_ASSETS_OK\n";
    }
    std::cout << "Race AI formulas, direct control, recovery and input boundaries passed\n";
}
