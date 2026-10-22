#include "game_src/unresolved/weapon_projectiles.hpp"
#include <cmath>

namespace roadwars {
namespace {
constexpr float kRadians = .017455555498600006f;
Vec3 add(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 subtract(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 scale(Vec3 a, float value) { return {a.x * value, a.y * value, a.z * value}; }
float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
bool finite(Vec3 a) { return std::isfinite(a.x) && std::isfinite(a.y) && std::isfinite(a.z); }
bool valid(const WeaponProjectileSpec& spec) {
    return std::isfinite(spec.acceleration) && std::isfinite(spec.top_speed) &&
           spec.top_speed >= 0 && std::isfinite(spec.diagonal_radius) &&
           spec.diagonal_radius >= 0 && std::isfinite(spec.corner_angle_degrees) &&
           std::isfinite(spec.wall_radius) && spec.wall_radius >= 0;
}
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
} // namespace
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0047e332
Result launch_weapon_projectile(WeaponProjectile& state, const WeaponProjectileSpec& spec,
                                float heading, float pitch, float starting_speed, Vec3 muzzle,
                                bool& launched) {
    launched = false;
    if (!valid(spec) || !std::isfinite(heading) || !std::isfinite(pitch) ||
        !std::isfinite(starting_speed) || starting_speed < 0 || !finite(muzzle))
        return Result::invalid_argument;
    if (state.active)
        return Result::ok;
    // CRT initializers 004dab4a/004dabb0/004dabd2: +Z, -X pitch axis, +Y yaw axis.
    const float p = pitch * kRadians, h = heading * kRadians;
    const Vec3 direction{std::sin(h) * std::cos(p), std::sin(p), std::cos(h) * std::cos(p)};
    state = {};
    state.position = state.previous_position = muzzle;
    state.heading_degrees = heading;
    state.pitch_degrees = pitch;
    state.velocity = scale(direction, starting_speed);
    state.acceleration_vector = scale(direction, spec.acceleration);
    state.scalar_speed = starting_speed;
    state.active = true;
    launched = true;
    return Result::ok;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0047e529
void stop_weapon_projectile(WeaponProjectile& state, const WeaponProjectileSpec& spec,
                            std::size_t live_trail_particles) {
    if (spec.has_trail) {
        state.stopping = true;
        if (live_trail_particles == 0)
            state.active = false;
    } else
        state.active = false;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0047e5e0
Result advance_weapon_projectile_motion(WeaponProjectile& state, const WeaponProjectileSpec& spec,
                                        float delta, std::size_t live_trail_particles) {
    if (!valid(spec) || !std::isfinite(delta) || delta < 0 || !finite(state.position) ||
        !finite(state.velocity) || !finite(state.acceleration_vector) ||
        !std::isfinite(state.age) || !std::isfinite(state.scalar_speed))
        return Result::invalid_argument;
    if (!state.active)
        return Result::ok;
    auto next = state;
    next.age += delta;
    next.scalar_speed += delta * spec.acceleration;
    if (next.age <= 5.0f) {
        next.previous_position = next.position;
        // The speed accumulator is updated first. No clamp or final partial acceleration exists.
        if (next.scalar_speed < spec.top_speed)
            next.velocity = add(next.velocity, scale(next.acceleration_vector, delta));
        next.position = add(next.position, scale(next.velocity, delta));
    } else
        stop_weapon_projectile(next, spec, live_trail_particles);
    if (!finite(next.position) || !finite(next.velocity) || !std::isfinite(next.age) ||
        !std::isfinite(next.scalar_speed))
        return Result::invalid_argument;
    state = next;
    return Result::ok;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0047eb01
WeaponProjectileHit weapon_projectile_vehicle_hit(const WeaponProjectile& state,
                                                  std::int32_t owner_identity,
                                                  std::span<const WeaponProjectileTarget> targets,
                                                  float below_origin_height) {
    if (!state.active || state.stopping || state.age > 5.0f)
        return {};
    constexpr std::array<std::size_t, 4> kSide{0, 3, 1, 2};
    for (const auto& target : targets) {
        if (target.identity == owner_identity)
            continue;
        const float height_limit =
            target.position.y < state.position.y ? target.height : below_origin_height;
        if (!(std::abs(state.position.y - target.position.y) < height_limit))
            continue;
        for (std::size_t edge = 0; edge < 4; ++edge) {
            Vec3 point;
            if (intersect(state.previous_position, state.position, target.footprint[edge],
                          target.footprint[(edge + 1) % 4], point)) {
                point.y = state.position.y;
                return {ProjectileHitKind::vehicle, point, target.participant, kSide[edge]};
            }
        }
    }
    return {};
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0047eb01
Result weapon_projectile_impact_impulse(const WeaponProjectile& state,
                                        const WeaponParameters& parameters, Vec3 point,
                                        WeaponProjectileImpulse& output) {
    if (!finite(state.velocity) || !finite(point) || !std::isfinite(state.heading_degrees) ||
        !std::isfinite(parameters.ammunition_mass_kg) || parameters.ammunition_mass_kg < 0 ||
        !std::isfinite(parameters.impact_multiplier) || parameters.impact_multiplier < 0)
        return Result::invalid_argument;
    const float speed = std::sqrt(dot(state.velocity, state.velocity));
    const float magnitude = speed * parameters.ammunition_mass_kg * parameters.impact_multiplier;
    const float heading = state.heading_degrees * kRadians;
    const Vec3 force{std::sin(heading) * magnitude, 0, std::cos(heading) * magnitude};
    if (!finite(force))
        return Result::invalid_argument;
    output = {force, {point.x, 0, point.z}};
    return Result::ok;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004be17b
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0047f11f
WeaponProjectileHit weapon_projectile_world_hit(const WeaponProjectile& state,
                                                const WeaponProjectileSpec& spec,
                                                std::span<const WallRecord> walls,
                                                const GroundContact& ground) {
    if (!state.active || state.stopping || state.age > 5.0f)
        return {};
    CollisionBody body;
    body.position = state.position;
    body.heading_degrees = state.heading_degrees;
    body.diagonal_radius = spec.diagonal_radius;
    body.corner_angle_degrees = spec.corner_angle_degrees;
    const auto current = collision_footprint(body);
    body.position = state.previous_position;
    const auto previous = collision_footprint(body);
    for (std::size_t corner = 0; corner < current.size(); ++corner)
        for (const auto& wall : walls) {
            Vec3 point;
            if (intersect(wall.lower, wall.upper, previous[corner], current[corner], point) &&
                dot(subtract(current[corner], previous[corner]), wall.normal) < 0)
                return {ProjectileHitKind::wall, state.position};
        }
    if (ground.hit && state.position.y <= ground.point.y)
        return {ProjectileHitKind::ground, ground.point};
    return {};
}
WeaponProjectileHit weapon_projectile_world_hit(const WeaponProjectile& state,
                                                const WeaponProjectileSpec& spec,
                                                const TrackCollision& track) {
    const auto walls =
        track.nearby_walls(state.position, state.previous_position, spec.wall_radius);
    return weapon_projectile_world_hit(state, spec, walls, track.sample(state.position));
}
Result WeaponSalvoRuntime::initialize(std::string_view weapon_name, std::string& error) {
    const auto name = ResourceStore::canonical(weapon_name);
    Kind kind;
    if (name == "MACHINE GUN")
        kind = Kind::machine;
    else if (name == "20MM CANNON")
        kind = Kind::cannon;
    else if (name == "PLASMA BLASTER")
        kind = Kind::plasma;
    else if (name == "VULCANIZED GUN")
        kind = Kind::vulcan;
    else if (name == "GATLING GUN")
        kind = Kind::gatling;
    else {
        error = "Original weapon has no recovered shared-projectile salvo kernel: " +
                std::string(weapon_name);
        return Result::unavailable;
    }
    *this = {};
    kind_ = kind;
    initialized_ = true;
    return Result::ok;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0045b387
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0040c7e5
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0047d4d8
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004cea45
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0044cccc
bool WeaponSalvoRuntime::begin(const WeaponFireRequest& request) {
    if (!ready() || !std::isfinite(request.speed) || !std::isfinite(request.heading) ||
        !std::isfinite(request.pitch) || !finite(request.origin))
        return false;
    active_ = true;
    ready_ = false;
    elapsed_ = 0;
    launch_speed_ = request.speed;
    muzzle_index_ = 0;
    for (auto& flash : flashes_) {
        flash.remaining = 1;
        flash.visible = false;
    }
    cadence_ = kind_ == Kind::machine ? .2f : kind_ == Kind::gatling ? .07f : 0;
    return true;
}
std::size_t WeaponSalvoRuntime::pool_capacity() const {
    return kind_ == Kind::machine ? 15 : kind_ == Kind::gatling ? 30 : 5;
}
std::string_view WeaponSalvoRuntime::muzzle_name(std::size_t index) const {
    constexpr std::array<std::string_view, 5> kGatling{"295ULFire1", "295ULFire2", "295ULFire3",
                                                       "295ULFire4", "295ULFire5"};
    if (kind_ == Kind::machine)
        return index == 0 ? "295ULFireLeft" : index == 1 ? "295ULFireRight" : "";
    if (kind_ == Kind::gatling)
        return index < kGatling.size() ? kGatling[index] : "";
    return index == 0 ? "295ULFire" : "";
}
WeaponProjectileSpec WeaponSalvoRuntime::projectile_spec(const WeaponParameters& parameters) const {
    return {parameters.acceleration, parameters.top_speed, .1f, 45, .1f, kind_ == Kind::plasma};
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0045b4ef
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0045b7f6
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0040c919
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0040cae3
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0047d619
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0047d7e3
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004ceb79
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004ced43
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0044ce3c
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0044d12c
Result WeaponSalvoRuntime::advance(float delta, float heading, float pitch,
                                   std::optional<WeaponSalvoShot>& shot, float parent_roll) {
    shot.reset();
    if (!initialized_)
        return Result::unavailable;
    if (!std::isfinite(delta) || delta < 0 || !std::isfinite(heading) || !std::isfinite(pitch) ||
        !std::isfinite(parent_roll))
        return Result::invalid_argument;
    if (!active_) {
        for (auto& flash : flashes_)
            flash.visible = false;
        ready_ = true;
        elapsed_ = 0;
        return Result::ok;
    }
    const bool burst = kind_ == Kind::machine || kind_ == Kind::gatling;
    bool emit = !burst && elapsed_ == 0;
    if (emit) {
        flashes_[0].remaining = 1;
        flashes_[0].visible = true;
    }
    elapsed_ += delta;
    for (auto& flash : flashes_) {
        flash.remaining -= delta * 3;
        if (kind_ == Kind::gatling && flash.visible)
            flash.scale = 3 - (flash.remaining + flash.remaining);
    }
    if (burst) {
        cadence_ += delta;
        const float interval = kind_ == Kind::machine ? .2f : .07f;
        if (interval <= cadence_) {
            emit = true;
            cadence_ = 0; // Original resets to zero; no remainder or per-callback catch-up loop.
        }
    }
    if (emit) {
        if (burst) {
            flashes_[muzzle_index_].remaining = 1;
            flashes_[muzzle_index_].visible = true;
        }
        const float offset = kind_ == Kind::machine  ? (muzzle_index_ == 0 ? 60.0f : 40.0f)
                             : kind_ == Kind::cannon ? 90.0f
                             : kind_ == Kind::plasma ? 110.0f
                             : kind_ == Kind::vulcan ? 70.0f
                                                     : 60.0f;
        shot = WeaponSalvoShot{pool_index_, muzzle_index_, heading, pitch, launch_speed_ + offset};
        pool_index_ = (pool_index_ + 1) % pool_capacity();
        if (kind_ == Kind::machine)
            muzzle_index_ = (muzzle_index_ + 1) % 2;
        else if (kind_ == Kind::gatling)
            muzzle_index_ = (muzzle_index_ + 1) % 5;
    }
    for (auto& flash : flashes_) {
        if (flash.visible) {
            if (flash.remaining > 0)
                flash.color_argb =
                    static_cast<std::uint32_t>(flash.remaining * 255) << 24 | 0x00ffffff;
            else
                flash.visible = false;
        }
        if (kind_ == Kind::machine)
            flash.local_roll += parent_roll + delta * 180;
        else if (kind_ == Kind::gatling)
            flash.local_roll += delta * 180;
    }
    if ((burst ? .9f : 1.0f) < elapsed_)
        active_ = false;
    return Result::ok;
}
std::span<const WeaponMuzzleFlash> WeaponSalvoRuntime::muzzle_flashes() const {
    const std::size_t count = kind_ == Kind::machine ? 2 : kind_ == Kind::gatling ? 5 : 1;
    return {flashes_.data(), count};
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0049f83a
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004cf5ed
Result launch_weapon_rocket(WeaponRocket& state, const WeaponParameters& parameters,
                            const WeaponFireRequest& request, bool& launched) {
    launched = false;
    if (!std::isfinite(parameters.acceleration) || !std::isfinite(request.speed) ||
        request.speed < 0)
        return Result::invalid_argument;
    if (!state.ready || state.motion.active)
        return Result::ok;
    WeaponRocket next;
    next.flare_roll = state.flare_roll;
    WeaponProjectileSpec spec;
    spec.acceleration = parameters.acceleration;
    spec.top_speed = parameters.top_speed;
    if (launch_weapon_projectile(next.motion, spec, request.heading, request.pitch, request.speed,
                                 request.origin, launched) != Result::ok)
        return Result::invalid_argument;
    const float pitch = request.pitch * kRadians, heading = request.heading * kRadians;
    next.direction = {std::sin(heading) * std::cos(pitch), std::sin(pitch),
                      std::cos(heading) * std::cos(pitch)};
    next.vertical_speed = request.speed * next.direction.y;
    next.horizontal_speed = request.speed - next.vertical_speed;
    next.ready = false;
    next.callback_running = true;
    state = next;
    return Result::ok;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0049faaf
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004a00d2
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0049fe9a
Result advance_weapon_rocket(WeaponRocket& state, const WeaponParameters& parameters,
                             const WeaponRocketShape& shape, float delta, bool trail_empty,
                             std::int32_t owner_identity,
                             std::span<const WeaponProjectileTarget> targets,
                             const TrackCollision& track, WeaponProjectileHit& hit,
                             const std::function<Result(bool, Vec3, float)>& advance_trail) {
    hit = {};
    if (!std::isfinite(delta) || delta < 0 || !std::isfinite(parameters.acceleration) ||
        !std::isfinite(shape.length) || shape.length <= 0 || !std::isfinite(shape.width) ||
        shape.width <= 0 || !std::isfinite(shape.height) || shape.height <= 0)
        return Result::invalid_argument;
    if (!state.callback_running)
        return Result::ok;
    auto next = state;
    next.flare_roll += 30 * delta;
    const float vertical_acceleration = parameters.acceleration * next.direction.y;
    next.horizontal_speed += (parameters.acceleration - std::abs(vertical_acceleration)) * delta;
    next.vertical_speed += vertical_acceleration * delta;
    next.motion.velocity =
        add(next.motion.velocity, scale(next.direction, delta * parameters.acceleration));
    next.motion.previous_position = next.motion.position;
    next.motion.position = add(next.motion.position, scale(next.motion.velocity, delta));
    if (!finite(next.motion.position) || !finite(next.motion.velocity) ||
        !std::isfinite(next.horizontal_speed) || !std::isfinite(next.vertical_speed))
        return Result::invalid_argument;
    // Original moves first, even during the final cleanup callback.
    if (!next.motion.active && trail_empty) {
        next.ready = true;
        next.callback_running = false;
        next.motion.age = 0;
        state = next;
        return Result::ok;
    }
    if (advance_trail &&
        advance_trail(next.motion.active, next.motion.position, delta) != Result::ok)
        return Result::unavailable;
    next.motion.age += delta;
    if (!next.impacted) {
        auto collision = next.motion;
        // Rocket collision remains enabled during expiry/trail drain until an actual impact.
        collision.active = true;
        collision.stopping = false;
        collision.age = 0;
        hit = weapon_projectile_vehicle_hit(collision, owner_identity, targets, shape.height);
        if (hit.kind == ProjectileHitKind::none) {
            WeaponProjectileSpec spec;
            spec.diagonal_radius = std::sqrt((shape.width * .5f) * (shape.width * .5f) +
                                             (shape.length * .5f) * (shape.length * .5f));
            spec.corner_angle_degrees = static_cast<float>(
                std::atan(static_cast<double>((shape.width * .5f) / (shape.length * .5f))) *
                57.288352966308594);
            spec.wall_radius = shape.length;
            hit = weapon_projectile_world_hit(collision, spec, track);
        }
        if (hit.kind != ProjectileHitKind::none) {
            next.impacted = true;
            next.motion.active = false;
        }
    }
    if (next.motion.age > 5)
        next.motion.active = false;
    state = next;
    return Result::ok;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004a00d2
Result weapon_rocket_impact_impulse(const WeaponRocket& state, const WeaponParameters& parameters,
                                    Vec3 point, WeaponProjectileImpulse& output) {
    if (!std::isfinite(state.horizontal_speed) || !std::isfinite(state.motion.heading_degrees) ||
        !finite(point) || !std::isfinite(parameters.ammunition_mass_kg) ||
        parameters.ammunition_mass_kg < 0 || !std::isfinite(parameters.impact_multiplier) ||
        parameters.impact_multiplier < 0)
        return Result::invalid_argument;
    const float magnitude =
        state.horizontal_speed * parameters.ammunition_mass_kg * parameters.impact_multiplier;
    const float heading = state.motion.heading_degrees * kRadians;
    const Vec3 force{std::sin(heading) * magnitude, 0, std::cos(heading) * magnitude};
    if (!finite(force))
        return Result::invalid_argument;
    output = {force, {point.x, 0, point.z}};
    return Result::ok;
}
} // namespace roadwars
