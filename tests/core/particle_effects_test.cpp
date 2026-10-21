#include "game_src/unresolved/particle_effects.hpp"
#include <cmath>
#include <stdexcept>

using namespace roadwars;
namespace {
void require(bool value) {
    if (!value)
        throw std::runtime_error("Original particle contract failed");
}
bool close(float a, float b) { return std::abs(a - b) < 0.0001f; }
} // namespace
int main() {
    ParticleRandom known;
    require(known.next() == 41 && known.next() == 18467 && known.next() == 6334);
    const auto settings = vehicle_spark_settings(1);
    require(settings.burst && settings.capacity == 15 && settings.preload_count == 0);
    require(settings.atlas_choice == 1 && settings.size == 0.25f);
    require(vehicle_spark_settings(0.5f).capacity == 7);
    ParticleEmitter emitter;
    require(emitter.configure(settings) == Result::ok);
    ParticleRandom random;
    require(emitter.begin_burst({{5, 6, 7}, {10, 20, 30}, 1.5f}));
    require(!emitter.begin_burst({}));
    require(emitter.advance(.01f, true, {}, {}, random) == Result::ok);
    require(emitter.active_count() == 1);
    const auto first = emitter.particles()[0];
    require(first.active && first.atlas_frame == 1 && first.color_argb == 0xfbffffffu);
    require(close(first.velocity.x, -.35f) && close(first.velocity.y, 3.55f) &&
            close(first.velocity.z, .6f));
    require(close(first.position.x, 5.0245f) && close(first.position.y, 6.1065f) &&
            close(first.position.z, 6.958f));
    // Original integration advances by cached velocity*.03 every callback,
    // including a short .001-second frame, not velocity*current_delta.
    require(emitter.advance(.001f, false, {}, {}, random) == Result::ok);
    const auto moved = emitter.particles()[0];
    require(close(moved.position.x - first.position.x, first.cached_step.x));
    require(close(moved.position.y - first.position.y, first.cached_step.y));
    require(close(moved.remaining_life, .9835f));
    for (int i = 0; i < 20; ++i)
        require(emitter.advance(.01f, true, {}, {}, random) == Result::ok);
    require(emitter.active_count() == 15);
    require(emitter.advance(1.0f, true, {}, {}, random) == Result::ok);
    require(emitter.active_count() == 0 && !emitter.active());
    require(emitter.begin_burst({}));
    emitter.reset();
    require(!emitter.active() && emitter.active_count() == 0);
    const auto triangle = particle_triangle(.25f);
    require(triangle[0].x == -.0625f && triangle[0].y == -.1875f);
    require(triangle[2].x == .1875f && triangle[2].y == .0625f);
    const auto uv = particle_atlas_uv(1);
    require(uv[0][0] == .391f && uv[0][1] == .609f && uv[2][0] == 0 && uv[2][1] == .218f);
    require(emitter.advance(-1, true, {}, {}, random) == Result::invalid_argument);
    TireParticleEmitter tire;
    require(tire.configure(1.0f, random) == Result::ok);
    require(tire.particles().size() == 20);
    const TireParticleSource source{{5, 6, 7}, true, 0xff123456u};
    require(tire.advance(.1f, source) == Result::ok);
    require(tire.active_count() == 1);
    auto smoke = tire.particles()[0];
    require(smoke.active && smoke.atlas_frame == 0 && smoke.size == 2.5f);
    require(close(smoke.position.x, 5) && close(smoke.position.z, 7));
    require(close(smoke.position.y, 6.2075f) && smoke.color_argb == 0xe5123456u);
    auto stopped = source;
    stopped.emitting = false;
    require(tire.advance(.05f, stopped) == Result::ok);
    require(tire.active_count() == 1 && tire.particles()[0].atlas_frame == 1);
    require(tire.advance(.05f, source) == Result::ok);
    require(tire.active_count() == 2);
    require(tire.draw_order()[0] == 1 && tire.draw_order()[1] == 0);
    require(tire.particles()[1].atlas_frame == 2 && tire.particles()[0].atlas_frame == 3);
    require(tire.advance(1.1f, stopped) == Result::ok && tire.active_count() == 0);
    require(tire.configure(.5f, random) == Result::ok && tire.particles().size() == 10);
    require(tire.advance(.1f, source) == Result::ok);
    require(close(tire.particles()[0].remaining_life, .8f));
    const auto smoke_uv = tire_particle_atlas_uv(0);
    require(smoke_uv[0][0] == 0 && smoke_uv[0][1] == .609f && smoke_uv[1][1] == 1);
}
