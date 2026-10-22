#pragma once
#include "game_src/unresolved/track_collision.hpp"
#include "game_src/unresolved/weapon_runtime.hpp"
#include <functional>
#include <optional>
#include <span>

namespace roadwars {
// Portable recovered fields of pooled projectile objects (0047dd34), not a named original class.
struct WeaponProjectileSpec {
    float acceleration{}, top_speed{};
    float diagonal_radius{}, corner_angle_degrees{}, wall_radius{};
    bool has_trail{};
};
struct WeaponProjectile {
    Vec3 position{}, previous_position{}, velocity{}, acceleration_vector{};
    float heading_degrees{}, pitch_degrees{}, scalar_speed{}, age{};
    bool active{}, stopping{};
};
struct WeaponProjectileTarget {
    std::size_t participant{};
    std::int32_t identity{};
    Vec3 position{};
    float height{};
    std::array<Vec3, 4> footprint{};
};
enum class ProjectileHitKind { none, vehicle, wall, ground };
struct WeaponProjectileHit {
    ProjectileHitKind kind{ProjectileHitKind::none};
    Vec3 point{};
    std::size_t participant{}, contact_side{};
};
struct WeaponProjectileImpulse {
    Vec3 force{}, application{};
};
Result weapon_projectile_impact_impulse(const WeaponProjectile&, const WeaponParameters&,
                                        Vec3 hit_point, WeaponProjectileImpulse&);
Result launch_weapon_projectile(WeaponProjectile&, const WeaponProjectileSpec&, float heading,
                                float pitch, float starting_speed, Vec3 muzzle, bool& launched);
Result advance_weapon_projectile_motion(WeaponProjectile&, const WeaponProjectileSpec&, float delta,
                                        std::size_t live_trail_particles);
WeaponProjectileHit weapon_projectile_vehicle_hit(const WeaponProjectile&,
                                                  std::int32_t owner_identity,
                                                  std::span<const WeaponProjectileTarget>,
                                                  float below_origin_height = .5f);
WeaponProjectileHit weapon_projectile_world_hit(const WeaponProjectile&,
                                                const WeaponProjectileSpec&,
                                                std::span<const WallRecord>, const GroundContact&);
WeaponProjectileHit weapon_projectile_world_hit(const WeaponProjectile&,
                                                const WeaponProjectileSpec&, const TrackCollision&);
void stop_weapon_projectile(WeaponProjectile&, const WeaponProjectileSpec&,
                            std::size_t live_trail_particles);

struct WeaponSalvoShot {
    std::size_t pool_index{}, muzzle_index{};
    float heading{}, pitch{}, starting_speed{};
};
struct WeaponMuzzleFlash {
    float remaining{1}, local_roll{}, scale{1};
    std::uint32_t color_argb{0xffffffff};
    bool visible{};
};
// Portable state of the five confirmed shared-projectile weapon implementations.
class WeaponSalvoRuntime {
  public:
    Result initialize(std::string_view weapon_name, std::string& error);
    bool begin(const WeaponFireRequest&);
    Result advance(float delta, float current_heading, float current_pitch,
                   std::optional<WeaponSalvoShot>&, float parent_roll = 0);
    std::span<const WeaponMuzzleFlash> muzzle_flashes() const;
    bool ready() const { return initialized_ && ready_ && !active_; }
    bool active() const { return active_; }
    float elapsed() const { return elapsed_; }
    void cancel() { active_ = false; }
    std::size_t pool_capacity() const;
    std::string_view muzzle_name(std::size_t index) const;
    bool applies_impact_impulse() const { return kind_ == Kind::vulcan; }
    WeaponProjectileSpec projectile_spec(const WeaponParameters&) const;

  private:
    enum class Kind { machine, cannon, plasma, vulcan, gatling };
    Kind kind_{};
    bool initialized_{}, active_{}, ready_{true};
    float elapsed_{}, cadence_{}, launch_speed_{};
    std::size_t pool_index_{}, muzzle_index_{};
    std::array<WeaponMuzzleFlash, 5> flashes_;
};
struct WeaponRocketShape {
    float length{}, width{}, height{};
};
struct WeaponRocket {
    WeaponProjectile motion;
    Vec3 direction{};
    float horizontal_speed{}, vertical_speed{};
    float flare_roll{};
    bool ready{true}, callback_running{}, impacted{};
};
Result launch_weapon_rocket(WeaponRocket&, const WeaponParameters&, const WeaponFireRequest&,
                            bool& launched);
Result advance_weapon_rocket(WeaponRocket&, const WeaponParameters&, const WeaponRocketShape&,
                             float delta, bool trail_empty, std::int32_t owner_identity,
                             std::span<const WeaponProjectileTarget>, const TrackCollision&,
                             WeaponProjectileHit&,
                             const std::function<Result(bool, Vec3, float)>& advance_trail = {});
Result weapon_rocket_impact_impulse(const WeaponRocket&, const WeaponParameters&, Vec3 hit_point,
                                    WeaponProjectileImpulse&);
} // namespace roadwars
