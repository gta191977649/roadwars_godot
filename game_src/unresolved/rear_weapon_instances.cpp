#include "game_src/unresolved/rear_weapon_instances.hpp"
#include <cmath>

namespace roadwars {
namespace {
bool finite(Vec3 p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }
bool valid(const RearInstanceParameters& p) {
    return std::isfinite(p.hitpoints) && p.hitpoints >= 0 && std::isfinite(p.acceleration) &&
           p.acceleration >= 0 && std::isfinite(p.ammunition_mass_kg) && p.ammunition_mass_kg >= 0;
}
Vec3 heading_direction(float heading) {
    const float radians = heading * .017455555498600006f;
    return {std::sin(radians), 0, std::cos(radians)};
}
bool triangle_contains(Vec3 p, std::array<Vec3, 3> triangle) {
    unsigned inside{}, previous = 2;
    for (unsigned i = 0; i < 3; ++i) {
        const auto a = triangle[i], b = triangle[previous];
        if (((a.z <= p.z && p.z < b.z) && (p.x - a.x) * (b.z - a.z) < (p.z - a.z) * (b.x - a.x)) ||
            ((b.z <= p.z && p.z < a.z) && (p.z - a.z) * (b.x - a.x) < (p.x - a.x) * (b.z - a.z)))
            inside ^= 1;
        previous = i;
    }
    return inside != 0;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004dd310
bool intersects(Vec3 a, Vec3 b, Vec3 c, Vec3 d) {
    const float x = a.x - b.x, z = a.z - b.z, u = -(c.x - d.x), v = -(c.z - d.z);
    if ((u == 0 && v == 0) || (x == 0 && z == 0))
        return false;
    const auto between = [](float p, float q, float r) {
        return (p <= q && r <= p) || (p <= r && q <= p);
    };
    if (u == 0 && x == 0) {
        if (a.x != c.x)
            return false;
        if (between(a.z, c.z, d.z) || between(b.z, c.z, d.z))
            return true;
    }
    if (v == 0 && z == 0) {
        if (a.z != c.z)
            return false;
        if (between(a.x, c.x, d.x) || between(b.x, c.x, d.x))
            return true;
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
    return !(t > 0 || t < -1 || s > 0 || s < -1);
}
Result sample(const RearInstanceWorld& world, Vec3 position, GroundContact& contact,
              std::string& error) {
    if (!world.ground) {
        error = "Original rear weapon surface projection unavailable";
        return Result::unavailable;
    }
    const auto result = world.ground(position, contact);
    if (result != Result::ok)
        error = "Original rear weapon surface projection failed";
    return result;
}
void explode(RearWeaponInstance& instance, RearInstanceOutput& output) {
    output.exploded = output.removed = true;
    output.explosion_position = instance.position;
    instance.active = instance.callback_running = false;
}
} // namespace

// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0044e05f
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00456a40
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0040694a
Result launch_rear_weapon_instance(RearWeaponInstance& state, Vec3 position, float heading,
                                   bool& launched, std::string& error) {
    launched = false;
    if (!finite(position) || !std::isfinite(heading) ||
        static_cast<unsigned>(state.kind) > static_cast<unsigned>(RearInstanceKind::arachnoid)) {
        error = "Invalid original rear weapon launch";
        return Result::invalid_argument;
    }
    if (!state.active) {
        state.position = position;
        state.age = 0;
        state.active = state.callback_running = true;
        launched = true;
    }
    if (state.kind == RearInstanceKind::arachnoid) {
        state.heading = heading - 180;
        state.direction = heading_direction(state.heading);
    }
    return Result::ok;
}

// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0044e143
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00456b64
void stop_rear_weapon_instance(RearWeaponInstance& state) {
    state.active = state.callback_running = false;
}

// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0044e493
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0044e1c4
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0044de4e
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004570db
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0049d1e2
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004066eb
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004069bb
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00456bc7
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004566c1
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0049d161
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00406611
Result advance_rear_weapon_instance(RearWeaponInstance& state, const RearInstanceParameters& params,
                                    std::uint64_t owner,
                                    std::span<const RearInstanceTarget> targets,
                                    const RearInstanceWorld& world, float dt,
                                    RearInstanceOutput& output, std::string& error) {
    output = {};
    if (!valid(params) || !std::isfinite(dt) || dt < 0 || !finite(state.position) ||
        !finite(state.direction) || !std::isfinite(state.heading) || !std::isfinite(state.age) ||
        !std::isfinite(state.left_leg_heading) || !std::isfinite(state.right_leg_heading) ||
        state.age < 0 ||
        static_cast<unsigned>(state.kind) > static_cast<unsigned>(RearInstanceKind::arachnoid)) {
        error = "Invalid original rear weapon instance update";
        return Result::invalid_argument;
    }
    if (!state.active)
        return Result::ok;
    for (const auto& target : targets) {
        if (!finite(target.position)) {
            error = "Invalid original rear weapon target position";
            return Result::invalid_argument;
        }
        for (const auto point : target.footprint)
            if (!finite(point)) {
                error = "Invalid original rear weapon target footprint";
                return Result::invalid_argument;
            }
        for (const auto air : target.air_time)
            if (!std::isfinite(air)) {
                error = "Invalid original rear weapon target air time";
                return Result::invalid_argument;
            }
    }
    if (state.kind == RearInstanceKind::grenade)
        state.age += dt;
    const auto previous = state.position;
    if (state.kind == RearInstanceKind::arachnoid) {
        const float leg_step = 40.0f * 1.5f * dt;
        if (!state.leg_phase) {
            state.left_leg_heading -= leg_step;
            state.right_leg_heading += leg_step;
            if (state.left_leg_heading < -15)
                state.leg_phase = true;
        } else {
            state.left_leg_heading += leg_step;
            state.right_leg_heading -= leg_step;
            if (state.left_leg_heading > 10)
                state.leg_phase = false;
        }
        state.position.x += dt * state.direction.x * 1.5f;
        state.position.y -= 4.7f * dt;
        state.position.z += dt * state.direction.z * 1.5f;
        if (!world.arachnoid_walls) {
            error = "Original Arachnoid wall query unavailable";
            return Result::unavailable;
        }
        std::vector<WallRecord> walls;
        const auto result = world.arachnoid_walls(state.position, walls);
        if (result != Result::ok) {
            error = "Original Arachnoid wall query failed";
            return result;
        }
        if (walls.size() > 100) {
            error = "Original Arachnoid wall worklist capacity exceeded";
            return Result::invalid_argument;
        }
        for (std::size_t i = 0; i < walls.size(); ++i) {
            const Vec3 end{state.position.x + state.direction.x,
                           state.position.y + state.direction.y,
                           state.position.z + state.direction.z};
            if (intersects(state.position, end, walls[i].lower, walls[i].upper)) {
                if (!world.random) {
                    error = "Original Arachnoid shared RNG unavailable";
                    return Result::unavailable;
                }
                const auto draw = world.random();
                if (!draw) {
                    error = "Original Arachnoid reorientation RNG exhausted";
                    return Result::unavailable;
                }
                state.heading = static_cast<float>(*draw % 360);
                state.direction = heading_direction(state.heading);
                state.position = previous;
                // The original resets its loop index to0, then increments to1.
                i = 0;
            }
        }
    }
    GroundContact ground;
    const auto sampled = sample(world, state.position, ground, error);
    if (sampled != Result::ok)
        return sampled;
    if (state.kind == RearInstanceKind::arachnoid) {
        if (ground.hit && state.position.y <= ground.point.y)
            state.position.y = ground.point.y;
    } else {
        const bool project = ground.hit && state.position.y <= ground.point.y;
        state.position.y -= 4.7f * dt;
        if (project)
            state.position = ground.point;
    }
    const float force_scale = params.acceleration * params.ammunition_mass_kg;
    if (state.kind == RearInstanceKind::grenade) {
        if (state.age >= .75f) {
            explode(state, output);
            for (const auto& target : targets) {
                const Vec3 delta{target.position.x - state.position.x,
                                 target.position.y - state.position.y,
                                 target.position.z - state.position.z};
                const float distance =
                    std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
                if (distance < 6) {
                    output.impacts.push_back(
                        {target.participant,
                         params.hitpoints,
                         {delta.x * force_scale, delta.y * force_scale, delta.z * force_scale},
                         {state.position.x, 0, state.position.z},
                         true});
                }
            }
        }
        return Result::ok;
    }
    for (const auto& target : targets) {
        if (target.chassis_identity == owner && state.age < 2)
            continue;
        const auto& p = target.footprint;
        if (!triangle_contains(state.position, {p[0], p[1], p[2]}) &&
            !triangle_contains(state.position, {p[2], p[3], p[0]}))
            continue;
        bool all_airborne = true;
        for (const auto time : target.air_time)
            if (time == 0)
                all_airborne = false;
        if (all_airborne)
            continue;
        output.impacts.push_back({target.participant,
                                  params.hitpoints,
                                  {0, force_scale, 0},
                                  {state.position.x, 0, state.position.z},
                                  false});
        explode(state, output);
        output.extra_reactive_effect = state.kind == RearInstanceKind::reactive_mine;
        break;
    }
    state.age += dt; // Original mine owner-immunity test sees the previous callback age.
    return Result::ok;
}
} // namespace roadwars
