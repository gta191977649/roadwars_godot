#include "game_src/unresolved/weapon_projectiles.hpp"
#include <cassert>
#include <cmath>

using namespace roadwars;
namespace {
bool close(float a, float b) { return std::abs(a - b) < .001f; }
WeaponProjectileTarget target(std::size_t id, std::int32_t identity, float z) {
    WeaponProjectileTarget result;
    result.participant = id;
    result.identity = identity;
    result.position = {0, 0, z};
    result.height = 1;
    result.footprint = {{{-1, 0, z + 1}, {1, 0, z + 1}, {1, 0, z - 1}, {-1, 0, z - 1}}};
    return result;
}
} // namespace
int main() {
    WeaponProjectileSpec spec;
    spec.acceleration = 2;
    spec.top_speed = 20;
    spec.diagonal_radius = .1f;
    spec.corner_angle_degrees = 45;
    spec.wall_radius = .1f;
    WeaponProjectile shot;
    bool launched{};
    assert(launch_weapon_projectile(shot, spec, 0, 0, 10, {}, launched) == Result::ok && launched);
    assert(shot.active && shot.velocity.z == 10 && shot.acceleration_vector.z == 2);
    assert(launch_weapon_projectile(shot, spec, 180, 0, 10, {}, launched) == Result::ok &&
           !launched);
    assert(advance_weapon_projectile_motion(shot, spec, 1, 0) == Result::ok);
    assert(shot.age == 1 && shot.scalar_speed == 12 && shot.velocity.z == 12 &&
           shot.position.z == 12);
    assert(shot.previous_position.z == 0 && shot.position.y == 0);
    shot.scalar_speed = 19;
    assert(advance_weapon_projectile_motion(shot, spec, 1, 0) == Result::ok);
    assert(shot.scalar_speed == 21 && shot.velocity.z == 12 && shot.position.z == 24);
    shot.age = 5;
    const auto position = shot.position;
    assert(advance_weapon_projectile_motion(shot, spec, .1f, 0) == Result::ok);
    assert(!shot.active && shot.position.z == position.z);

    assert(launch_weapon_projectile(shot, spec, 0, 90, 10, {}, launched) == Result::ok && launched);
    assert(shot.velocity.y > 9.999f); // Original pitch axis is -X, not +X.
    stop_weapon_projectile(shot, spec, 0);
    assert(launch_weapon_projectile(shot, spec, 90, 0, 10, {}, launched) == Result::ok && launched);
    assert(shot.velocity.x > 9.999f);

    shot.previous_position = {0, .2f, -10};
    shot.position = {0, .2f, 10};
    const std::array targets{target(5, 2, 4), target(3, 3, -4)};
    auto hit = weapon_projectile_vehicle_hit(shot, 1, targets);
    assert(hit.kind == ProjectileHitKind::vehicle && hit.participant == 5 && hit.contact_side == 0);
    assert(hit.point.z == 5 &&
           hit.point.y == .2f); // First participant and first edge, not nearest.
    hit = weapon_projectile_vehicle_hit(shot, 2, targets);
    assert(hit.kind == ProjectileHitKind::vehicle && hit.participant == 3);
    shot.position.y = 1;
    assert(weapon_projectile_vehicle_hit(shot, 1, targets).kind == ProjectileHitKind::none);
    shot.position.y = -.5f;
    assert(weapon_projectile_vehicle_hit(shot, 1, targets).kind == ProjectileHitKind::none);
    shot.position.y = -.49f;
    assert(weapon_projectile_vehicle_hit(shot, 1, targets).kind == ProjectileHitKind::vehicle);

    shot.position = {10, 1, 0};
    shot.previous_position = {0, 1, 0};
    shot.heading_degrees = 90;
    std::array<WallRecord, 1> walls{{{{5, -5, -2}, {5, 5, 2}, {-1, 0, 0}, 0}}};
    GroundContact ground;
    ground.hit = true;
    ground.point = {10, 0, 0};
    hit = weapon_projectile_world_hit(shot, spec, walls, ground);
    assert(hit.kind == ProjectileHitKind::wall && hit.point.x == 10);
    walls[0].normal = {1, 0, 0};
    assert(weapon_projectile_world_hit(shot, spec, walls, ground).kind == ProjectileHitKind::none);
    ground.point.y = 1;
    assert(weapon_projectile_world_hit(shot, spec, walls, ground).kind ==
           ProjectileHitKind::ground);

    spec.has_trail = true;
    stop_weapon_projectile(shot, spec, 1);
    assert(shot.active && shot.stopping);
    assert(weapon_projectile_vehicle_hit(shot, 1, targets).kind == ProjectileHitKind::none);
    stop_weapon_projectile(shot, spec, 0);
    assert(!shot.active);
    assert(advance_weapon_projectile_motion(shot, spec, -1, 0) == Result::invalid_argument);
    assert(close(shot.position.x, 10));

    WeaponSalvoRuntime salvo;
    std::string error;
    assert(salvo.initialize("Rocket Launcher", error) == Result::unavailable);
    assert(!salvo.ready());
    assert(salvo.initialize("Machine Gun", error) == Result::ok && salvo.ready());
    assert(salvo.pool_capacity() == 15 && salvo.muzzle_name(0) == "295ULFireLeft");
    assert(salvo.begin({{}, 0, 0, 10}) && !salvo.begin({{}, 0, 0, 10}));
    std::optional<WeaponSalvoShot> emitted;
    assert(salvo.advance(.01f, 10, 5, emitted) == Result::ok && emitted);
    assert(emitted->pool_index == 0 && emitted->muzzle_index == 0 && emitted->starting_speed == 70);
    assert(salvo.advance(.2f, 11, 6, emitted) == Result::ok && emitted);
    assert(emitted->pool_index == 1 && emitted->muzzle_index == 1 && emitted->starting_speed == 50);
    assert(emitted->heading == 11 && emitted->pitch == 6);
    assert(salvo.advance(.7f, 0, 0, emitted) == Result::ok && emitted);
    assert(!salvo.active() && !salvo.ready());
    assert(salvo.advance(.01f, 0, 0, emitted) == Result::ok && !emitted && salvo.ready());
    assert(salvo.begin({{}, 0, 0, 0}));
    assert(salvo.advance(.01f, 0, 0, emitted) == Result::ok && emitted->pool_index == 3);

    for (const auto& [name, speed] :
         std::array{std::pair{"20mm Cannon", 90.0f}, std::pair{"Plasma Blaster", 110.0f},
                    std::pair{"Vulcanized Gun", 70.0f}}) {
        assert(salvo.initialize(name, error) == Result::ok && salvo.pool_capacity() == 5);
        assert(salvo.begin({{}, 0, 0, 0}));
        assert(salvo.advance(.1f, 0, 0, emitted) == Result::ok && emitted);
        assert(emitted->starting_speed == speed && salvo.muzzle_name(0) == "295ULFire");
        assert(salvo.advance(.9f, 0, 0, emitted) == Result::ok && !emitted && salvo.active());
        assert(salvo.advance(.01f, 0, 0, emitted) == Result::ok && !emitted && !salvo.active());
    }
    assert(salvo.initialize("Gatling Gun", error) == Result::ok && salvo.pool_capacity() == 30);
    assert(salvo.begin({{}, 0, 0, 1}));
    for (std::size_t i = 0; i < 6; ++i) {
        assert(salvo.advance(.07f, 0, 0, emitted) == Result::ok && emitted);
        assert(emitted->muzzle_index == i % 5 && emitted->pool_index == i);
        assert(emitted->starting_speed == 61);
    }

    WeaponParameters parameters;
    parameters.ammunition_mass_kg = 2;
    parameters.impact_multiplier = 3;
    shot.heading_degrees = 0;
    shot.velocity = {0, 3, 4};
    WeaponProjectileImpulse impulse;
    assert(weapon_projectile_impact_impulse(shot, parameters, {1, 9, 2}, impulse) == Result::ok);
    assert(impulse.force.z == 30 && impulse.force.y == 0 && impulse.application.y == 0);

    WeaponRocket rocket;
    parameters.acceleration = 50;
    parameters.top_speed = 200;
    assert(launch_weapon_rocket(rocket, parameters, {{0, 2, 0}, 0, 0, 60}, launched) == Result::ok);
    assert(launched && !rocket.ready && rocket.horizontal_speed == 60 && rocket.callback_running);
    TrackCollision empty_track;
    const WeaponRocketShape rocket_shape{1, .2f, .2f};
    auto rocket_target = target(7, 9, 5);
    rocket_target.height = 3;
    assert(advance_weapon_rocket(rocket, parameters, rocket_shape, .1f, true, 1,
                                 std::span(&rocket_target, 1), empty_track, hit) == Result::ok);
    assert(hit.kind == ProjectileHitKind::vehicle && hit.participant == 7 && hit.contact_side == 0);
    assert(rocket.impacted && !rocket.motion.active && !rocket.ready);
    assert(close(rocket.flare_roll, 3));
    assert(close(rocket.motion.position.z, 6.5f) && close(rocket.horizontal_speed, 65));
    assert(weapon_rocket_impact_impulse(rocket, parameters, hit.point, impulse) == Result::ok);
    assert(close(impulse.force.z, 390) && impulse.application.y == 0);
    const auto hit_position = rocket.motion.position;
    assert(advance_weapon_rocket(rocket, parameters, rocket_shape, .1f, true, 1, {}, empty_track,
                                 hit) == Result::ok);
    assert(rocket.ready && !rocket.callback_running && rocket.motion.position.z > hit_position.z);
    assert(close(rocket.flare_roll, 6));
    assert(hit.kind == ProjectileHitKind::none);
    assert(launch_weapon_rocket(rocket, parameters, {{0, 10, 0}, 0, 30, 60}, launched) ==
           Result::ok);
    assert(close(rocket.flare_roll, 6)); // Native child roll persists across projectile reuse.
    assert(rocket.vertical_speed > 29.9f && rocket.horizontal_speed < 30.1f);
    assert(advance_weapon_rocket(rocket, parameters, rocket_shape, 1, false, 1, {}, empty_track,
                                 hit) == Result::ok);
    assert(rocket.motion.velocity.y > 54.9f && rocket.horizontal_speed > 54.9f);
    rocket.motion.age = 5;
    assert(advance_weapon_rocket(rocket, parameters, rocket_shape, .01f, false, 1, {}, empty_track,
                                 hit) == Result::ok);
    assert(!rocket.motion.active && rocket.callback_running && !rocket.ready);
}
