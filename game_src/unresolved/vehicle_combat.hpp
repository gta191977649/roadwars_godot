#pragma once
#include "game_src/unresolved/combat_audio.hpp"
#include "game_src/unresolved/contact_weapon_runtime.hpp"
#include "game_src/unresolved/guided_weapon_runtime.hpp"
#include "game_src/unresolved/mesh_scene.hpp"
#include "game_src/unresolved/rear_weapon_effects.hpp"
#include "game_src/unresolved/rear_weapon_instances.hpp"
#include "game_src/unresolved/skeletal_mesh_runtime.hpp"
#include "game_src/unresolved/special_weapon_effects.hpp"
#include "game_src/unresolved/weapon_projectiles.hpp"

namespace roadwars {
struct VehicleCombatTarget {
    std::size_t participant{};
    std::int32_t identity{};
    Vec3 position{};
    float height{};
    std::array<Vec3, 4> footprint{};
    std::array<float, 4> air_time{};
    Vec3 velocity{};
};
struct VehicleCombatFrame {
    std::int32_t owner_identity{};
    Transform vehicle_pose;
    Vec3 forward{};
    float heading{}, pitch{}, cached_speed{};
    int route_node{-1};
    bool on_branch{};
    std::span<const VehicleCombatTarget> targets;
    std::optional<GuidedWeaponTarget> guided_target;
    std::span<const JammerWeaponView> jammer_weapons;
    float roll{};
    float body_pitch{}, body_roll{};
};
struct VehicleCombatDamage {
    WeaponSlot slot{};
    std::size_t participant{}, contact_side{};
    float hitpoints{};
    Vec3 point{};
    bool internal_damage{}, has_impulse{}, apply_yaw{};
    WeaponProjectileImpulse impulse;
};
enum class VehicleCombatEffectKind { projectile_trail, guided_trail, electron_impact, smoke };
struct VehicleCombatEffectKey {
    WeaponSlot slot{};
    std::size_t projectile{};
    VehicleCombatEffectKind kind{};
};
struct VehicleCombatEffectStep {
    VehicleCombatEffectKey key;
    float delta{};
    bool emitting{};
    Vec3 position{}, velocity{};
    float heading{}, pitch{};
};
struct VehicleCombatImpact {
    WeaponSlot slot{};
    std::size_t projectile{};
    WeaponProjectileHit hit;
    Vec3 velocity{};
    float heading{}, pitch{};
    bool rear_explosion{}, extra_reactive_effect{};
};
struct VehicleCombatServices {
    // 0044f365 refreshes the query and latches a distinct active target once at launch.
    std::function<Result()> select_guided_target;
    std::function<std::uint32_t()> random;
    std::function<Result(const VehicleCombatDamage&)> damage;
    // Counts/updates must use the actual original pools; no invented empty-pool fallback.
    std::function<Result(VehicleCombatEffectKey, std::size_t&)> effect_count;
    std::function<Result(const VehicleCombatEffectStep&)> effect_step;
    // Registers the distinct native explosion/impact callbacks, including their shared RNG.
    std::function<Result(const VehicleCombatImpact&)> impact;
    // Index refers to VehicleCombatFrame::jammer_weapons. Original duration is one second.
    std::function<Result(std::size_t, float)> jam;
};
struct VehicleCombatAsset {
    const WeaponDescriptor* descriptor{};
    WeaponParameters parameters;
    MeshObject weapon, ammunition;
    std::string mount_name;
    std::vector<std::size_t> mount_path;
    std::vector<Vec3> muzzle_positions;
    std::vector<MeshObject> muzzle_meshes;
    MeshObject flare_mesh;
    Vec3 flare_position{};
    bool has_flare_mesh{};
    std::array<MeshObject, 2> leg_meshes;
    std::array<Vec3, 2> leg_positions;
    bool has_leg_meshes{};
    std::array<MeshObject, 2> mount_housing_meshes;
    std::array<std::vector<std::size_t>, 2> mount_housing_paths;
    bool has_mount_housing{};
    WeaponRocketShape shape;
    bool has_weapon_mesh{}, has_ammunition_mesh{};
};
enum class VehicleCombatVisualKind {
    mounted,
    projectile,
    rear_instance,
    muzzle_flash,
    missile_flare,
    rear_leg,
    mount_housing
};
struct VehicleCombatVisual {
    WeaponSlot slot{};
    std::size_t index{};
    VehicleCombatVisualKind kind{};
    Transform pose;
    bool visible{};
    std::uint32_t color_argb{0xffffffff};
};
struct VehicleCombatSlotView {
    const WeaponDescriptor* descriptor{};
    WeaponParameters parameters;
    std::int32_t ammunition{};
    bool active{}, ready{}, pending{};
    float deployment_roll{};
    std::optional<float> elapsed;
};
struct VehicleCombatOilVisual {
    std::array<Vec3, 3> triangle;
    std::uint32_t atlas_frame{};
};
// New portable coordinator of evidenced original objects. This is not an original class name.
class VehicleCombat {
  public:
    Result load(ResourceStore&, std::string_view car, const std::array<std::string, 3>& weapons,
                std::string& error);
    bool request(WeaponSlot, float cached_speed);
    bool request(WeaponSlot, float cached_speed, float owner_heading, float owner_pitch,
                 float body_pitch = 0);
    Result advance(float delta, const VehicleCombatFrame&, const TrackCollision&, const RaceRoute*,
                   const VehicleCombatServices&, std::string& error);
    using CallbackToken = std::uint64_t;
    void observe_callbacks(std::function<void(CallbackToken, bool)>);
    std::vector<CallbackToken> callback_tokens() const;
    Result dispatch_callback(CallbackToken, float delta, const VehicleCombatFrame&,
                             const TrackCollision&, const RaceRoute*, const VehicleCombatServices&,
                             std::string& error);
    Result jam(float seconds);
    void cancel();
    VehicleCombatSlotView slot(WeaponSlot) const;
    std::span<const VehicleCombatAsset> assets() const { return assets_; }
    std::span<const VehicleCombatVisual> visuals() const { return visuals_; }
    std::vector<CombatAudioEvent> drain_audio_events();
    std::span<const GuidedMissileState> guided_missiles() const;
    const ContactWeaponRuntime* contact_weapon() const;
    const OilSlickState* oil_slick() const;
    const FlameParticlePool* flame_particles() const;
    void refresh_visuals(const VehicleCombatFrame& frame) { update_visuals(frame); }
    std::optional<Transform> node_world_pose(std::string_view, const VehicleCombatFrame&) const;
    const ElectronCannonState* electron_state() const;
    const ElectronCannonOutput* electron_output() const;
    const SkeletalMeshRuntime* electron_geometry() const;
    std::optional<Transform> electron_beam_pose() const;
    const MissileJammerOutput* jammer_output() const;
    std::span<const VehicleCombatOilVisual> oil_visual_triangles() const;

