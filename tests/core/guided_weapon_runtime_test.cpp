#include "game_src/unresolved/guided_weapon_runtime.hpp"
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>
using namespace roadwars;
namespace {
void require(bool value) {
    if (!value)
        throw std::runtime_error("Original guided weapon contract mismatch");
}
void u32(std::vector<std::byte>& out, std::uint32_t v) {
    for (int i = 0; i < 4; ++i)
        out.push_back(static_cast<std::byte>((v >> (i * 8)) & 255));
}
void f32(std::vector<std::byte>& out, float v) { u32(out, std::bit_cast<std::uint32_t>(v)); }
void vec(std::vector<std::byte>& out, Vec3 v) {
    f32(out, v.x);
    f32(out, v.y);
    f32(out, v.z);
}
std::vector<std::byte> route_fixture() {
    std::vector<std::byte> out;
    u32(out, 4);
    u32(out, 6);
    for (int i = 0; i < 6; ++i) {
        const float z = static_cast<float>(i) * 10;
        u32(out, static_cast<unsigned>(i + 1));
        vec(out, {-10, 0, z});
        vec(out, {10, 0, z});
        vec(out, {0, 0, z});
        f32(out, 10);
        vec(out, {1, 0, 0});
        f32(out, 0);
        f32(out, 0);
        f32(out, 5);
        f32(out, -5);
        f32(out, 0);
        u32(out, 0);
        u32(out, 0);
        u32(out, 0);
        f32(out, z);
        u32(out, 0);
        u32(out, 0);
        u32(out, 0);
    }
    return out;
}
} // namespace
int main() {
    WeaponParameters p;
    p.ammunition_count = 1;
    p.acceleration = 90;
    p.top_speed = 200;
    p.rotational_speed = 360;
    GuidedWeaponRuntime guided;
    std::string error;
    require(guided.initialize("Guided Missile", p, {2, 3}, error) == Result::ok);
    bool launched{};
    require(guided.launch({{0, 10, 0}, 0, 0, 40}, launched, error) == Result::ok && launched);
    require(guided.active() && !guided.ready() && guided.missiles()[0].heading == 10);
    require(guided.launch({{}, 0, 0, 40}, launched, error) == Result::ok && !launched);
    unsigned draws{}, ground_queries{};
    GuidedWeaponServices services;
    services.random = [&] {
        ++draws;
        return 1u;
    };
    services.ground = [&](Vec3 query, float& height) {
        ++ground_queries;
        require(query.y >= 5);
        height = 0;
        return Result::ok;
    };
    std::array<std::size_t, 1> particles{};
    const GuidedWeaponTarget target{{-100, 10, 0}, 1};
    require(guided.begin_update(.6f, target, nullptr, services, error) == Result::ok);
    require(draws == 0 && ground_queries == 0 && guided.missiles()[0].position.y == 10);
    require(std::abs(guided.missiles()[0].flare_roll - 216) < .0001f);
    require(guided.finish_update(particles) == Result::ok);
    require(guided.jam(1) == Result::ok);
    require(guided.begin_update(.1f, target, nullptr, services, error) == Result::ok);
    require(draws == 0 && ground_queries == 1 && guided.missiles()[0].heading == 10);
    require(std::abs(guided.missiles()[0].jam_remaining - .9f) < .0001f);
    require(guided.finish_update(particles) == Result::ok);
    require(guided.jam(.05f) == Result::ok);
    require(guided.begin_update(.1f, target, nullptr, services, error) == Result::ok);
    require(draws == 1 && guided.missiles()[0].heading != 10 &&
            guided.missiles()[0].position.y == .5f);
    require(guided.record_impact(0) == Result::ok);
    particles[0] = 1;
    require(guided.finish_update(particles) == Result::ok && guided.active());
    require(guided.begin_update(.1f, target, nullptr, services, error) == Result::ok);
    particles[0] = 0;
    require(guided.finish_update(particles) == Result::ok && !guided.active() && !guided.ready());
    require(guided.begin_update(.1f, target, nullptr, services, error) == Result::ok);
    require(guided.finish_update(particles) == Result::ok && guided.ready());
    require(!guided.missiles()[0].visible);
    require(guided.record_impact(0) == Result::invalid_argument);
    const auto previous_flare_roll = guided.missiles()[0].flare_roll;
    require(guided.launch({{0, 10, 0}, 0, 0, 40}, launched, error) == Result::ok && launched);
    require(guided.missiles()[0].flare_roll == previous_flare_roll);
    require(guided.initialize("Back Missile", p, {2, 3}, error) == Result::ok);
    require(guided.launch({{0, 10, 0}, 0, 0, 40}, launched, error) == Result::ok && launched);
    require(guided.missiles()[0].heading == 170);
    require(guided.begin_update(.11f, {}, nullptr, services, error) == Result::ok);
    require(guided.finish_update(particles) == Result::ok);
    require(guided.begin_update(.1f, {}, nullptr, services, error) == Result::ok);
    require(guided.missiles()[0].position.y < 1);
    require(guided.finish_update(particles) == Result::ok);
    p.ammunition_count = 4;
    require(guided.initialize("Swarmers", p, {2, 3}, error) == Result::ok);
    require(guided.launch({{0, 10, 0}, 0, 0, 40}, launched, error) == Result::ok && launched);
    require(std::abs(guided.missiles()[0].position.x + 1.5f) < .001f &&
            std::abs(guided.missiles()[1].position.x - 1.5f) < .001f &&
            guided.missiles()[2].position.y == 7 && guided.missiles()[3].position.y == 7);
    std::array<std::size_t, 4> trails{};
    draws = 0;
    require(guided.begin_update(.6f, target, nullptr, services, error) == Result::ok);
    require(guided.finish_update(trails) == Result::ok);
    require(guided.begin_update(.1f, target, nullptr, services, error) == Result::ok);
    require(draws == 8); // One magnitude and one sign draw for each of four live missiles.
    require(guided.finish_update(trails) == Result::ok);
    require(guided.begin_update(4.3f, {}, nullptr, services, error) == Result::ok);
    require(guided.finish_update(trails) == Result::ok && guided.active());
    require(guided.begin_update(.01f, {}, nullptr, services, error) == Result::ok);
    require(guided.finish_update(trails) == Result::ok && !guided.active());
    p.ammunition_count = 1;
    require(guided.initialize("The Excalibur", p, {2, 3}, error) == Result::ok);
    require(guided.launch({{0, 10, 0}, 0, 0, 40}, launched, error) == Result::invalid_argument);
    require(guided.launch({{0, 10, 0}, 0, 0, 40}, launched, error, 0) == Result::ok && launched);
    require(guided.begin_update(.01f, target, nullptr, services, error) == Result::unavailable);
    RaceRoute route;
    require(route.decode(route_fixture(), error) == Result::ok);
    require(guided.begin_update(.01f, GuidedWeaponTarget{{100, 0, 30}, 3}, &route, services,
                                error) == Result::ok);
    require(guided.tracking_target() && guided.tracking_target()->x == 0 &&
            guided.tracking_target()->z == 10);
    require(guided.finish_update(particles) == Result::ok);
    require(guided.begin_update(.01f, GuidedWeaponTarget{{100, 0, 10}, 1}, &route, services,
                                error) == Result::ok);
    require(guided.tracking_target()->x == 100);
    require(guided.finish_update(particles) == Result::ok);
    require(guided.begin_update(20, {}, nullptr, services, error) == Result::ok);
    require(guided.finish_update(particles) == Result::ok && !guided.active());
    require(guided.begin_update(std::numeric_limits<float>::quiet_NaN(), {}, nullptr, services,
                                error) == Result::invalid_argument);
    // Native inactive cleanup follows motion and tracking, rather than preceding them.
    require(guided.initialize("Guided Missile", p, {2, 3}, error) == Result::ok);
    require(guided.launch({{0, 10, 0}, 0, 0, 40}, launched, error) == Result::ok);
    require(guided.begin_update(.6f, {}, nullptr, services, error) == Result::ok);
    require(guided.finish_update(particles) == Result::ok);
    guided.cancel();
    const auto hidden_previous = guided.missiles()[0].position;
    const auto hidden_draws = draws;
    require(guided.begin_update(.1f, target, nullptr, services, error) == Result::ok);
    require(draws == hidden_draws + 1 && guided.missiles()[0].position.z != hidden_previous.z);
    require(guided.finish_update(particles) == Result::ok && guided.ready());
    require(!guided.missiles()[0].visible && guided.age() == 0);
}
