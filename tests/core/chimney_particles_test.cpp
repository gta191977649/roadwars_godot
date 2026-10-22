#include "game_src/unresolved/chimney_particles.hpp"
#include <cassert>
#include <cmath>
using namespace roadwars;
int main() {
    std::size_t draws{};
    ParticleRandom random([&] {
        ++draws;
        return 2u;
    });
    ChimneyParticles pool;
    assert(pool.advance(.1f, {}, {}, random) == Result::unavailable);
    assert(pool.configure(100) == Result::ok);
    pool.start();
    assert(pool.advance(.5f, {}, {}, random) == Result::ok && draws == 0);
    assert(pool.advance(.01f, {}, {}, random) == Result::ok && draws == 1);
    assert(pool.draw_order().size() == 1 && pool.particles()[0].atlas_frame == 2);
    assert(std::abs(pool.particles()[0].remaining_life - (1 - .01f / 3)) < .00001f);
    assert(pool.particles()[0].position.y == .01f);
    const auto old = pool.particles()[0].remaining_life;
    assert(pool.advance(10, {121, 0, 0}, {}, random) == Result::ok && draws == 1);
    assert(pool.particles()[0].remaining_life == old);
    assert(pool.advance(.51f, {0, 0, 1}, {}, random) == Result::ok && draws == 2);
    // Existing smoke and newly emitted smoke both advance in this call.
    assert(pool.draw_order()[0] == 1 && pool.particles()[0].position.y == .52f);
    assert(std::abs(pool.triangles()[0][0].x - (-.625f - .75f * .52f)) < .00001f);
    assert(pool.advance(3, {}, {}, random) == Result::ok && pool.draw_order().empty());
    assert(pool.advance(-1, {}, {}, random) == Result::invalid_argument);
}