  private:
    enum class Kind {
        none,
        salvo,
        rocket,
        guided,
        contact,
        electron,
        flame,
        jammer,
        oil,
        smoke,
        rear
    };
    struct Item {
        Kind kind{};
        WeaponMountRuntime mount;
        std::vector<Vec3> attached_missile_offsets;
        bool missile_offsets_prepared{};
        bool missile_flare_enabled{};
        WeaponSalvoRuntime salvo;
        std::vector<WeaponProjectile> projectiles;
        WeaponRocket rocket;
        GuidedWeaponRuntime guided;
        ContactWeaponRuntime contact;
        ElectronCannonState electron;
        ElectronCannonOutput electron_output;
        SkeletalMeshRuntime electron_mesh;
        Transform beam_pose;
        FlameThrowerState flame;
        MissileJammerState jammer;
        MissileJammerOutput jammer_output;
        OilSlickState oil;
        std::vector<VehicleCombatOilVisual> oil_visual_triangles;
        std::array<unsigned, 5> oil_render_counts{};
        SmokeScreenState smoke;
        RearDeployKind deploy_kind{};
        RearDeployPoolState deployment;
        std::array<RearWeaponInstance, 5> instances;
        float launch_pitch{};
    };
    enum class CallbackKind { mount, weapon, projectile, rear_instance };
    struct Callback {
        CallbackKind kind{};
        std::size_t slot{}, index{};
        bool live{true};
    };
    bool active(std::size_t) const;
    bool ready(std::size_t) const;
    void register_callback(CallbackKind, std::size_t, std::size_t = 0);
    static CallbackToken callback_token(const Callback&);
    void remove_callback(std::size_t);
    Result begin(std::size_t, const WeaponFireRequest&, const VehicleCombatFrame&,
                 const VehicleCombatServices&, std::string& error);
    Result update_weapon(std::size_t, float, const VehicleCombatFrame&, const TrackCollision&,
                         const RaceRoute*, const VehicleCombatServices&, std::string& error);
    Transform mounted_pose(std::size_t, const VehicleCombatFrame&) const;
    Vec3 mounted_angles(std::size_t, const VehicleCombatFrame&) const;
    void prepare_missile_attachment(std::size_t, float heading, float pitch);
    void update_visuals(const VehicleCombatFrame&);
    std::array<VehicleCombatAsset, 3> assets_;
    std::array<Item, 3> items_;
    MeshObject body_;
    std::vector<std::size_t> roll_path_;
    std::vector<Callback> callbacks_;
    std::vector<VehicleCombatVisual> visuals_;
    std::vector<CombatAudioEvent> audio_events_;
    std::function<void(CallbackToken, bool)> callback_observer_;
    std::optional<CallbackToken> dispatch_filter_;
    bool initialized_{};
};
} // namespace roadwars
