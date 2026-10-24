#include "game_src/unresolved/race_session.hpp"
#include "game_src/unresolved/wheel_pose.hpp"
#include <algorithm>
#include <cmath>
#include <type_traits>

namespace roadwars {
// The original temporarily reuses vehicle objects. Moving their live portable owners
// into this memento additionally preserves every callback/solver field across playback.
struct RaceSession::ReplaySavedState {
    std::vector<RaceParticipant> racers;
    VehicleDynamics player;
    std::vector<std::size_t> ranking, finished;
    std::vector<RaceCallbackKey> callbacks;
    SceneTurretCombat scene;
    SceneElectronTraps traps;
    std::vector<WeaponTrailPool> electron_trails, nitrous_trails;
    std::vector<SceneTrail> scene_trails;
    std::vector<VehicleTrail> vehicle_trails;
    std::vector<Impact> impacts;
    std::vector<std::array<WeaponRearBlastPool, 5>> rear_blasts;
    std::vector<VehicleParticles> particles;
    SceneWeather weather;
    std::vector<Chimney> chimneys;
    RaceStart start;
    OriginalChaseCamera chase, steering_chase;
    CameraFrame chase_frame;
    CombatRaceLoopAudio fire_audio, nitrous_audio;
    std::vector<CombatAudioEvent> audio_events;
    std::vector<AudioLoopCommand> audio_commands;
    std::vector<bool> audio_alive;
    std::vector<DispatchEvent> dispatch;
    std::vector<RaceDebugLine> collision_lines;
    AmmunitionNotices notices;
    RaceWeaponInputState weapon_input;
    std::uint32_t random{};
    std::uint64_t ticks{};
    std::size_t scanner{}, target{};
    double race_time{};
    float countdown{}, table{}, idle{};
    int camera_mode{}, hud_item{};
    bool paused{}, autopilot{}, idle_requested{}, warning_ready{};
    bool look_back{};
    RacePhase phase{};
    RaceEndReason ending{};
};

bool RaceSession::replay_available() const {
    return !replaying() && !replay_tracks_.empty() &&
           std::all_of(replay_tracks_.begin(), replay_tracks_.end(),
                       [](const auto& track) { return track.ready(); });
}
Result RaceSession::reset_replay_recording(std::string& error, bool preserve_spins) {
    if (replay_initializing_)
        return Result::ok;
    replay_tracks_.clear();
    replay_tracks_.resize(racers_.size());
    replay_previous_.clear();
    replay_clock_owner_.reset();
    if (!preserve_spins)
        replay_wheel_spins_.assign(racers_.size(), {});
    replay_nitrous_flags_.assign(racers_.size(), false);
    for (std::size_t i = 0; i < racers_.size(); ++i) {
        if (replay_tracks_[i].configure(replay_seconds_) != Result::ok) {
            error = "Invalid original REPLAYSECS";
            return Result::invalid_argument;
        }
        const auto& car = *racers_[i].vehicle;
        replay_previous_.push_back({car.equipment().internal_hp, car.equipment().armor_hp,
                                    car.damage_visuals()->marks().size(),
                                    car.damage_visuals()->deformations().size()});
    }
    return record_replay(0, error);
}
Result RaceSession::record_replay(float dt, std::string& error) {
    if (replaying() || replay_seconds_ == 0)
        return Result::ok;
    for (std::size_t i = 0; i < racers_.size(); ++i) {
        const auto& car = *racers_[i].vehicle;
        const auto& state = car.state();
        auto& previous = replay_previous_[i];
        auto& track = replay_tracks_[i];
        const auto& health = car.equipment();
        previous = {health.internal_hp, health.armor_hp, car.damage_visuals()->marks().size(),
                    car.damage_visuals()->deformations().size()};
        const auto forward = car.transform().basis[2];
        const float length =
            std::sqrt(state.velocity.x * state.velocity.x + state.velocity.y * state.velocity.y +
                      state.velocity.z * state.velocity.z);
        float angle{};
        if (length > 0)
            angle =
                std::acos(std::clamp((forward.x * state.velocity.x + forward.y * state.velocity.y +
                                      forward.z * state.velocity.z) /
                                         length,
                                     -1.f, 1.f)) *
                57.288352966308594f;
        if (advance_wheel_spin(replay_wheel_spins_[i], state.cached_speed, angle,
                               car.parameters().rear_wheel_radius * 6.2831854820251465f,
                               state.brake_amount, state.air_time, dt) != Result::ok ||
            track.record(dt, {state.position, state.velocity, state.heading_degrees,
                              state.pitch_degrees, state.roll_degrees, state.steering_degrees,
                              replay_wheel_spins_[i][2], health.nitrous_timer != 0,
                              racers_[i].progress.current_node}) != Result::ok) {
            error = "Original replay pose recording failed";
            return Result::invalid_argument;
        }
    }
    return Result::ok;
}
ReplayTarget RaceSession::replay_target(std::size_t i) {
    return {
        [this, i](const ReplayPose& pose) {
            auto& car = *racers_[i].vehicle;
            auto& state = car.collision_state();
            state.position = pose.position;
            state.velocity = pose.velocity;
            state.heading_degrees = pose.heading;
            state.pitch_degrees = pose.pitch;
            state.roll_degrees = pose.roll;
            state.steering_degrees = pose.steering;
            state.cached_speed =
                std::sqrt(pose.velocity.x * pose.velocity.x + pose.velocity.y * pose.velocity.y +
                          pose.velocity.z * pose.velocity.z);
            car.refresh_after_pair_collision();
            racers_[i].progress.current_node = pose.route_node;
            replay_wheel_spins_[i].fill(pose.wheel_spin);
            replay_nitrous_flags_[i] = pose.nitrous;
            return Result::ok;
        },
        [this, i](const ReplayEvent& event, ReplayEventMode mode) -> Result {
            auto& car = *racers_[i].vehicle;
            const bool playback = mode == ReplayEventMode::playback;
            const bool backward = mode == ReplayEventMode::seek_backward;
            return std::visit(
                [&](const auto& data) -> Result {
                    using T = std::decay_t<decltype(data)>;
                    if constexpr (std::is_same_v<T, ReplayWeaponEvent>) {
                        const auto count = data.ammunition_before - (backward || playback ? 0 : 1);
                        if (car.combat()->replay_ammunition(data.slot, count) != Result::ok)
                            return Result::invalid_argument;
                        if (playback) {
                            const auto& state = car.state();
                            // Native0049dd5e ignores a busy mount's rejected request.
                            car.combat()->request(data.slot, state.cached_speed,
                                                  state.heading_degrees, state.pitch_degrees,
                                                  car.body_orientation().body_pitch);
                        }
                    } else if constexpr (std::is_same_v<T, ReplayTextureEvent>) {
                        return car.damage_visuals()->replay_texture_mark(data.mark, !backward);
                    } else if constexpr (std::is_same_v<T, ReplayDeformationEvent>) {
                        if (!backward) {
                            std::vector<VehicleDeformation> records(
                                car.damage_visuals()->deformations().begin(),
                                car.damage_visuals()->deformations().end());
                            records.push_back(data.deformation);
                            return car.damage_visuals()->reapply_deformations(records);
                        }
                    } else if constexpr (std::is_same_v<T, ReplayHealthEvent>) {
                        car.equipment().internal_hp = data.internal_hp;
                        car.equipment().armor_hp = data.armor_hp;
                    } else if constexpr (std::is_same_v<T, ReplayNitrousEvent>) {
                        car.equipment().nitrous_ammo =
                            data.ammunition_before - (backward || playback ? 0 : 1);
                        if (playback)
                            activate_nitrous(i);
                    } else if constexpr (std::is_same_v<T, ReplayImpactEvent>) {
                        if (playback) {
                            if (vehicle_particles_[i].sparks.begin_burst(
                                    {data.position, data.force, 1.5f}))
                                callbacks_.insert({RaceCallbackKey::Domain::vehicle_sparks, i, 0});
                            car.append_pair_collision_event(1, data.position);
                        }
                    }
                    return Result::ok;
                },
                event.data);
        },
        [this, i] { return racers_[i].vehicle->damage_visuals()->reapply_deformations({}); }};
}

// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00483496
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0048ec42
Result RaceSession::begin_replay(std::string& error) {
    if (!replay_available()) {
        error = "No complete original replay recording available";
        return Result::unavailable;
    }
    replay_prepared_ = false;
    auto saved = std::make_shared<ReplaySavedState>();
    saved->callbacks = callbacks_.snapshot();
    saved->ranking = std::move(ranking_);
    saved->finished = std::move(finished_);
    saved->random = random_state_;
    saved->ticks = ticks_;
    saved->scanner = scanner_;
    saved->race_time = race_time_;
    saved->countdown = countdown_;
    saved->table = time_table_remaining_;
    saved->idle = idle_return_timer_;
    saved->camera_mode = native_camera_mode_;
    saved->look_back = look_back_;
    saved->target = spectator_target_;
    saved->hud_item = selected_hud_item_;
    saved->paused = paused_;
    saved->autopilot = player_autopilot_;
    saved->idle_requested = idle_return_requested_;
    saved->warning_ready = warning_countdown_ready_;
    saved->phase = phase_;
    saved->ending = ending_;
    saved->notices = ammunition_notices_;
    saved->weapon_input = weapon_input_;
    saved->start = start_;
    saved->chase = spectator_chase_;
    saved->steering_chase = spectator_steering_chase_;
    saved->chase_frame = spectator_chase_frame_;
    saved->scene = std::move(scene_combat_);
    saved->traps = std::move(scene_electron_traps_);
    saved->electron_trails = std::move(scene_electron_trails_);
    saved->scene_trails = std::move(scene_trails_);
    saved->vehicle_trails = std::move(vehicle_trails_);
    saved->nitrous_trails = std::move(nitrous_trails_);
    saved->impacts = std::move(impact_pools_);
    saved->rear_blasts = std::move(rear_blasts_);
    saved->particles = std::move(vehicle_particles_);
    saved->weather = std::move(weather_);
    saved->chimneys = std::move(chimneys_);
    saved->fire_audio = fire_audio_;
    saved->nitrous_audio = nitrous_audio_;
    saved->audio_events = std::move(audio_events_);
    saved->audio_commands = std::move(audio_loop_commands_);
    saved->audio_alive = audio_voice_alive_;
    saved->dispatch = std::move(dispatch_trace_);
    saved->collision_lines = std::move(collision_lines_);
    saved->racers = std::move(racers_);
    saved->player = std::move(*saved->racers.back().vehicle);
    replay_saved_ = saved;
    callbacks_.reset();
    racers_.reserve(saved->racers.size());
    for (std::size_t i = 0; i < saved->racers.size(); ++i) {
        const auto& old = saved->racers[i];
        const auto& old_car = i + 1 == saved->racers.size() ? saved->player : *old.vehicle;
        RaceParticipant racer;
        racer.character = old.character;
        racer.loadout = old.loadout;
        racer.ai = old.ai;
        racer.chase_camera = old.chase_camera;
        racer.weapon_mounts = old.weapon_mounts;
        racer.turret_exempt = old.turret_exempt;
        racer.passive_jammer = old.passive_jammer;
        racer.rear_mount_local = old.rear_mount_local;
        racer.hood_point_local = old.hood_point_local;
        if (i + 1 == saved->racers.size())
            racer.vehicle = old.vehicle;
        else {
            racer.owned_vehicle = std::make_unique<VehicleDynamics>();
            racer.vehicle = racer.owned_vehicle.get();
        }
        if (racer.vehicle->initialize(old_car.parameters(), *collision_, error) != Result::ok) {
            end_replay(error);
            return Result::unavailable;
        }
        racers_.push_back(std::move(racer));
    }
    chimneys_ = saved->chimneys;
    rear_blasts_.resize(racers_.size());
    vehicle_particles_.resize(racers_.size());
    if (scene_combat_.load(*resources_, options_.track, scene_world_, error) != Result::ok) {
        end_replay(error);
        return Result::unavailable;
    }
    replay_initializing_ = true;
    const auto initialized = restart(error);
    replay_initializing_ = false;
    if (initialized != Result::ok) {
        end_replay(error);
        return initialized;
    }
    //0049d870 reuses the current vehicle before0049da23 reverses events. Seed
    // that exact baseline, including mutations older than the retained pose ring.
    for (std::size_t i = 0; i < racers_.size(); ++i) {
        const auto& old = saved->racers[i];
        const auto& old_car = i + 1 == racers_.size() ? saved->player : *old.vehicle;
        auto& car = *racers_[i].vehicle;
        car.seed_replay_state(old_car);
        car.equipment() = old_car.equipment();
        //0049d838 unregisters only the recorder; vehicle weapon callbacks persist.
        car.set_combat(std::make_unique<VehicleCombat>(*old_car.combat()));
        if (car.damage_visuals()->restore_state(old_car.damage_visuals()->capture_state()) !=
            Result::ok) {
            end_replay(error);
            error = "Native replay visual baseline unavailable";
            return Result::unavailable;
        }
        for (std::size_t slot = 0; slot < 3; ++slot) {
            const auto weapon = old_car.combat()->slot(static_cast<WeaponSlot>(slot));
            if (weapon.descriptor &&
                car.combat()->replay_ammunition(static_cast<WeaponSlot>(slot), weapon.ammunition) !=
                    Result::ok) {
                end_replay(error);
                error = "Native replay weapon baseline unavailable";
                return Result::unavailable;
            }
        }
        racers_[i].progress = old.progress;
        racers_[i].lap_times = old.lap_times;
        racers_[i].finish_time = old.finish_time;
        racers_[i].rank = old.rank;
    }
    race_time_ = saved->race_time;
    ticks_ = saved->ticks;
    //004ba8a3 resets scene turrets/weather, not vehicle weapons or global effects.
    vehicle_trails_ = saved->vehicle_trails;
    nitrous_trails_ = saved->nitrous_trails;
    scene_trails_ = saved->scene_trails;
    impact_pools_ = saved->impacts;
    rear_blasts_ = saved->rear_blasts;
    vehicle_particles_ = saved->particles;
    for (auto& particles : vehicle_particles_)
        for (auto& tire : particles.tires)
            tire.reset(); //0049d870 calls004e8440 four times.
    scene_electron_traps_ = saved->traps;
    scene_electron_trails_ = saved->electron_trails;
    chimneys_ = saved->chimneys;
    std::vector<RaceCallbackKey> retained;
    for (const auto key : saved->callbacks)
        if (key.domain != RaceCallbackKey::Domain::scene &&
            key.domain != RaceCallbackKey::Domain::weather)
            retained.push_back(key);
    callbacks_.restore(retained);
    // Initialization-only AI resets are absent from native replay entry.
    random_state_ = saved->random;
    // Native playback uses its recorder's clock and does not run race controllers/countdown.
    countdown_ = 0;
    phase_ = RacePhase::racing;
    ending_ = RaceEndReason::none;
    native_camera_mode_ = 5;
    spectator_target_ = player_index();
    paused_ = false;
    replay_paused_ = false;
    replay_selection_ = 2;
    for (std::size_t i = 0; i < replay_tracks_.size(); ++i) {
        if (replay_tracks_[i].begin(replay_target(i)) != Result::ok) {
            error = "Original replay target/event initialization failed";
            end_replay(error);
            return Result::unavailable;
        }
        callbacks_.insert({RaceCallbackKey::Domain::replay, i, 0});
    }
    replay_prepared_ = true;
    //0048ec42 primes the actual callback manager and camera with one second.
    if (advance_combat(1, error, true) != Result::ok) {
        const auto reason = error;
        replay_prepared_ = false;
        end_replay(error);
        error = reason;
        return Result::unavailable;
    }
    error.clear();
    return Result::ok;
}

Result RaceSession::end_replay(std::string& error) {
    if (!replay_saved_)
        return Result::unavailable;
    const bool commit = replay_prepared_;
    if (commit)
        for (std::size_t i = 0; i < replay_tracks_.size(); ++i) {
            if (replay_tracks_[i].seek(replay_tracks_[i].last_time(), replay_target(i)) !=
                Result::ok) {
                error = "Native replay exit seek failed";
                return Result::unavailable;
            }
            race_time_ = replay_tracks_[i].time();
        }
    const auto final_time = race_time_;
    std::vector<RaceCallbackKey> surviving;
    if (commit)
        for (const auto key : callbacks_.snapshot())
            if (key.domain != RaceCallbackKey::Domain::replay)
                surviving.push_back(key);
    auto saved = std::move(replay_saved_);
    const auto replay_random = random_state_;
    if (commit)
        for (std::size_t i = 0; i < racers_.size(); ++i) {
            auto& old = saved->racers[i];
            auto& old_car = i + 1 == racers_.size() ? saved->player : *old.vehicle;
            old_car.commit_replay_state(*racers_[i].vehicle);
            old.progress = racers_[i].progress;
            old.side_target = racers_[i].side_target;
            old.active_side_target = racers_[i].active_side_target;
        }
    callbacks_.reset();
    *saved->racers.back().vehicle = std::move(saved->player);
    racers_ = std::move(saved->racers);
    ranking_ = std::move(saved->ranking);
    finished_ = std::move(saved->finished);
    // Native exit has no CRT RNG snapshot restoration: playback consumes it.
    random_state_ = commit ? replay_random : saved->random;
    ticks_ = saved->ticks;
    scanner_ = saved->scanner;
    race_time_ = commit ? final_time : saved->race_time;
    countdown_ = saved->countdown;
    time_table_remaining_ = saved->table;
    idle_return_timer_ = saved->idle;
    native_camera_mode_ = commit ? 4 : saved->camera_mode;
    if (!commit)
        look_back_ = saved->look_back;
    spectator_target_ = commit ? player_index() : saved->target;
    selected_hud_item_ = saved->hud_item;
    paused_ = saved->paused;
    player_autopilot_ = saved->autopilot;
    idle_return_requested_ = saved->idle_requested;
    warning_countdown_ready_ = saved->warning_ready;
    phase_ = saved->phase;
    ending_ = saved->ending;
    ammunition_notices_ = saved->notices;
    weapon_input_ = saved->weapon_input;
    start_ = std::move(saved->start);
    spectator_chase_ = std::move(saved->chase);
    spectator_steering_chase_ = std::move(saved->steering_chase);
    spectator_chase_frame_ = saved->chase_frame;
    if (!commit) {
        scene_combat_ = std::move(saved->scene);
        scene_electron_traps_ = std::move(saved->traps);
        scene_electron_trails_ = std::move(saved->electron_trails);
        scene_trails_ = std::move(saved->scene_trails);
        vehicle_trails_ = std::move(saved->vehicle_trails);
        nitrous_trails_ = std::move(saved->nitrous_trails);
        impact_pools_ = std::move(saved->impacts);
        rear_blasts_ = std::move(saved->rear_blasts);
        vehicle_particles_ = std::move(saved->particles);
        weather_ = std::move(saved->weather);
        chimneys_ = std::move(saved->chimneys);
        fire_audio_ = saved->fire_audio;
        nitrous_audio_ = saved->nitrous_audio;
        audio_events_ = std::move(saved->audio_events);
        audio_loop_commands_ = std::move(saved->audio_commands);
        audio_voice_alive_ = std::move(saved->audio_alive);
        dispatch_trace_ = std::move(saved->dispatch);
        collision_lines_ = std::move(saved->collision_lines);
    }
    callbacks_.restore(commit ? std::span<const RaceCallbackKey>(surviving)
                              : std::span<const RaceCallbackKey>(saved->callbacks));
    replay_paused_ = false;
    replay_prepared_ = false;
    for (auto& track : replay_tracks_)
        track.end();
    return reset_replay_recording(error, commit);
}
Result RaceSession::seek_replay(float time, std::string& error) {
    if (!replaying() || !std::isfinite(time))
        return Result::unavailable;
    for (std::size_t i = 0; i < replay_tracks_.size(); ++i) {
        if (replay_tracks_[i].seek(time, replay_target(i)) != Result::ok) {
            error = "Original replay event seek failed";
            return Result::unavailable;
        }
        race_time_ = replay_tracks_[i].time();
        replay_clock_owner_ = i;
    }
    error.clear();
    return Result::ok;
}
Result RaceSession::advance_replay(float dt, std::string& error) {
    audio_events_.clear();
    audio_loop_commands_.clear();
    for (auto& racer : racers_)
        if (racer.vehicle->advance_replay_collision_audio(replay_paused_ ? 0 : dt) != Result::ok)
            return Result::invalid_argument;
    if (replay_paused_)
        return Result::ok;
    // Native004803e6 invokes00413200(1), recalculating RPM from recorded velocity.
    for (auto& racer : racers_) {
        auto& car = *racer.vehicle;
        auto& state = car.collision_state();
        state.rpm = vehicle_rpm(car.parameters(), state, state.gear_index, state.engine_throttle);
        update_automatic_transmission(car.parameters(), state);
    }
    if (advance_audio(dt, error) != Result::ok || advance_combat(dt, error) != Result::ok)
        return Result::unavailable;
    error.clear();
    return Result::ok;
}
Result RaceSession::execute_replay_action(std::string& error) {
    if (!replaying() || replay_tracks_.empty())
        return Result::unavailable;
    switch (replay_selection_) {
    case 0:
    case 4:
        for (std::size_t i = 0; i < replay_tracks_.size(); ++i) {
            const auto target = replay_selection_ == 0 ? replay_tracks_[i].first_time()
                                                       : replay_tracks_[i].last_time();
            if (replay_tracks_[i].seek(target, replay_target(i)) != Result::ok) {
                error = "Native replay endpoint seek failed";
                return Result::unavailable;
            }
            race_time_ = replay_tracks_[i].time();
            replay_clock_owner_ = i;
        }
        return Result::ok;
    case 1:
    case 3:
        for (std::size_t i = 0; i < replay_tracks_.size(); ++i) {
            if (replay_tracks_[i].step(replay_selection_ == 3, replay_target(i)) != Result::ok) {
                error = "Native replay sample step failed";
                return Result::unavailable;
            }
            race_time_ = replay_tracks_[i].time();
            replay_clock_owner_ = i;
        }
        return Result::ok;
    case 2:
        replay_paused_ = !replay_paused_;
        return Result::ok;
    case 5: {
        const auto result = end_replay(error);
        if (result == Result::ok) {
            native_camera_mode_ = 4;
            spectator_target_ = player_index();
        }
        return result;
    }
    case 6: {
        native_camera_mode_ = native_camera_mode_ == 4   ? 1
                              : native_camera_mode_ == 1 ? 2
                              : native_camera_mode_ == 2 ? 3
                              : native_camera_mode_ == 3 ? 5
                                                         : 4;
        spectator_target_ = player_index();
        return Result::ok;
    }
    }
    return Result::invalid_argument;
}
} // namespace roadwars
