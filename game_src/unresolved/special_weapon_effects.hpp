#pragma once
#include "game_src/interfaces/host.hpp"
#include <array>
#include <functional>
#include <optional>
#include <span>
#include <string>

namespace roadwars {
// Portable representations of confirmed weapon/vehicle fields, not recovered class names.
struct SpecialWeaponTarget {
    std::size_t participant{};
    std::uint64_t chassis_identity{};
    std::array<Vec3, 4> footprint{};
};
struct SpecialWeaponDamage {
    std::size_t participant{}, contact_side{};
    float damage{};
};
struct ElectronCannonState {
    bool active_270{}, ready_6c0{true}, beam_attached{};
    float elapsed_6b8{}, sound_timer_6bc{};
    float range_6d0{10}, width_6d4{1.5f}, height_6d8{1.5f};
};
struct ElectronCannonOutput {
    std::vector<SpecialWeaponDamage> damage;
    std::optional<Vec3> hit_point;
    float beam_length{};
    bool play_hit_sound{}, deactivate{}, remove_beam{}, remove_callback{};
};
bool electron_cannon_can_activate(const ElectronCannonState&);
// Call only after a successful shared004cf5ed activation.
void begin_electron_cannon(ElectronCannonState&);
Result advance_electron_cannon(ElectronCannonState&, std::uint64_t owner_identity,
                               Vec3 original_muzzle, float original_heading,
                               float damage_per_second, std::span<const SpecialWeaponTarget>,
                               std::size_t live_impact_particles, float dt, ElectronCannonOutput&,
                               std::string& error);

struct FlameParticle {
    Vec3 position{}, velocity{};
    float remaining{1}, hue{};
    std::uint32_t color_argb{}, atlas_frame{};
};
struct FlameParticlePool {
    float emission_timer_293c{};
    // Original AddHead iteration order, newest first, capacity100.
    std::vector<FlameParticle> particles;
};
Result advance_flame_particles(FlameParticlePool&, bool emitting, Vec3 original_muzzle,
                               Vec3 original_velocity, float dt,
                               const std::function<std::uint32_t()>& shared_random,
                               std::string& error);
struct FlameThrowerState {
    bool active_270{}, ready_2f30{true};
    float elapsed_2f2c{};
    FlameParticlePool pool;
};
struct FlameThrowerOutput {
    std::vector<SpecialWeaponDamage> damage;
    bool deactivate{}, remove_callback{};
};
bool flame_thrower_can_activate(const FlameThrowerState&);
void begin_flame_thrower(FlameThrowerState&);
// Particle velocity is the actual vehicle+5b10 forward vector scaled by cached_speed+10.
Result advance_flame_thrower(FlameThrowerState&, std::uint64_t owner_identity, Vec3 original_muzzle,
                             Vec3 original_particle_velocity, float damage_per_second,
                             std::span<const SpecialWeaponTarget>, float dt,
                             const std::function<std::uint32_t()>& shared_random,
                             FlameThrowerOutput&, std::string& error);
} // namespace roadwars
