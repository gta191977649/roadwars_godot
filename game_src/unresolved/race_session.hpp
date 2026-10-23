#pragma once
#include "game_src/interfaces/race_debug.hpp"
#include "game_src/unresolved/chimney_particles.hpp"
#include "game_src/unresolved/combat_audio.hpp"
#include "game_src/unresolved/race_ai.hpp"
#include "game_src/unresolved/race_ai_combat.hpp"
#include "game_src/unresolved/race_callback_registry.hpp"
#include "game_src/unresolved/race_frontend.hpp"
#include "game_src/unresolved/race_start.hpp"
#include "game_src/unresolved/scene_electron_traps.hpp"
#include "game_src/unresolved/scene_turret_combat.hpp"
#include "game_src/unresolved/scene_weather.hpp"
#include "game_src/unresolved/vehicle_damage_particles.hpp"
#include "game_src/unresolved/vehicle_equipment_effects.hpp"
#include "game_src/unresolved/weapon_trails.hpp"
#include <memory>
#include <optional>

namespace roadwars {
struct RaceOptions {
    std::string track{"Dark Forest"}, car{"Animal"};
    int laps{3}, ai_count{5};
    std::uint32_t seed{1}; // Diagnostic seed, not an original startup default claim.
    std::optional<RaceLoadout> player_loadout;
    int diagnostic_weapon_lock_seconds{
        20}; // Fresh native Single Race default; diagnostic override accepts0 or20.
};
enum class RacePhase { countdown, racing, finished };
enum class RaceEndReason { none, completed, eliminated };
struct RaceParticipant {
    std::string character;
    RaceLoadout loadout;
    std::unique_ptr<VehicleDynamics> owned_vehicle;
    VehicleDynamics* vehicle{};
    RaceAi ai;
    OriginalChaseCamera chase_camera;
    CameraFrame camera;
    RaceRouteProgress progress;
    std::array<float, 10> lap_times{};
    double finish_time{};
    int rank{};
    RaceAiCombatState combat_ai;
    RaceAiCombatTrace combat_trace;
    unsigned weapon_mounts{};
    int side_target{-1}, active_side_target{-1};
    bool turret_exempt{};
    bool passive_jammer{};
    Vec3 rear_mount_local{}, hood_point_local{};
    float reset_timer_14{}, reset_timer_10{};
};
// Portable owner inferred from race+0348/+034c and vehicle+4418 links.
// Original C++ class name and module remain unresolved.
class RaceSession {
  public:
    ~RaceSession();
    Result load(ResourceStore&, const TrackCollision&, const RaceOptions&, VehicleDynamics& player,
                std::string& error);
    Result advance(float delta, const InputState&, std::string& error);
    void set_paused(bool value) { paused_ = value; }
    bool paused() const { return paused_; }
    std::span<const AmmunitionNotice> ammunition_notices() const { return ammunition_notices_.entries(); }
    void set_player_autopilot(bool value) { player_autopilot_ = value; }
    Result restart(std::string& error);
    std::span<const RaceParticipant> racers() const { return racers_; }
    const RaceRoute& route() const { return route_; }
    const CameraFrame& camera_for(int ai) const {
        return ai >= 0 && static_cast<std::size_t>(ai) < player_index() ? racers_[ai].camera
                                                                        : start_.camera();
    }
    const RaceOptions& options() const { return options_; }
    float special_effects_scalar() const { return special_effects_scalar_; }
    std::size_t player_index() const { return racers_.empty() ? 0 : racers_.size() - 1; }
    RacePhase phase() const { return phase_; }
    RaceEndReason ending() const { return ending_; }
    bool idle_return_requested() const { return idle_return_requested_; }
    float idle_return_remaining() const { return idle_return_timer_; }
    void acknowledge_idle_menu() { idle_return_requested_ = false; }
    void toggle_time_table() { time_table_remaining_ = time_table_remaining_ == 0 ? 15.f : 0.f; }
    float time_table_remaining() const { return time_table_remaining_; }
    Result select_native_camera(int mode);
    int native_camera_mode() const { return native_camera_mode_; }
    std::size_t spectator_target() const { return spectator_target_; }
    CameraFrame spectator_camera() const;
    double time() const { return race_time_; }
    float countdown() const { return countdown_; }
    RaceStart& start_sequence() { return start_; }
    const RaceStart& start_sequence() const { return start_; }
    std::uint64_t ticks() const { return ticks_; }
    std::uint32_t random_state() const { return random_state_; }
    struct DispatchEvent {
        enum class Stage { controller, force };
        Stage stage;
        std::size_t participant;
    };
    std::span<const DispatchEvent> dispatch_trace() const { return dispatch_trace_; }
    FrontendFrame frontend_frame(std::vector<FrontendRacer>& storage) const;
    RaceDebugSnapshot debug_snapshot(bool include_route_geometry = true) const;
    std::vector<std::size_t> ranking() const { return ranking_; }
    const SceneTurretCombat& scene_combat() const { return scene_combat_; }
    struct SceneTrail {
        SceneCombatTrailKey key;
        WeaponTrailPool pool;
    };
    std::span<const SceneTrail> scene_trails() const { return scene_trails_; }
    struct VehicleTrail {
        std::size_t participant{};
        VehicleCombatEffectKey key;
        WeaponTrailPool pool;
    };
    std::span<const VehicleTrail> vehicle_trails() const { return vehicle_trails_; }
    std::span<const WeaponTrailPool> nitrous_trails() const { return nitrous_trails_; }
    int selected_hud_item() const { return selected_hud_item_; }
    struct Impact {
        bool scene{};
        std::size_t owner{}, projectile{};
        WeaponSlot slot{};
        WeaponImpactPool pool;
        VulcanImpactSprite sprite;
    };
    std::span<const Impact> impact_pools() const { return impact_pools_; }
    std::span<const std::array<WeaponRearBlastPool, 5>> rear_blasts() const { return rear_blasts_; }
    std::span<const std::array<WeaponRearBlastPool, 5>> rear_blast_pools() const {
        return rear_blasts_;
    }
    struct VehicleParticles {
        ParticleEmitter sparks;
        std::array<TireParticleEmitter, 4> tires;
        VehicleDamageParticles damage;
    };
    std::span<const VehicleParticles> vehicle_particles() const { return vehicle_particles_; }
    const SceneElectronTraps& scene_electron_traps() const { return scene_electron_traps_; }
    std::span<const WeaponTrailPool> scene_electron_trails() const {
        return scene_electron_trails_;
    }
    const SceneWeather& weather() const { return weather_; }
    struct Chimney {
        Vec3 position;
        ChimneyParticles pool;
    };
    std::span<const Chimney> chimneys() const { return chimneys_; }
    const CombatAudioResources& audio_resources() const { return audio_resources_; }
    std::span<const CombatAudioEvent> audio_events() const { return audio_events_; }
    struct AudioLoopCommand {
        std::size_t voice{};
        CombatAudioCommand command;
    };
    std::span<const AudioLoopCommand> audio_loop_commands() const { return audio_loop_commands_; }
    void set_audio_voice_alive(std::size_t voice, bool alive) {
        if (voice < audio_voice_alive_.size())
            audio_voice_alive_[voice] = alive;
    }

