#pragma once
#include "game_src/unresolved/collision_audio.hpp"
#include "game_src/unresolved/contact_orientation.hpp"
#include "game_src/unresolved/particle_effects.hpp"
#include "game_src/unresolved/race_loadout.hpp"
#include "game_src/unresolved/resource_store.hpp"
#include "game_src/unresolved/tire_audio.hpp"
#include "game_src/unresolved/track_collision.hpp"
#include "game_src/unresolved/vehicle_combat.hpp"
#include "game_src/unresolved/vehicle_damage_visuals.hpp"
#include "game_src/unresolved/vehicle_equipment_effects.hpp"
#include <functional>
#include <memory>

namespace roadwars {
// New portable representation of evidenced fields; original class ownership is unresolved.
struct VehicleParameters {
    TireSelection tires;
    int tire_kind{}; // 00412834: Street=0, Slicks=1, Off Road=2.
    float mass_kg{}, torque_nm{}, frontal_area_m2{}, drag_coefficient{};
    float rear_end_ratio{}, max_wheel_angle_degrees{};
    std::vector<float> gear_ratios;
    bool automatic_transmission{true};
    float rear_wheel_radius{}, length{}, width{}, height{}, wheelbase{};
    float tire_slip_lower_degrees{}, tire_slip_upper_degrees{};
    float tire_half_width{};
    std::array<Vec3, 4> wheel_ground_anchors{};
    std::array<Vec3, 4> wheel_axle_anchors{};
    Vec3 body_local_position{};
    float front_extension{};
    std::array<std::int32_t, 7> engine_upgrades{};
    float inverse_mass{}, gravity_force{}, quarter_gravity_force{}, drag_factor{};
    float partial_brake_factor{}, yaw_inertia{}, pitch_roll_inertia{};
    float added_tire_friction{}, fixed_angular_radius{};
    std::array<float, 4> positive_yaw_levers{}, negative_yaw_levers{};
};
struct VehicleForceLimits {
    float traction{}, partial_brake{}, full_brake{}, lateral{}, yaw_damping{};
    std::array<float, 4> rolling{};
};
struct VehicleState {
    Vec3 position{}, velocity{};
    float heading_degrees{}, pitch_degrees{}, roll_degrees{};
    float yaw_rate{}, pitch_rate{}, roll_rate{}, rpm{}, cached_speed{};
    float steering_degrees{};
    float cached_slip_angle{};
    float engine_throttle{}, brake_amount{};
    std::size_t gear_index{};
    bool reverse_engaged{}, upshifted{};
    std::array<float, 4> air_time{};
};
Result load_vehicle_metrics(ResourceStore& resources, std::string_view vehicle,
                            VehicleParameters& output, std::string& error);
Result finalize_vehicle_parameters(VehicleParameters& parameters);
Result load_vehicle_geometry(ResourceStore& resources, std::string_view vehicle,
                             VehicleParameters& parameters, std::string& error);
VehicleForceLimits vehicle_force_limits(const VehicleParameters& parameters,
                                        const std::array<float, 4>& longitudinal,
                                        const std::array<float, 4>& lateral,
                                        const std::array<float, 4>& rolling);
void apply_vehicle_impulse(Vec3& velocity, Vec3 direction, float impulse, float inverse_mass,
                           bool braking);
void apply_angular_delta(float& rate, float delta, bool braking);
float vehicle_drive_force(const VehicleParameters& parameters, std::size_t gear, float health_ratio,
                          float traction_limit, float difficulty_multiplier = 1.0f);
float vehicle_rpm(const VehicleParameters& parameters, const VehicleState& state, std::size_t gear,
                  float throttle);
void update_automatic_transmission(const VehicleParameters& parameters, VehicleState& state);
void apply_vehicle_engine(const VehicleParameters& parameters, VehicleState& state, Vec3 forward,
                          float throttle, float drive_force, float dt);
void apply_vehicle_brake(const VehicleParameters& parameters, VehicleState& state,
                         const VehicleForceLimits& limits, float brake, float dt);
void apply_vehicle_drag(const VehicleParameters& parameters, VehicleState& state,
                        Vec3 original_wind, float dt);
void apply_vehicle_tilt(const VehicleParameters& parameters, VehicleState& state,
                        const std::array<Vec3, 4>& wheels, Vec3 force, Vec3 application, float dt,
                        bool fixed_radius, bool braking, bool pitch_only = false);
void apply_vehicle_yaw_impulse(const VehicleParameters& parameters, VehicleState& state, Vec3 force,
                               Vec3 application, float dt, bool braking, float fixed_lever = -1.0f);
struct VehicleControls {
    float engine_throttle{}, brake_amount{}, steering_degrees{};
};
class VehicleDynamics {
  public:
    Result initialize(VehicleParameters parameters, const TrackCollision& track,
                      std::string& error);
    void update(float delta_seconds, const InputState& input);
    void update_controlled(float delta_seconds, const VehicleControls& controls);
    void begin_race_update(float delta_seconds, const InputState& input) {
        begin_update(delta_seconds, input, nullptr);
    }
    void begin_race_update(float delta_seconds, const VehicleControls& controls) {
        begin_update(delta_seconds, {}, &controls);
    }
    void solve_race_walls(float delta_seconds);
    void finish_race_update(float delta_seconds);
    void reset_to(const TrackSpawn& spawn);
    // Presentation metadata only; never enters force/contact calculations.
    std::uint64_t pose_generation() const { return pose_generation_; }
    void set_respawn_handler(std::function<TrackSpawn()> handler) {
        respawn_handler_ = std::move(handler);
    }
    // Borrowed only by the portable race's recovered pair-collision phase.
    VehicleState& collision_state() { return state_; }
    void refresh_after_pair_collision() { cache_wheels(); }
    // Replay reuses the native vehicle's current dynamic/presentation baseline.
    void seed_replay_state(const VehicleDynamics& source) {
        state_ = source.state_;
        body_ = source.body_;
        contacts_ = source.contacts_;
        wheel_points_ = source.wheel_points_;
        local_wheel_heights_ = source.local_wheel_heights_;
        tire_particle_sources_ = source.tire_particle_sources_;
        tire_audio_input_ = source.tire_audio_input_;
        oil_contacts_ = source.oil_contacts_;
        disabled_contact_1c_ = source.disabled_contact_1c_;
    }
    void commit_replay_state(VehicleDynamics& source) {
        seed_replay_state(source);
        equipment_ = source.equipment_;
        combat_ = std::move(source.combat_);
        damage_visuals_ = std::move(source.damage_visuals_);
        collision_audio_ = std::move(source.collision_audio_);
        particle_impacts_ = std::move(source.particle_impacts_);
    }
    void append_pair_collision_event(float strength, Vec3 position) {
        collision_audio_.vehicle_contact(strength, position);
    }
    float longitudinal_grip() const { return longitudinal_grip_; }
    const VehicleState& state() const { return state_; }
    const ContactOrientationState& body_orientation() const { return body_; }
    const TireAudioInput& tire_audio_input() const { return tire_audio_input_; }
    Transform transform() const;
    const VehicleParameters& parameters() const { return parameters_; }
    VehicleEquipmentEffects& equipment() { return equipment_; }
    const VehicleEquipmentEffects& equipment() const { return equipment_; }
    void set_equipment(VehicleEquipmentEffects equipment) { equipment_ = std::move(equipment); }
    VehicleCombat* combat() { return combat_.get(); }
    const VehicleCombat* combat() const { return combat_.get(); }
    void set_combat(std::unique_ptr<VehicleCombat> combat) {
        combat_ = std::move(combat);
        const auto* contact = combat_ ? combat_->contact_weapon() : nullptr;
        parameters_.front_extension = contact ? contact->front_extension() : 0;
    }
    VehicleDamageVisuals* damage_visuals() { return damage_visuals_.get(); }
    const VehicleDamageVisuals* damage_visuals() const { return damage_visuals_.get(); }
    void set_damage_visuals(std::unique_ptr<VehicleDamageVisuals> value) {
        damage_visuals_ = std::move(value);
    }
    void
    set_damage_visual_handler(std::function<Result(const VehicleDamageVisualContact&)> handler) {
        damage_visual_handler_ = std::move(handler);
    }
    Result apply_damage_visual_contact(const VehicleDamageVisualContact& contact) {
        return damage_visual_handler_ ? damage_visual_handler_(contact) : Result::unavailable;
    }
    const std::array<Vec3, 4>& wheel_points() const { return wheel_points_; }
    void set_particle_impact_handler(std::function<void(const ParticleImpact&)> handler) {
        particle_impact_handler_ = std::move(handler);
    }
    void append_particle_impact(const ParticleImpact& impact) {
        particle_impacts_.push_back(impact);
        if (particle_impact_handler_)
            particle_impact_handler_(impact);
    }
    void set_elimination_handler(std::function<Result()> handler) {
        elimination_handler_ = std::move(handler);
    }
    Result notify_internal_damage(float previous_hp) {
        return previous_hp > 0 && equipment_.internal_hp <= 0 && elimination_handler_
                   ? elimination_handler_()
                   : Result::ok;
    }
    void set_collision_damage_handler(std::function<Result(Vec3, Vec3)> handler) {
        collision_damage_handler_ = std::move(handler);
    }
    Result collision_damage_result() const { return collision_damage_result_; }
    void set_oil_contacts(unsigned count) { oil_contacts_ = count; }
    void note_disabled_pair_contact() { disabled_contact_1c_ = 1; }
    float disabled_contact_1c() const { return disabled_contact_1c_; }
    const std::array<float, 4>& local_wheel_heights() const { return local_wheel_heights_; }
    std::uint64_t wall_contact_count() const { return wall_contact_count_; }
    std::span<const ParticleImpact> particle_impacts() const { return particle_impacts_; }
    std::span<const CollisionAudioEvent> collision_audio_events() const {
        return collision_audio_.events();
    }
    Result advance_replay_collision_audio(float delta) { return collision_audio_.advance(delta); }
    const std::array<TireParticleSource, 4>& tire_particle_sources() const {
        return tire_particle_sources_;
    }

