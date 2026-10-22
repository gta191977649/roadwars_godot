#include "game_src/unresolved/special_weapon_effects.hpp"
#include <cmath>

namespace roadwars {
namespace {
bool finite(Vec3 p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }
bool valid_step(float damage, float dt) {
    return std::isfinite(damage) && damage >= 0 && std::isfinite(dt) && dt >= 0;
}
// Same original segment predicate used by track and projectile collisions.
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004dd310
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0047c368
bool intersect(Vec3 a, Vec3 b, Vec3 c, Vec3 d, Vec3& point) {
    const float x = a.x - b.x, z = a.z - b.z, u = -(c.x - d.x), v = -(c.z - d.z);
    if ((u == 0 && v == 0) || (x == 0 && z == 0))
        return false;
    auto between = [](float p, float q, float r) {
        return (p <= q && r <= p) || (p <= r && q <= p);
    };
    if (u == 0 && x == 0) {
        if (a.x != c.x)
            return false;
        if (between(a.z, c.z, d.z) || between(b.z, c.z, d.z)) {
            point.x = a.x;
            point.z = (a.z + b.z + c.z + d.z) / 4.0f;
            return true;
        }
    }
    if (v == 0 && z == 0) {
        if (a.z != c.z)
            return false;
        if (between(a.x, c.x, d.x) || between(b.x, c.x, d.x)) {
            point.x = (a.x + b.x + c.x + d.x) / 4.0f;
            point.z = a.z;
            return true;
        }
    }
    if (u != 0 && x != 0 && z / x == v / u)
        return false;
    float t{}, s{};
    if (v == 0 || u == 0) {
        if (u == 0) {
            if (v != 0)
                t = (c.x - a.x) / x;
        } else
            t = (c.z - a.z) / z;
    } else
        t = ((c.z - a.z) + (c.x - a.x) * (-v / u)) / (x * (-v / u) + z);
    if (x == 0 || z == 0) {
        if (z == 0) {
            if (x != 0)
                s = (c.z - a.z) / v;
        } else
            s = (c.x - a.x) / u;
    } else
        s = ((c.z - a.z) + (c.x - a.x) * (-z / x)) / (u * (-z / x) + v);
    if (t > 0 || t < -1 || s > 0 || s < -1)
        return false;
    point.x = x * t + a.x;
    point.z = z * t + a.z;
    return true;
}
std::uint32_t flame_color(float remaining, float hue) {
    const auto alpha = static_cast<std::uint32_t>(remaining * 255.0f);
    const auto shade = static_cast<std::uint32_t>((.5f + hue) * 255.0f);
    return alpha << 24 | 0x00ff0000u | shade << 8 | shade;
}
} // namespace

// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00433da1
bool electron_cannon_can_activate(const ElectronCannonState& state) {
    return !state.active_270 && state.ready_6c0;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00433da1
void begin_electron_cannon(ElectronCannonState& state) {
    state.active_270 = true;
    state.ready_6c0 = false;
    state.elapsed_6b8 = state.sound_timer_6bc = 0;
    state.beam_attached = true;
}

// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00433f1b
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00434128
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00434609
Result advance_electron_cannon(ElectronCannonState& state, std::uint64_t owner, Vec3 muzzle,
                               float heading, float damage_per_second,
                               std::span<const SpecialWeaponTarget> targets,
                               std::size_t live_impact_particles, float dt,
                               ElectronCannonOutput& output, std::string& error) {
    output = {};
    if (!valid_step(damage_per_second, dt) || !finite(muzzle) || !std::isfinite(heading) ||
        !std::isfinite(state.elapsed_6b8) || !std::isfinite(state.sound_timer_6bc) ||
        !std::isfinite(state.range_6d0) || state.range_6d0 < 0) {
        error = "Invalid original Electron Cannon update";
        return Result::invalid_argument;
    }
    output.beam_length = state.range_6d0;
    if (!state.active_270) {
        output.remove_beam = state.beam_attached;
        state.beam_attached = false;
        state.elapsed_6b8 = 0;
        if (live_impact_particles == 0) {
            output.remove_callback = true;
            state.ready_6c0 = true;
        }
        return Result::ok;
    }
    state.elapsed_6b8 += dt;
    if (state.sound_timer_6bc > 0)
        state.sound_timer_6bc -= dt;
    const float radians = heading * .017455555498600006f; // 005f53b0.
    const Vec3 end{muzzle.x + std::sin(radians) * state.range_6d0, muzzle.y,
                   muzzle.z + std::cos(radians) * state.range_6d0};
    for (const auto& target : targets) {
        if (target.chassis_identity == owner)
            continue;
        for (const auto corner : target.footprint)
            if (!finite(corner)) {
                error = "Invalid original Electron Cannon target footprint";
                return Result::invalid_argument;
            }
        for (std::size_t edge = 0; edge < 4; ++edge) {
            Vec3 point{0, muzzle.y, 0};
            if (!intersect(muzzle, end, target.footprint[edge], target.footprint[(edge + 1) % 4],
                           point))
                continue;
            output.hit_point = point;
            const float x = point.x - muzzle.x, z = point.z - muzzle.z;
            output.beam_length = std::sqrt(z * z + x * x) + .1f;
            for (std::size_t side = 0; side < 4; ++side)
                output.damage.push_back({target.participant, side, dt * damage_per_second});
            if (state.sound_timer_6bc <= 0) {
                output.play_hit_sound = true;
                state.sound_timer_6bc = .5f;
            }
            break;
        }
        if (output.hit_point)
            break; // Original participant/edge order, not the nearest hit.
    }
    if (state.elapsed_6b8 > 1.5f) {
        state.active_270 = false;
        output.deactivate = true;
    }
    return Result::ok;
}

// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004ef862
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004ef996
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004efbee
Result advance_flame_particles(FlameParticlePool& pool, bool emitting, Vec3 muzzle, Vec3 velocity,
                               float dt, const std::function<std::uint32_t()>& random,
                               std::string& error) {
    if (!valid_step(0, dt) || !std::isfinite(pool.emission_timer_293c) || !finite(muzzle) ||
        !finite(velocity) || pool.particles.size() > 100) {
        error = "Invalid original Flame Thrower particle update";
        return Result::invalid_argument;
    }
    for (const auto& particle : pool.particles)
        if (!finite(particle.position) || !finite(particle.velocity) ||
            !std::isfinite(particle.remaining) || particle.remaining < 0 ||
            particle.remaining > 1 || !std::isfinite(particle.hue) || particle.hue < 0 ||
            particle.hue > .49f) {
            error = "Invalid original Flame Thrower particle fields";
            return Result::invalid_argument;
        }
    pool.emission_timer_293c += dt;
    if (emitting && pool.emission_timer_293c >= .1f && pool.particles.size() < 100) {
        if (!random) {
            error = "Original Flame Thrower shared RNG unavailable";
            return Result::unavailable;
        }
        FlameParticle particle;
        particle.position = muzzle;
        particle.velocity = velocity;
        particle.hue = static_cast<float>(random() % 50) / 100.0f;
        particle.color_argb = flame_color(1, particle.hue);
        particle.atlas_frame = random() % 4;
        pool.particles.insert(pool.particles.begin(), particle); // Original AddHead.
        // 004ef996 does not reset +293c: after the first .1s the original emits
        // once per callback. Fixed simulation scheduling makes that render-independent.
    }
    for (auto it = pool.particles.begin(); it != pool.particles.end();) {
        it->remaining -= dt;
        if (it->remaining <= 0) {
            it = pool.particles.erase(it);
            continue;
        }
        it->color_argb = flame_color(it->remaining, it->hue);
        if (1.0f - dt != it->remaining) { // Newly emitted particles skip the first movement.
            it->position.x += dt * it->velocity.x;
            it->position.y += dt * it->velocity.y;
            it->position.z += dt * it->velocity.z;
        }
        if (!random) {
            error = "Original Flame Thrower shared RNG unavailable";
            return Result::unavailable;
        }
        it->atlas_frame = random() % 4;
        ++it;
    }
    return Result::ok;
}

// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00435682
bool flame_thrower_can_activate(const FlameThrowerState& state) {
    return !state.active_270 && state.ready_2f30;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00435682
void begin_flame_thrower(FlameThrowerState& state) {
    state.active_270 = true;
    state.ready_2f30 = false;
    state.elapsed_2f2c = 0;
}

// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0043578f
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00435a93
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00435928
Result advance_flame_thrower(FlameThrowerState& state, std::uint64_t owner, Vec3 muzzle,
                             Vec3 velocity, float damage_per_second,
                             std::span<const SpecialWeaponTarget> targets, float dt,
                             const std::function<std::uint32_t()>& random,
                             FlameThrowerOutput& output, std::string& error) {
    output = {};
    if (!valid_step(damage_per_second, dt) || !std::isfinite(state.elapsed_2f2c)) {
        error = "Invalid original Flame Thrower update";
        return Result::invalid_argument;
    }
    if (!state.active_270 && state.pool.particles.empty()) {
        output.remove_callback = true;
        state.ready_2f30 = true;
        state.elapsed_2f2c = 0;
        return Result::ok;
    }
    const auto advanced =
        advance_flame_particles(state.pool, state.active_270, muzzle, velocity, dt, random, error);
    if (advanced != Result::ok)
        return advanced;
    state.elapsed_2f2c += dt;
    constexpr std::array<std::size_t, 4> kContactSide{0, 3, 1, 2};
    for (const auto& target : targets) {
        if (target.chassis_identity == owner)
            continue;
        for (const auto corner : target.footprint)
            if (!finite(corner)) {
                error = "Invalid original Flame Thrower target footprint";
                return Result::invalid_argument;
            }
        std::array<Vec3, 4> midpoints;
        for (std::size_t i = 0; i < 4; ++i) {
            const auto a = target.footprint[i], b = target.footprint[(i + 1) % 4];
            midpoints[i] = {(a.x + b.x) / 2, 0, (a.z + b.z) / 2};
        }
        for (auto& particle : state.pool.particles) {
            for (std::size_t edge = 0; edge < 4; ++edge) {
                const float distance = std::abs(particle.position.x - midpoints[edge].x) +
                                       std::abs(particle.position.z - midpoints[edge].z);
                if (distance < 1.5f) {
                    output.damage.push_back(
                        {target.participant, kContactSide[edge], dt * damage_per_second});
                    const float damping = 1.0f - dt;
                    particle.velocity.x *= damping;
                    particle.velocity.y *= damping;
                    particle.velocity.z *= damping;
                }
            }
        }
    }
    if (state.elapsed_2f2c > 3) {
        state.active_270 = false;
        output.deactivate = true;
    }
    return Result::ok;
}
} // namespace roadwars
