#include "game_src/unresolved/vehicle_damage_particles.hpp"
#include <cassert>
#include <cmath>
using namespace roadwars;
int main() {
    std::size_t draws{};
    ParticleRandom random([&] {
        ++draws;
        return 0u;
    });
    VehicleDamageParticles pool;
    assert(pool.advance(.1f, .05f, {}, {}, random) == Result::ok && draws == 0);
    assert(pool.advance(0, .5f, {1, 2, 3}, {4, 5, 6}, random) == Result::ok && draws == 3);
    assert(pool.draw_order().size() == 1 && !pool.fire_material(0));
    auto particle = pool.particles()[0];
    assert(particle.remaining_life == .5f && particle.size == 2.5f);
    assert(particle.velocity.x == 3.5f && particle.velocity.y == 9 && particle.velocity.z == 5.5f);
    assert(particle.position.x == 1 && particle.color_argb == 0x7f0f0f0cu);
    assert(pool.advance(.02f, 0, {}, {}, random) == Result::ok && draws == 3);
    assert(std::abs(pool.particles()[0].position.x - 1.0336f) < .0001f);
    pool.reset();
    assert(pool.advance(.1f, 1, {}, {}, random) == Result::ok);
    assert(pool.fire_material(0) && pool.particles()[0].atlas_frame == 1);
    assert(pool.advance(.25f, 0, {}, {}, random) == Result::ok);
    assert(!pool.fire_material(0));
    assert(pool.particles()[0].color_argb == 0xa5525252u);
    pool.reset();
    assert(pool.advance(.1f, 1, {}, {}, random) == Result::ok);
    assert(pool.advance(.1f, .11f, {}, {}, random) == Result::ok);
    assert(pool.draw_order()[0] == 1 && pool.draw_order()[1] == 0);
    std::uint32_t frame{};
    assert(pool.atlas_frame(0, frame) == Result::ok);
    assert(pool.atlas_frame(1, frame) == Result::ok);
    assert(pool.atlas_frame(40, frame) == Result::invalid_argument);
    const auto older_life = pool.particles()[0].remaining_life;
    assert(pool.advance(.02f, 0, {}, {}, random) == Result::ok);
    assert(pool.draw_order().size() == 1 && pool.draw_order()[0] == 0);
    // Original live-count traversal bound stops after removing the youngest head.
    assert(pool.particles()[0].remaining_life == older_life);
    // Every emitted slot must remain drawable through several complete pool wraps.
    pool.reset();
    for (unsigned tick = 0; tick < 1200; ++tick) {
        assert(pool.advance(1.f / 60, 1, {}, {}, random) == Result::ok);
        for (const auto slot : pool.draw_order()) {
            assert(pool.atlas_frame(slot, frame) == Result::ok && frame < 4);
        }
        if (tick > 12)
            assert(pool.draw_order().size() >= 2);
    }
    pool.stop(true);
    assert(!pool.enabled() && pool.draw_order().empty());
    const auto before = draws;
    assert(pool.advance(1, 1, {}, {}, random) == Result::ok && draws == before);
    pool.start();
    assert(pool.advance(0, 1, {}, {}, random) == Result::ok && draws == before + 3);
    assert(pool.advance(-1, 1, {}, {}, random) == Result::invalid_argument);
}
