#include "game_src/unresolved/special_weapon_effects.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

using namespace roadwars;
namespace {
SpecialWeaponTarget rectangle(std::size_t index, std::uint64_t id, float front, float back,
                              float y = 0) {
    return {index, id, {{{-1, y, front}, {1, y, front}, {1, y, back}, {-1, y, back}}}};
}
void electron_tests() {
    ElectronCannonState state;
    assert(state.range_6d0 == 10 && electron_cannon_can_activate(state));
    begin_electron_cannon(state);
    assert(!electron_cannon_can_activate(state));
    std::array<SpecialWeaponTarget, 3> targets{rectangle(0, 1, 1, 2), rectangle(1, 2, 8, 9, 999),
                                               rectangle(2, 3, 4, 5)};
    ElectronCannonOutput out;
    std::string error;
    assert(advance_electron_cannon(state, 1, {0, 7, 0}, 0, 20, targets, 0, .25f, out, error) ==
           Result::ok);
    assert(out.hit_point && out.hit_point->z == 8 && out.hit_point->y == 7);
    assert(out.beam_length == 8.1f); // First participant, even though another target is nearer.
    assert(out.damage.size() == 4 && out.play_hit_sound);
    for (unsigned i = 0; i < 4; ++i)
        assert(out.damage[i].participant == 1 && out.damage[i].contact_side == i &&
               out.damage[i].damage == 5);
    assert(advance_electron_cannon(state, 1, {}, 0, 20, targets, 0, .25f, out, error) ==
           Result::ok);
    assert(!out.play_hit_sound);
    assert(advance_electron_cannon(state, 1, {}, 0, 20, targets, 0, .25f, out, error) ==
           Result::ok);
    assert(out.play_hit_sound);
    state.elapsed_6b8 = 1.25f;
    assert(advance_electron_cannon(state, 1, {}, 0, 20, {}, 0, .25f, out, error) == Result::ok);
    assert(state.active_270 && !out.hit_point && out.beam_length == 10);
    assert(advance_electron_cannon(state, 1, {}, 0, 20, {}, 0, .01f, out, error) == Result::ok);
    assert(!state.active_270 && out.deactivate && !out.remove_beam);
    assert(advance_electron_cannon(state, 1, {}, 0, 20, {}, 1, .01f, out, error) == Result::ok);
    assert(out.remove_beam && !out.remove_callback && !state.ready_6c0);
    assert(advance_electron_cannon(state, 1, {}, 0, 20, {}, 0, .01f, out, error) == Result::ok);
    assert(!out.remove_beam && out.remove_callback && electron_cannon_can_activate(state));
    assert(advance_electron_cannon(state, 1, {}, 0, 20, {}, 0, -1, out, error) ==
           Result::invalid_argument);
}
void particle_tests() {
    FlameParticlePool pool;
    std::string error;
    unsigned draws{};
    const auto random = [&] {
        ++draws;
        return 49u;
    };
    assert(advance_flame_particles(pool, true, {1, 2, 3}, {0, 0, 10}, .05f, random, error) ==
           Result::ok);
    assert(pool.particles.empty() && draws == 0);
    assert(advance_flame_particles(pool, true, {1, 2, 3}, {0, 0, 10}, .05f, random, error) ==
           Result::ok);
    assert(pool.particles.size() == 1 && draws == 3); // Two creation draws plus one animation.
    assert(pool.particles[0].remaining == .95f && pool.particles[0].position.z == 3);
    assert(pool.particles[0].hue == .49f && pool.particles[0].atlas_frame == 1);
    const auto alpha = static_cast<unsigned>(.95f * 255);
    const auto shade = static_cast<unsigned>(.99f * 255);
    assert(pool.particles[0].color_argb == (alpha << 24 | 0xff0000u | shade << 8 | shade));
    assert(advance_flame_particles(pool, true, {4, 5, 6}, {0, 0, 10}, .01f, random, error) ==
           Result::ok);
    assert(pool.particles.size() == 2 && draws == 7); // Emission timer is not reset.
    assert(pool.particles[0].position.z == 6 && pool.particles[1].position.z == 3.1f);
    assert(advance_flame_particles(pool, false, {}, {}, 2, {}, error) == Result::ok);
    assert(pool.particles.empty()); // All die before any animation random is needed.
    assert(advance_flame_particles(pool, true, {}, {}, .1f, {}, error) == Result::unavailable);
    pool = {};
    for (unsigned i = 0; i < 110; ++i)
        assert(advance_flame_particles(pool, true, {}, {}, .001f, random, error) == Result::ok);
    // The first .1 seconds still elapse before the one-per-callback emission starts.
    assert(pool.particles.size() < 20);
    pool.emission_timer_293c = 1;
    pool.particles.resize(100);
    const auto before = draws;
    assert(advance_flame_particles(pool, true, {}, {}, .001f, random, error) == Result::ok);
    assert(pool.particles.size() == 100 && draws == before + 100); // No creation draws at capacity.
}
void flame_tests() {
    FlameThrowerState state;
    assert(flame_thrower_can_activate(state));
    begin_flame_thrower(state);
    assert(!flame_thrower_can_activate(state));
    // A point close to all four edge midpoints gets four distinct side contacts.
    state.pool.particles.push_back({{0, 999, 0}, {10, 20, 30}, 1, 0, 0, 0});
    const std::array<SpecialWeaponTarget, 2> targets{rectangle(0, 1, -1, 1),
                                                     rectangle(1, 2, -1, 1)};
    FlameThrowerOutput out;
    std::string error;
    unsigned draws{};
    const auto random = [&] {
        ++draws;
        return 0u;
    };
    assert(advance_flame_thrower(state, 1, {}, {}, 100, targets, .01f, random, out, error) ==
           Result::ok);
    assert(out.damage.size() == 4 && draws == 1);
    const std::array<unsigned, 4> sides{0, 3, 1, 2};
    for (unsigned i = 0; i < 4; ++i)
        assert(out.damage[i].participant == 1 && out.damage[i].contact_side == sides[i] &&
               out.damage[i].damage == 1);
    assert(std::abs(state.pool.particles[0].velocity.x - 10 * std::pow(.99f, 4)) < .00001f);
    // Strict Manhattan distance1.5 excludes the boundary even at arbitrary Y.
    state.pool.particles.clear();
    state.pool.particles.push_back({{0, 0, -2.5f}, {}, 1, 0, 0, 0});
    assert(advance_flame_thrower(state, 1, {}, {}, 100, targets, .01f, random, out, error) ==
           Result::ok);
    assert(out.damage.empty());
    state.elapsed_2f2c = 2.99f;
    assert(advance_flame_thrower(state, 1, {}, {}, 100, {}, .01f, random, out, error) ==
           Result::ok);
    assert(state.active_270 && !out.deactivate);
    assert(advance_flame_thrower(state, 1, {}, {}, 100, {}, .01f, random, out, error) ==
           Result::ok);
    assert(!state.active_270 && out.deactivate && !state.ready_2f30);
    assert(advance_flame_thrower(state, 1, {}, {}, 100, {}, 2, {}, out, error) == Result::ok);
    assert(state.pool.particles.empty() && !state.ready_2f30);
    assert(advance_flame_thrower(state, 1, {}, {}, 100, {}, .01f, {}, out, error) == Result::ok);
    assert(out.remove_callback && flame_thrower_can_activate(state) && state.elapsed_2f2c == 0);
    assert(advance_flame_thrower(state, 1, {}, {}, 100, {}, std::numeric_limits<float>::quiet_NaN(),
                                 {}, out, error) == Result::invalid_argument);
}
} // namespace
int main() {
    electron_tests();
    particle_tests();
    flame_tests();
    std::cout << "Original Electron Cannon and Flame Thrower kernels passed\n";
}