  private:
    TrackSpawn rescue(std::size_t) const;
    std::uint32_t random();
    void update_ranking();
    Result combat_commands(std::size_t, float, const InputState&, std::string&);
    void activate_nitrous(std::size_t);
    Result advance_combat(float, std::string&);
    Result advance_audio(float, std::string&);
    Result refresh_side_target(std::size_t, std::string&);
    std::vector<RaceAiCombatVehicleView> combat_views() const;
    Result apply_combat_damage(std::size_t, std::size_t, float, bool, bool, bool,
                               WeaponProjectileImpulse, std::string&);
    RaceStart start_;
    RaceOptions options_;
    RaceRoute route_;
    std::vector<RaceParticipant> racers_;
    std::vector<VehicleEquipmentEffects> initial_equipment_;
    std::vector<TrackSpawn> spawns_;
    std::vector<std::size_t> ranking_, finished_;
    std::vector<RaceDebugLine> collision_lines_;
    std::vector<DispatchEvent> dispatch_trace_;
    SceneTurretCombat scene_combat_;
    SceneElectronTraps scene_electron_traps_;
    std::vector<WeaponTrailPool> scene_electron_trails_;
    MeshObject scene_world_;
    SceneWeather weather_;
    std::vector<Chimney> chimneys_;
    float chimney_view_distance_{};
    bool chimneys_enabled_{};
    std::vector<SceneTrail> scene_trails_;
    std::vector<VehicleTrail> vehicle_trails_;
    std::vector<WeaponTrailPool> nitrous_trails_;
    std::vector<Impact> impact_pools_;
    std::vector<std::array<WeaponRearBlastPool, 5>> rear_blasts_;
    RaceCallbackRegistry callbacks_;
    std::vector<VehicleParticles> vehicle_particles_;
    CombatAudioResources audio_resources_;
    std::vector<CombatAudioEvent> audio_events_;
    std::vector<AudioLoopCommand> audio_loop_commands_;
    std::vector<bool> audio_voice_alive_;
    CombatRaceLoopAudio fire_audio_, nitrous_audio_;
    RaceWeaponInputState weapon_input_;
    int selected_hud_item_{};
    AmmunitionNotices ammunition_notices_;
    float special_effects_scalar_{};
    int difficulty_{}, weapon_lock_seconds_{};
    ResourceStore* resources_{};
    const TrackCollision* collision_{};
    float initial_countdown_{}, countdown_{};
    bool warning_countdown_ready_{};
    double race_time_{};
    std::uint64_t ticks_{};
    std::uint32_t random_state_{};
    std::size_t scanner_{};
    bool paused_{}, player_autopilot_{};
    RacePhase phase_{RacePhase::countdown};
    RaceEndReason ending_{RaceEndReason::none};
    float idle_return_timer_{10};
    bool idle_return_requested_{};
    float time_table_remaining_{15};
    int native_camera_mode_{4};
    std::size_t spectator_target_{};
};
} // namespace roadwars
