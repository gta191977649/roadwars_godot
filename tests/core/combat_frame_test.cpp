#include "game_src/core/simulation_clock.hpp"
#include "game_src/unresolved/guided_weapon_runtime.hpp"
#include "game_src/unresolved/race_loadout.hpp"
#include "game_src/unresolved/rear_weapon_effects.hpp"
#include "game_src/unresolved/special_weapon_effects.hpp"
#include "game_src/unresolved/turret_runtime.hpp"
#include "game_src/unresolved/vehicle_equipment_effects.hpp"
#include "game_src/unresolved/weapon_projectiles.hpp"
#include <bit>
#include <iostream>
#include <source_location>
#include <stdexcept>

using namespace roadwars;
namespace {
void require(bool value, std::source_location location = std::source_location::current()) {
    if (!value)
        throw std::runtime_error("Combat frame-rate regression failed at line " +
                                 std::to_string(location.line()));
}
using Trace = std::vector<std::vector<std::uint32_t>>;
Trace run(std::span<const double> cadence) {
    TurretParameters parameters;
    require(parse_turret_parameters("Turret_000_Full_M", parameters) == Result::ok);
    parameters.barrel_length = 1;
    TurretRuntime turret;
    require(turret.initialize(parameters, {0, 0, 0}) == Result::ok);
    WeaponSalvoRuntime salvo;
    std::string error;
    require(salvo.initialize("Machine Gun", error) == Result::ok);
    WeaponParameters weapon;
    weapon.top_speed = 300;
    const auto spec = salvo.projectile_spec(weapon);
    std::vector<WeaponProjectile> projectiles(salvo.pool_capacity());
    VehicleEquipmentEffects health;
    health.internal_hp = health.maximum_internal_hp = 1000;
    health.armor_hp = health.base_armor_hp = health.maximum_armor_hp = {100, 100, 100, 100};
    TurretTarget target;
    target.id = 0;
    // The original canonical +Z branch retains desired heading360 and never aligns to0.
    // Exercise real firing with an off-axis fixture, preserving that evidenced quirk.
    target.position = {10, 0, 40};
    WeaponProjectileTarget collision_target;
    collision_target.identity = 1;
    collision_target.position = target.position;
    collision_target.height = 2;
    collision_target.footprint = {{{8, 0, 38}, {12, 0, 38}, {12, 0, 42}, {8, 0, 42}}};
    GuidedWeaponRuntime guided;
    WeaponParameters guided_parameters;
    guided_parameters.ammunition_count = 4;
    guided_parameters.top_speed = 100;
    guided_parameters.acceleration = 20;
    guided_parameters.rotational_speed = 90;
    guided_parameters.tracking_seconds = 1;
    require(guided.initialize("Swarmers", guided_parameters, {1, 1}, error) == Result::ok);
    bool guided_launched{};
    require(guided.launch({{0, 1, 0}, 10, 0, 20}, guided_launched, error) == Result::ok &&
            guided_launched);
    std::uint32_t random_state = 1;
    GuidedWeaponServices guided_services;
    guided_services.random = [&] { return original_random(random_state); };
    guided_services.ground = [](Vec3, float& y) {
        y = 0;
        return Result::ok;
    };
    FlameThrowerState flame;
    begin_flame_thrower(flame);
    SpecialWeaponTarget flame_target;
    flame_target.chassis_identity = 1;
    flame_target.footprint = collision_target.footprint;
    MissileJammerState jammer;
    Trace trace;
    std::size_t frame{}, fired{}, hits{};
    SimulationClock clock;
    auto tick = [&](float dt, const InputState&) {
        if (trace.size() >= 720)
            return Result::ok;
        const GuidedWeaponTarget guided_target{target.position, 0};
        require(guided.begin_update(dt, guided_target, nullptr, guided_services, error) ==
                Result::ok);
        if (trace.size() == 60)
            begin_missile_jammer(jammer);
        if (jammer.active_270) {
            std::vector<Vec3> positions;
            for (const auto& missile : guided.missiles())
                positions.push_back(missile.position);
            const JammerWeaponView other{2, positions};
            const MissileJammerFrame jammer_frame{1, {0, 0, 0}, 0, std::span(&other, 1)};
            MissileJammerOutput jam_output;
            const MissileJammerDraws draws{original_random(random_state),
                                           original_random(random_state),
                                           original_random(random_state)};
            require(advance_missile_jammer(jammer, jammer_frame, draws, dt, jam_output, error) ==
                    Result::ok);
            for (const auto ignored : jam_output.jam_weapon_indices) {
                (void)ignored;
                require(guided.jam(1) == Result::ok);
            }
        }
        std::vector<std::size_t> empty_trails(guided.missiles().size());
        require(guided.finish_update(empty_trails) == Result::ok);
        FlameThrowerOutput flames;
        require(advance_flame_thrower(flame, 2, {10, 0, 32}, {0, 0, 10}, 10,
                                      std::span(&flame_target, 1), dt, guided_services.random,
                                      flames, error) == Result::ok);
        for (const auto& damage : flames.damage)
            require(damage_vehicle_contact(health, damage.contact_side, damage.damage) ==
                    Result::ok);
        std::optional<TurretFireEvent> firing;
        require(turret.advance(dt, std::span(&target, 1), true, firing) == Result::ok);
        if (firing) {
            ++fired;
            salvo.begin({firing->origin, firing->heading, firing->pitch, firing->speed});
        }
        std::optional<WeaponSalvoShot> shot;
        require(salvo.advance(dt, turret.heading(), turret.pitch(), shot) == Result::ok);
        if (shot) {
            // Explicit synthetic muzzle fixture: this does not stand in for live mesh attachment.
            const Vec3 muzzle{shot->muzzle_index ? .2f : -.2f, 0, 1};
            bool launched{};
            require(launch_weapon_projectile(projectiles[shot->pool_index], spec, shot->heading,
                                             shot->pitch, shot->starting_speed, muzzle,
                                             launched) == Result::ok);
        }
        for (auto& projectile : projectiles) {
            require(advance_weapon_projectile_motion(projectile, spec, dt, 0) == Result::ok);
            if (!projectile.active)
                continue;
            const auto hit =
                weapon_projectile_vehicle_hit(projectile, -1, std::span(&collision_target, 1));
            if (hit.kind == ProjectileHitKind::vehicle) {
                ++hits;
                require(damage_vehicle_contact(health, hit.contact_side, 1) == Result::ok);
                stop_weapon_projectile(projectile, spec, 0);
            }
        }
        std::vector<std::uint32_t> state;
        auto number = [&](float f) { state.push_back(std::bit_cast<std::uint32_t>(f)); };
        auto vector = [&](Vec3 v) {
            number(v.x);
            number(v.y);
            number(v.z);
        };
        state.push_back(static_cast<std::uint32_t>(fired));
        state.push_back(static_cast<std::uint32_t>(hits));
        state.push_back(salvo.active());
        state.push_back(random_state);
        state.push_back(guided.active());
        number(guided.age());
        for (const auto& missile : guided.missiles()) {
            vector(missile.position);
            vector(missile.velocity);
            number(missile.heading);
            number(missile.jam_remaining);
            state.push_back(missile.impacted);
        }
        number(jammer.elapsed_53c);
        state.push_back(jammer.active_270);
        state.push_back(static_cast<std::uint32_t>(flame.pool.particles.size()));
        for (const auto& particle : flame.pool.particles) {
            vector(particle.position);
            vector(particle.velocity);
            number(particle.remaining);
            state.push_back(particle.color_argb);
            state.push_back(particle.atlas_frame);
        }
        number(turret.heading());
        number(turret.pitch());
        vector(turret.predicted_target());
        number(health.internal_hp);
        for (float hp : health.armor_hp)
            number(hp);
        for (const auto& projectile : projectiles) {
            vector(projectile.position);
            vector(projectile.velocity);
            number(projectile.age);
            state.push_back(projectile.active);
        }
        trace.push_back(std::move(state));
        return Result::ok;
    };
    while (trace.size() < 720)
        require(clock.advance(cadence[frame++ % cadence.size()], {}, tick) == Result::ok);
    require(fired > 0 && hits > 0 &&
            health.armor_hp[0] + health.armor_hp[1] + health.armor_hp[2] + health.armor_hp[3] <
                400);
    return trace;
}
} // namespace
int main() {
    const std::array<double, 1> reference_cadence{1.0 / 60};
    const auto reference = run(reference_cadence);
    for (double fps : {10, 15, 30, 60, 75, 120, 144, 240}) {
        const std::array<double, 1> cadence{1.0 / fps};
        require(run(cadence) == reference);
    }
    const std::array<double, 6> stalls{.35, 1.0 / 240, .9, 1.0 / 75, .007, .021};
    require(run(stalls) == reference);
    std::cout << "Combat turret/salvo/projectile/damage traces match for 720 fixed ticks\n";
}
