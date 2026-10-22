#pragma once
#include "game_src/unresolved/combat_audio.hpp"
#include "game_src/unresolved/race_turrets.hpp"
#include "game_src/unresolved/weapon_projectiles.hpp"
#include <functional>

namespace roadwars {
struct SceneCombatTrailKey {
    std::size_t turret{}, projectile{};
    bool rocket{};
};
struct SceneCombatDamageEvent {
    std::size_t turret{}, participant{}, contact_side{};
    float hitpoints{};
    Vec3 point{};
    bool has_impulse{};
    WeaponProjectileImpulse impulse;
};
struct SceneCombatImpact {
    SceneCombatTrailKey key;
    WeaponProjectileHit hit;
    Vec3 velocity{};
    float heading{}, pitch{};
};
struct SceneCombatServices {
    std::function<Result(const SceneCombatDamageEvent&)> damage;
    // Both callbacks are required when the scene contains Plasma Blaster or Rocket Launcher.
    // Counts must come from their actual recovered trail pools; an invented empty flag is invalid.
    std::function<Result(SceneCombatTrailKey, std::size_t&)> trail_count;
    std::function<Result(SceneCombatTrailKey, float, bool, Vec3, Vec3)> advance_trail;
    std::function<Result(const SceneCombatImpact&)> impact;
};
struct SceneCombatWeaponAsset {
    std::string name;
    MeshObject weapon, ammunition;
    WeaponParameters parameters;
    WeaponRocketShape rocket_shape;
    std::vector<Vec3> muzzle_local_positions;
    std::vector<MeshObject> muzzle_meshes;
    MeshObject flare_mesh;
    Vec3 flare_position{};
    bool rocket{}, has_trail{};
};
struct SceneCombatProjectileVisual {
    std::size_t turret{}, asset{}, projectile{};
    Transform pose;
    bool visible{}, rocket{};
    bool muzzle_flash{};
    std::uint32_t color_argb{0xffffffff};
    bool missile_flare{};
};
// Portable integration of confirmed scene turret and weapon objects, not an original class name.
class SceneTurretCombat {
  public:
    Result load(ResourceStore&, std::string_view track, const MeshObject& world,
                std::string& error);
    Result advance(float delta, std::span<const TurretTarget>,
                   std::span<const WeaponProjectileTarget>, const TrackCollision&,
                   const SceneCombatServices&, std::string& error, bool weapons_unlocked = true);
    using CallbackToken = std::uint64_t;
    Result advance_controllers(float, std::span<const TurretTarget>, std::string& error,
                               bool weapons_unlocked = true);
    void observe_callbacks(std::function<void(CallbackToken, bool)>);
    std::vector<CallbackToken> callback_tokens() const;
    Result dispatch_callback(CallbackToken, float delta, std::span<const TurretTarget>,
                             std::span<const WeaponProjectileTarget>, const TrackCollision&,
                             const SceneCombatServices&, std::string& error,
                             bool weapons_unlocked = true);
    // The caller must reset its external trail pools together with this scene.
    void reset();
    const RaceTurrets& turrets() const { return turrets_; }
    std::span<const SceneCombatWeaponAsset> assets() const { return assets_; }
    std::span<const SceneCombatProjectileVisual> projectiles() const { return visuals_; }
    std::span<const SceneCombatDamageEvent> damage_events() const { return damage_events_; }
    std::vector<CombatAudioEvent> drain_audio_events();
    std::uint64_t launched_projectiles() const { return launched_; }
    std::uint64_t vehicle_impacts() const { return impacts_; }

  private:
    struct Item {
        std::size_t asset{};
        WeaponSalvoRuntime salvo;
        std::vector<WeaponProjectile> projectiles;
        WeaponRocket rocket;
    };
    enum class CallbackKind { controller, salvo, projectile, rocket };
    struct Callback {
        CallbackKind kind{};
        std::size_t turret{}, projectile{};
        bool live{true};
    };
    Result emit_damage(std::size_t, const WeaponProjectileHit&, const WeaponProjectile&,
                       const SceneCombatServices&, std::string& error);
    void update_visuals();
    static CallbackToken callback_token(const Callback&);
    void register_callback(CallbackKind, std::size_t, std::size_t = 0);
    void remove_callback(std::size_t);
    RaceTurrets turrets_;
    std::vector<Item> items_;
    std::vector<SceneCombatWeaponAsset> assets_;
    std::vector<Callback> callbacks_;
    std::vector<SceneCombatProjectileVisual> visuals_;
    std::vector<SceneCombatDamageEvent> damage_events_;
    std::vector<CombatAudioEvent> audio_events_;
    std::uint64_t launched_{}, impacts_{};
    bool requires_trails_{};
    std::function<void(CallbackToken, bool)> callback_observer_;
    std::optional<CallbackToken> dispatch_filter_;
};
} // namespace roadwars