  private:
    void update_internal(float, const InputState&, const VehicleControls*);
    void begin_update(float, const InputState&, const VehicleControls*);
    VehicleState step_previous_;
    std::array<GroundContact, 4> step_effect_contacts_;
    float step_effect_speed_{};
    bool step_pending_{};
    std::function<TrackSpawn()> respawn_handler_;
    float longitudinal_grip_{1};
    TireAudioInput tire_audio_input_{};
    VehicleParameters parameters_{};
    VehicleEquipmentEffects equipment_{};
    std::unique_ptr<VehicleCombat> combat_;
    unsigned oil_contacts_{};
    float disabled_contact_1c_{};
    std::uint64_t pose_generation_{};
    VehicleState state_{};
    const TrackCollision* track_{};
    ContactOrientationState body_{};
    std::array<Vec3, 4> wheel_points_{};
    std::array<GroundContact, 4> contacts_{};
    std::array<float, 4> local_wheel_heights_{};
    std::uint64_t wall_contact_count_{};
    std::vector<ParticleImpact> particle_impacts_;
    std::function<void(const ParticleImpact&)> particle_impact_handler_;
    std::unique_ptr<VehicleDamageVisuals> damage_visuals_;
    std::function<Result(const VehicleDamageVisualContact&)> damage_visual_handler_;
    std::function<Result(Vec3, Vec3)> collision_damage_handler_;
    std::function<Result()> elimination_handler_;
    Result collision_damage_result_{Result::ok};
    CollisionAudio collision_audio_;
    std::array<TireParticleSource, 4> tire_particle_sources_{};
    void cache_wheels(bool sample_contacts = true);
};
} // namespace roadwars
