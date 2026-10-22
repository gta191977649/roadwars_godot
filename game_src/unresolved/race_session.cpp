#include "game_src/unresolved/race_session.hpp"
#include "game_src/unresolved/contact_orientation.hpp"
#include "game_src/unresolved/race_collision.hpp"
#include "game_src/unresolved/track_render_settings.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace roadwars {
namespace {
Vec3 add(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 sub(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
VehicleCombatFrame combat_pose(const VehicleDynamics& vehicle, std::int32_t identity) {
    VehicleCombatFrame frame;
    const auto& state = vehicle.state();
    frame.owner_identity = identity;
    frame.vehicle_pose = vehicle.transform();
    frame.forward = frame.vehicle_pose.basis[2];
    frame.heading = state.heading_degrees;
    frame.pitch = state.pitch_degrees;
    frame.roll = state.roll_degrees;
    frame.cached_speed = state.cached_speed;
    frame.body_pitch = vehicle.body_orientation().body_pitch;
    frame.body_roll = vehicle.body_orientation().body_roll;
    return frame;
}
bool start(const MeshObject& node, Vec3 parent, std::string_view name, TrackSpawn& output) {
    const auto position = add(parent, node.position);
    if (node.name == name) {
        output = {position, node.dummy_heading_degrees};
        return true;
    }
    for (const auto& child : node.children)
        if (start(child, position, name, output))
            return true;
    return false;
}
std::array<Vec3, 4> footprint(const VehicleDynamics& v) {
    const auto& p = v.parameters();
    const auto& s = v.state();
    CollisionBody body;
    body.position = s.position;
    body.heading_degrees = s.heading_degrees;
    body.front_extension = p.front_extension;
    const float w = p.width * .5f, l = p.length * .5f;
    body.diagonal_radius = std::sqrt(w * w + l * l);
    body.corner_angle_degrees =
        static_cast<float>(std::atan(static_cast<double>(w / l)) * 57.288352966308594);
    return collision_footprint(body);
}
} // namespace
RaceSession::~RaceSession() {
    for (auto& racer : racers_)
        if (racer.vehicle) {
            racer.vehicle->set_respawn_handler({});
            racer.vehicle->set_particle_impact_handler({});
            racer.vehicle->set_damage_visual_handler({});
            racer.vehicle->set_collision_damage_handler({});
            racer.vehicle->set_elimination_handler({});
            if (racer.vehicle->combat())
                racer.vehicle->combat()->observe_callbacks({});
        }
}
std::uint32_t RaceSession::random() {
    // 005170b0: original CRT sequence; seed selection belongs to the diagnostic host.
    return original_random(random_state_);
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004a527a
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00484189
Result RaceSession::load(ResourceStore& resources, const TrackCollision& collision,
                         const RaceOptions& options, VehicleDynamics& player, std::string& error) {
    if (options.laps < 1 || options.laps > 10 || options.ai_count < 0 || options.ai_count > 5 ||
        (options.diagnostic_weapon_lock_seconds != 0 &&
         options.diagnostic_weapon_lock_seconds != 20)) {
        error = "Race requires 1..10 laps and 0..5 AI";
        return Result::invalid_argument;
    }
    for (auto& racer : racers_)
        if (racer.vehicle) {
            racer.vehicle->set_respawn_handler({});
            racer.vehicle->set_particle_impact_handler({});
            racer.vehicle->set_damage_visual_handler({});
            racer.vehicle->set_collision_damage_handler({});
            racer.vehicle->set_elimination_handler({});
            if (racer.vehicle->combat())
                racer.vehicle->combat()->observe_callbacks({});
        }
    racers_.clear();
    initial_equipment_.clear();
    rear_blasts_.clear();
    vehicle_particles_.clear();
    spawns_.clear();
    options_ = options;
    resources_ = &resources;
    weapon_lock_seconds_ = options.diagnostic_weapon_lock_seconds;
    collision_ = &collision;
    random_state_ = options.seed;
    if (route_.load(resources, options.track, error) != Result::ok)
        return Result::unavailable;
    Configuration config;
    float difficulty{};
    if (config.load(resources, "Ini/RoadWars.ini") != Result::ok ||
        config.number("ROADWARS", "SPECIAL_EFFECTS_SCALAR", special_effects_scalar_) !=
            Result::ok ||
        config.number("ROADWARS", "DIFFICULTY_LEVEL", difficulty) != Result::ok ||
        !std::isfinite(special_effects_scalar_) || special_effects_scalar_ < 0 || difficulty < 0 ||
        difficulty > 3 || std::floor(difficulty) != difficulty) {
        error = "Original race combat global configuration unavailable";
        return Result::unavailable;
    }
    difficulty_ = static_cast<int>(difficulty);
    if (load_combat_audio(resources, audio_resources_, error) != Result::ok ||
        fire_audio_.initialize(CombatSample::fire) != Result::ok ||
        nitrous_audio_.initialize(CombatSample::nitrous) != Result::ok)
        return Result::unavailable;
    if (config.load(resources, "Game Options/Single Race/RaceInit.ini") != Result::ok ||
        config.number("DATA", "SECONDSTILLGO", initial_countdown_) != Result::ok ||
        !std::isfinite(initial_countdown_) || initial_countdown_ < 1) {
        error = "Original race countdown is missing or malformed";
        return Result::unavailable;
    }
    if (config.load(resources, "Screens/SelectCharacter/Data.ini") != Result::ok) {
        error = "Original character selection table is unavailable";
        return Result::unavailable;
    }
    float count{};
    if (config.number("INFO", "NUMSELECTIONS", count) != Result::ok || count < 1 || count > 256 ||
        std::floor(count) != count)
        return Result::invalid_argument;
    std::vector<std::string> available;
    for (int i = 0; i < static_cast<int>(count); ++i) {
        const auto section = "SELECTION" + std::to_string(i);
        float enabled{};
        if (config.number(section, "ISAVAILABLE", enabled) != Result::ok)
            return Result::invalid_argument;
        auto name = config.value(section, "VALUE");
        if (enabled != 0 && !name.empty() &&
            ResourceStore::canonical(name) != ResourceStore::canonical(options.car))
            available.push_back(std::move(name));
    }
    if (available.size() < static_cast<std::size_t>(options.ai_count)) {
        error = "Not enough available original opponents";
        return Result::unavailable;
    }
    std::vector<std::byte> bytes;
    MeshObject world;
    if (resources.read("Tracks/" + options.track + "/World/SpatialObject.msh", bytes) !=
            Result::ok ||
        decode_msh(bytes, world, error) != Result::ok)
        return Result::unavailable;
    for (int i = 0; i <= options.ai_count; ++i) {
        TrackSpawn pose;
        if (!start(world, {}, "Start" + std::to_string(i + 1), pose)) {
            error = "Original race grid start is missing";
            return Result::not_found;
        }
        // 0048355e calls 004bb389 for every grid slot: project the dummy onto
        // the original surface and subtract 0.5, rather than using its raw Y.
        const auto ground = collision.sample(pose.position);
        if (!ground.hit) {
            error = "Original race grid start has no surface: Start" + std::to_string(i + 1);
            return Result::invalid_argument;
        }
        pose.position.y = ground.point.y - .5f;
        spawns_.push_back(pose);
        RaceParticipant racer;
        if (i < options.ai_count) {
            const auto pick = static_cast<std::size_t>(random() % available.size());
            racer.character = available[pick];
            available.erase(available.begin() + pick);
            racer.owned_vehicle = std::make_unique<VehicleDynamics>();
            racer.vehicle = racer.owned_vehicle.get();
            VehicleParameters parameters;
            if (load_vehicle_metrics(resources, racer.character, parameters, error) != Result::ok ||
                load_vehicle_geometry(resources, racer.character, parameters, error) !=
                    Result::ok ||
                racer.vehicle->initialize(std::move(parameters), collision, error) != Result::ok)
                return Result::unavailable;
        } else {
            racer.character = options.car;
            racer.vehicle = &player;
        }
        if (racer.chase_camera.load(resources, racer.character, error) != Result::ok)
            return Result::unavailable;
        if (load_turret_target_exemption(resources, racer.character, racer.turret_exempt, error) !=
            Result::ok)
            return Result::unavailable;
        float jam{};
        if (config.load(resources, "Characters/" + racer.character + "/Data.ini") != Result::ok ||
            config.number("PRICE_SCALARS", "JAM", jam) != Result::ok || std::floor(jam) != jam) {
            error = "Original character JAM flag unavailable";
            return Result::unavailable;
        }
        racer.passive_jammer = jam != 0;
        racers_.push_back(std::move(racer));
    }
    vehicle_particles_.resize(racers_.size());
    ParticleRandom particle_setup([this] { return random(); });
    for (auto& effects : vehicle_particles_) {
        if (effects.sparks.configure(vehicle_spark_settings(special_effects_scalar_)) != Result::ok)
            return Result::unavailable;
        for (auto& tire : effects.tires)
            if (tire.configure(special_effects_scalar_, particle_setup) != Result::ok)
                return Result::unavailable;
    }
    RaceLoadoutTable loadouts;
    if (loadouts.load(resources, options.track, error) != Result::ok)
        return Result::unavailable;
    // Original setup completes all character selections before assigning equipment.
    rear_blasts_.resize(racers_.size());
    std::size_t equipment_index{};
    ParticleRandom setup_random([this] { return random(); });
    for (auto& racer : racers_) {
        RaceLoadoutInstallers installers;
        installers.weapon = [&](std::size_t slot, std::string_view name) {
            const auto* descriptor = find_weapon_descriptor(name);
            if (!descriptor)
                return Result::unavailable;
            if (slot == 2 &&
                (descriptor->name == "Grenades" || descriptor->name == "Land Mines" ||
                 descriptor->name == "Reactive Mines" || descriptor->name == "Arachnoids"))
                for (auto& pool : rear_blasts_[equipment_index])
                    if (pool.configure(descriptor->name, setup_random) != Result::ok)
                        return Result::unavailable;
            return Result::ok;
        };
        if (loadouts.assign(random_state_, racer.loadout, installers, error) != Result::ok)
            return Result::unavailable;
        std::vector<std::pair<std::string, std::string>> drivetrain;
        for (const auto& item : racer.loadout.drivetrain)
            drivetrain.emplace_back(item.type, item.quality);
        VehicleEquipmentEffects equipment;
        if (load_vehicle_equipment_effects(resources, racer.character, racer.character,
                                           racer.loadout.armor.type, racer.loadout.armor.quality,
                                           drivetrain, racer.loadout.weapons[0], equipment,
                                           error) != Result::ok)
            return Result::unavailable;
        auto parameters = racer.vehicle->parameters();
        parameters.tires = racer.loadout.tires;
        parameters.engine_upgrades = equipment.engine_upgrades;
        if (load_vehicle_geometry(resources, racer.character, parameters, error) != Result::ok ||
            racer.vehicle->initialize(std::move(parameters), collision, error) != Result::ok)
            return Result::unavailable;
        racer.vehicle->set_equipment(std::move(equipment));
        initial_equipment_.push_back(racer.vehicle->equipment());
        MeshObject body;
        if (resources.read("Cars/" + racer.character + "/Body/SpatialObject.msh", bytes) !=
                Result::ok ||
            decode_msh(bytes, body, error) != Result::ok)
            return Result::unavailable;
        racer.weapon_mounts = race_weapon_mount_count(body);
        TrackSpawn rear;
        if (!start(body, {}, "RearWeaponMount", rear)) {
            error = "Original nitrous RearWeaponMount unavailable";
            return Result::unavailable;
        }
        racer.rear_mount_local = rear.position;
        TrackSpawn hood;
        if (!start(body, {}, "HoodPoint", hood)) {
            error = "Original damage HoodPoint unavailable";
            return Result::unavailable;
        }
        racer.hood_point_local = hood.position;
        ++equipment_index;
    }
    for (std::size_t i = 0; i < racers_.size(); ++i)
        racers_[i].vehicle->set_respawn_handler([this, i] { return rescue(i); });
    if (start_.load(resources, world, options.car, spawns_, collision, error) != Result::ok)
        return Result::unavailable;
    if (scene_combat_.load(resources, options.track, world, error) != Result::ok)
        return Result::unavailable;
    Configuration world_config;
    float stacks{};
    ViewDistanceMap distances;
    if (world_config.load(resources, "Tracks/" + options.track + "/World/Data.txt") != Result::ok ||
        world_config.number("DATA", "STACKS", stacks) != Result::ok ||
        distances.load(resources, options.track, error) != Result::ok)
        return Result::unavailable;
    chimney_view_distance_ = distances.maximum_distance();
    chimneys_enabled_ = stacks != 0;
    chimneys_.clear();
    const std::function<Result(const MeshObject&, Vec3, bool)> visit_stack =
        [&](const MeshObject& mesh, Vec3 parent, bool transformed) {
            const auto point = add(parent, mesh.position);
            if (mesh.name.starts_with("Stack")) {
                if (transformed) {
                    error = "Original Stack transformed ancestry unavailable";
                    return Result::unavailable;
                }
                chimneys_.push_back({point, {}});
            }
            for (const auto& child : mesh.children)
                if (visit_stack(child, point,
                                transformed || mesh.type == 4 ||
                                    mesh.name.starts_with("RotObject")) != Result::ok)
                    return Result::unavailable;
            return Result::ok;
        };
    if (visit_stack(world, {}, false) != Result::ok)
        return Result::unavailable;
    scene_world_ = std::move(world);
    return restart(error);
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0048355e
Result RaceSession::restart(std::string& error) {
    if (racers_.empty())
        return Result::unavailable;
    race_time_ = 0;
    countdown_ = initial_countdown_;
    warning_countdown_ready_ = false;
    ticks_ = 0;
    scanner_ = 0;
    paused_ = false;
    phase_ = RacePhase::countdown;
    ending_ = RaceEndReason::none;
    idle_return_timer_ = 10;
    idle_return_requested_ = false;
    finished_.clear();
    collision_lines_.clear();
    callbacks_.reset();
    audio_events_.clear();
    audio_loop_commands_.clear();
    dispatch_trace_.clear();
    audio_voice_alive_.assign(2 + racers_.size(), false);
    fire_audio_.reset();
    nitrous_audio_.reset();
    weapon_input_ = {};
    selected_hud_item_ = 0; // 0040d4cb initializes4ab0=1;00487fbe therefore selects map.
    for (auto& chimney : chimneys_) {
        if (chimney.pool.configure(chimney_view_distance_) != Result::ok)
            return Result::unavailable;
        chimney.pool.start();
    }
    scene_combat_.reset();
    scene_combat_.observe_callbacks([this](auto token, bool insert) {
        const RaceCallbackKey key{RaceCallbackKey::Domain::scene, 0,
                                  static_cast<std::size_t>(token)};
        if (insert)
            callbacks_.insert(key);
        else
            callbacks_.remove(key);
    });
    if (!resources_ || scene_electron_traps_.load(*resources_, scene_world_, error) != Result::ok)
        return Result::unavailable;
    scene_electron_traps_.observe_callbacks([this](auto token, bool insert) {
        const RaceCallbackKey key{RaceCallbackKey::Domain::scene_electron, 0,
                                  static_cast<std::size_t>(token)};
        if (insert)
            callbacks_.insert(key);
        else
            callbacks_.remove(key);
    });
    scene_electron_trails_.clear();
    scene_electron_trails_.resize(scene_electron_traps_.poses().size());
    for (auto& trail : scene_electron_trails_)
        if (trail.configure("Electron Cannon", special_effects_scalar_) != Result::ok)
            return Result::unavailable;
    scene_trails_.clear();
    vehicle_trails_.clear();
    impact_pools_.clear();
    for (auto& pools : rear_blasts_)
        for (auto& pool : pools)
            pool.reset();
    nitrous_trails_.clear();
    nitrous_trails_.resize(racers_.size());
    for (std::size_t i = 0; i < racers_.size(); ++i) {
        auto& racer = racers_[i];
        if (racer.vehicle->initialize(racer.vehicle->parameters(), *collision_, error) !=
            Result::ok)
            return Result::unavailable;
        racer.vehicle->reset_to(spawns_[i]);
        // Diagnostic restart restores the loaded loadout; it is not the full legacy reset ABI.
        racer.vehicle->set_equipment(initial_equipment_[i]);
        auto combat = std::make_unique<VehicleCombat>();
        if (!resources_ ||
            combat->load(*resources_, racer.character, racer.loadout.weapons, error) != Result::ok)
            return Result::unavailable;
        racer.vehicle->set_combat(std::move(combat));
        auto visuals = std::make_unique<VehicleDamageVisuals>();
        if (visuals->load(*resources_, racer.character, racer.loadout.paint_job, error) !=
            Result::ok)
            return Result::unavailable;
        racer.vehicle->set_damage_visuals(std::move(visuals));
        racer.vehicle->set_damage_visual_handler(
            [this, i](const VehicleDamageVisualContact& contact) {
                ParticleRandom shared([this] { return random(); });
                return racers_[i].vehicle->damage_visuals()->collision(contact, shared);
            });
        racer.vehicle->combat()->observe_callbacks([this, i](auto token, bool insert) {
            const RaceCallbackKey key{RaceCallbackKey::Domain::vehicle, i,
                                      static_cast<std::size_t>(token)};
            if (insert)
                callbacks_.insert(key);
            else
                callbacks_.remove(key);
        });
        racer.vehicle->set_collision_damage_handler([this, i](Vec3 force, Vec3 point) {
            return apply_vehicle_collision_damage(*racers_[i].vehicle, force, point,
                                                  racers_[i].progress.completed_laps >=
                                                      options_.laps,
                                                  i == player_index(), difficulty_);
        });
        racer.vehicle->set_elimination_handler(
            i == player_index() ? std::function<Result()>{[this] {
                std::optional<CombatAudioEvent> event;
                const auto result =
                    combat_audio_event(CombatAudioCue::selected_player_eliminated, "", {}, event);
                if (result == Result::ok && event)
                    audio_events_.push_back(*event);
                return result;
            }}
                                : std::function<Result()>{});
        vehicle_particles_[i].sparks.reset();
        vehicle_particles_[i].damage.reset();
        for (auto& tire : vehicle_particles_[i].tires)
            tire.reset();
        racer.vehicle->set_particle_impact_handler([this, i](const ParticleImpact& impact) {
            if (vehicle_particles_[i].sparks.begin_burst(impact))
                callbacks_.insert({RaceCallbackKey::Domain::vehicle_sparks, i, 0});
        });
        if (nitrous_trails_[i].configure("Nitrous Boost", special_effects_scalar_) != Result::ok) {
            error = "Original nitrous particle pool unavailable";
            return Result::unavailable;
        }
        racer.combat_ai = {};
        racer.combat_trace = {};
        racer.side_target = racer.active_side_target = -1;
        reset_race_ai_side_delay(racer.combat_ai, random());
        racer.reset_timer_14 = static_cast<float>(random() % 500) / 1000.0f;
        racer.reset_timer_10 = static_cast<float>(random() % 500) / 1000.0f;
        reset_race_ai_rear_timer(racer.combat_ai, random());
        racer.ai.reset();
        racer.chase_camera.reset();
        if (racer.chase_camera.update(racer.vehicle->state(), racer.vehicle->parameters(),
                                      collision_, 0, racer.camera) != Result::ok)
            return Result::unavailable;
        racer.lap_times = {};
        racer.finish_time = 0;
        if (route_.initialize_progress(racer.progress, spawns_[i].position, error) != Result::ok)
            return Result::invalid_argument;
    }
    for (std::size_t i = 0; i < racers_.size(); ++i)
        for (std::size_t j = 0; j < i; ++j)
            if (ResourceStore::canonical(racers_[i].vehicle->damage_visuals()->texture_path()) ==
                ResourceStore::canonical(racers_[j].vehicle->damage_visuals()->texture_path())) {
                if (racers_[i].vehicle->damage_visuals()->share_paint_with(
                        *racers_[j].vehicle->damage_visuals()) != Result::ok)
                    return Result::unavailable;
                break;
            }
    start_.reset();
    if (start_.update(countdown_, 0, *racers_[player_index()].vehicle, *collision_) != Result::ok)
        return Result::unavailable;
    if (weather_.load(*resources_, options_.track, error) != Result::ok)
        return Result::unavailable;
    const auto& camera = start_.camera();
    weather_.seed_camera_history(
        {camera.transform.origin, camera.heading_degrees, camera.pitch_degrees, 0});
    weather_.observe_callbacks([this](auto token, bool insert) {
        const RaceCallbackKey key{RaceCallbackKey::Domain::weather, 0,
                                  static_cast<std::size_t>(token)};
        if (insert)
            callbacks_.insert(key);
        else
            callbacks_.remove(key);
    });
    update_ranking();
    error.clear();
    return Result::ok;
}
TrackSpawn RaceSession::rescue(std::size_t index) const {
    const auto& p = racers_[index].progress;
    const auto a = route_.respawn(p.current_node, 0, p.on_branch);
    const auto b = route_.respawn(p.current_node, 1, p.on_branch);
    const auto direction = sub(b, a);
    return {a, static_cast<float>(std::atan2(direction.x, direction.z) * 57.288352966308594)};
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00480b40
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0047fff6
Result RaceSession::advance(float dt, const InputState& input, std::string& error) {
    if (!collision_ || !std::isfinite(dt) || dt < 0)
        return Result::invalid_argument;
    if (paused_ || dt == 0)
        return Result::ok;
    if (input.reset_requested)
        return restart(error);
    if (ending_ != RaceEndReason::none && !idle_return_requested_) {
        if (input.any_key_down)
            idle_return_timer_ = 10;
        idle_return_timer_ -= dt;
        if (idle_return_timer_ < 0) {
            idle_return_timer_ = 0;
            idle_return_requested_ = true;
        }
    }
    audio_events_.clear();
    audio_loop_commands_.clear();
    dispatch_trace_.clear();
    ++ticks_;
    scanner_ = (scanner_ + 1) % racers_.size();
    const bool go = countdown_ < 1;
    if (go) {
        race_time_ += dt;
        phase_ = ending_ == RaceEndReason::none ? RacePhase::racing : RacePhase::finished;
    }
    std::vector<VehicleState> previous;
    std::vector<RaceAiObstacle> obstacles;
    for (auto& racer : racers_) {
        unsigned contacts{};
        for (const auto& owner : racers_)
            if (const auto* oil = owner.vehicle->combat()->oil_slick()) {
                std::array<OilSlickPatchView, 5> patches;
                for (std::size_t index = 0; index < patches.size(); ++index)
                    patches[index] = {oil->patches[index].route_node,
                                      oil->patches[index].triangles};
                unsigned owner_contacts{};
                if (oil_slick_wheel_contacts(patches, racer.progress.current_node,
                                             static_cast<int>(route_.nodes().size()),
                                             racer.vehicle->wheel_points(), owner_contacts,
                                             error) != Result::ok)
                    return Result::unavailable;
                contacts += owner_contacts;
            }
        racer.vehicle->set_oil_contacts(contacts);
    }
    for (const auto& r : racers_) {
        previous.push_back(r.vehicle->state());
        obstacles.push_back({r.vehicle->state().position, r.vehicle->state().velocity,
                             footprint(*r.vehicle), true});
    }
    std::vector<VehicleControls> prepared(racers_.size());
    std::vector<bool> human_controls(racers_.size());
    const auto prepare_controller = [&](std::size_t i) -> Result {
        auto& r = racers_[i];
        const bool ai = i != player_index() || player_autopilot_ || r.progress.finished;
        prepared[i] = {0, 0, r.vehicle->state().steering_degrees};
        if (!go) {
            prepared[i] = {0, 1, 0};
            return Result::ok;
        }
        if (r.vehicle->equipment().dying_timer != 0 ||
            (ai && r.vehicle->equipment().internal_hp <= 0))
            return Result::ok;
        if (ai) {
            std::vector<RaceAiObstacle> others;
            for (std::size_t j = 0; j < obstacles.size(); ++j)
                if (j != i)
                    others.push_back(obstacles[j]);
            if (r.ai.update(route_, r.progress, r.vehicle->state(), r.vehicle->parameters(), others,
                            r.vehicle->longitudinal_grip(), static_cast<float>(race_time_),
                            scanner_ == i, dt, error, r.vehicle->equipment().internal_hp,
                            r.vehicle->equipment().maximum_internal_hp) != Result::ok)
                return Result::unavailable;
            const auto& controls = r.ai.output();
            if (controls.relocate) {
                auto pose = rescue(i);
                pose.position = controls.relocation;
                r.vehicle->reset_to(pose);
            }
            previous[i] = r.vehicle->state();
            prepared[i] = {controls.engine_throttle, controls.brake_amount,
                           controls.steering_degrees};
        } else
            human_controls[i] = true;
        return combat_commands(i, dt, input, error);
    };
    // 00480b40 dispatches the selected controller first. Other controllers run
    // in participant order immediately before each participant's force phase.
    if (prepare_controller(player_index()) != Result::ok)
        return Result::unavailable;
    for (std::size_t i = 0; i < racers_.size(); ++i) {
        if (i != player_index() && prepare_controller(i) != Result::ok)
            return Result::unavailable;
        auto& r = racers_[i];
        dispatch_trace_.push_back({DispatchEvent::Stage::force, i});
        if (human_controls[i])
            r.vehicle->begin_race_update(dt, input);
        else
            r.vehicle->begin_race_update(dt, prepared[i]);
        obstacles[i] = {r.vehicle->state().position, r.vehicle->state().velocity,
                        footprint(*r.vehicle), true};
    }
    collision_lines_.clear();
    struct PendingContact {
        std::size_t a, b;
        float remaining;
    };
    std::vector<PendingContact> pending;
    bool contact_overflow = false;
    Result damage_visual_result = Result::ok;
    const auto walls = [&](std::size_t i, float remaining) {
        auto& r = racers_[i];
        const auto before = r.vehicle->wall_contact_count();
        r.vehicle->solve_race_walls(remaining);
        if (r.vehicle->collision_damage_result() != Result::ok)
            damage_visual_result = r.vehicle->collision_damage_result();
        if (i != player_index() || player_autopilot_)
            r.ai.wall_update(static_cast<unsigned>(r.vehicle->wall_contact_count() - before),
                             r.vehicle->state().engine_throttle, remaining);
    };
    const auto pair = [&](std::size_t a, std::size_t b, float remaining,
                          std::vector<PendingContact>& output) {
        const int gap =
            std::abs(racers_[a].progress.current_node - racers_[b].progress.current_node);
        if (gap > 2 && gap < static_cast<int>(route_.nodes().size()) - 3)
            return;
        float leftover{};
        const VehiclePairDamageContext damage_context{
            racers_[a].progress.completed_laps >= options_.laps,
            racers_[b].progress.completed_laps >= options_.laps,
            a == player_index(),
            b == player_index(),
            difficulty_,
            &damage_visual_result};
        if (resolve_vehicle_collision(*racers_[a].vehicle, *racers_[b].vehicle, previous[a],
                                      previous[b], remaining, &collision_lines_, &leftover,
                                      &damage_context)) {
            if (output.size() >= 32)
                contact_overflow = true;
            else
                output.push_back({a, b, leftover});
        }
    };
    // 0047fff6 interleaves each car's wall pass with its higher-index pairs.
    for (std::size_t a = 0; a < racers_.size(); ++a) {
        walls(a, dt);
        for (std::size_t b = a + 1; b < racers_.size(); ++b)
            pair(a, b, dt, pending);
    }
    for (int iteration = 0; iteration < 10 && !pending.empty(); ++iteration) {
        std::vector<PendingContact> next;
        for (const auto& contact : pending) {
            walls(contact.a, contact.remaining);
            walls(contact.b, contact.remaining);
            for (std::size_t other = 0; other < racers_.size(); ++other) {
                if (contact.a != other)
                    pair(contact.a, other, contact.remaining, next);
                if (contact.b != other && other != contact.a)
                    pair(contact.b, other, contact.remaining, next);
            }
        }
        pending = std::move(next);
    }
    if (damage_visual_result != Result::ok) {
        error = "Original collision damage visuals unavailable";
        return damage_visual_result;
    }
    if (contact_overflow) {
        error = "Original pair collision worklist capacity exceeded";
        return Result::unavailable;
    }
    // Native per-participant damage emitter precedes wheel and health updates.
    for (std::size_t i = 0; i < racers_.size(); ++i) {
        auto& r = racers_[i];
        ParticleRandom shared_particles([this] { return random(); });
        const auto& health_before = r.vehicle->equipment();
        const float damage_fraction =
            1 - health_before.internal_hp / health_before.maximum_internal_hp;
        const auto hood = r.vehicle->combat()->node_world_pose(
            "HoodPoint", combat_pose(*r.vehicle, static_cast<std::int32_t>(i + 1)));
        if (!hood) {
            error = "Original runtime HoodPoint pose unavailable";
            return Result::unavailable;
        }
        if (vehicle_particles_[i].damage.advance(dt, damage_fraction, hood->origin,
                                                 r.vehicle->state().velocity,
                                                 shared_particles) != Result::ok) {
            error = "Original vehicle damage particle phase unavailable";
            return Result::unavailable;
        }
        r.vehicle->finish_race_update(dt);
        if (go && !r.progress.finished) {
            const auto lap = r.progress.completed_laps;
            if (lap < options_.laps)
                r.lap_times[lap] += dt;
            if (route_.update(r.progress, previous[i].position, r.vehicle->state().position,
                              options_.laps, error) != Result::ok)
                return Result::unavailable;
            if (r.progress.finished &&
                std::find(finished_.begin(), finished_.end(), i) == finished_.end()) {
                r.finish_time = race_time_;
                finished_.push_back(i);
            }
        }
        if (advance_vehicle_health(r.vehicle->equipment(), dt) != Result::ok) {
            error = "Original participant health phase failed";
            return Result::invalid_argument;
        }
        if (r.passive_jammer) {
            std::vector<std::vector<Vec3>> positions(racers_.size());
            std::vector<PassiveJammerWeaponView> views;
            for (std::size_t other = 0; other < racers_.size(); ++other) {
                const auto& candidate = racers_[other];
                for (const auto& missile : candidate.vehicle->combat()->guided_missiles())
                    positions[other].push_back(missile.position);
                views.push_back({other + 1,
                                 candidate.side_target >= 0
                                     ? static_cast<std::uint64_t>(candidate.side_target + 1)
                                     : 0,
                                 candidate.vehicle->combat()->slot(WeaponSlot::side).active,
                                 positions[other]});
            }
            for (const auto index :
                 character_missile_jam_indices(i + 1, r.vehicle->state().position, views))
                if (racers_[index].vehicle->combat()->jam(1) != Result::ok)
                    return Result::unavailable;
        }
        for (std::size_t tire = 0; tire < 4; ++tire)
            if (vehicle_particles_[i].tires[tire].advance(
                    dt, r.vehicle->tire_particle_sources()[tire]) != Result::ok)
                return Result::unavailable;
    }
    const auto& selected = racers_[player_index()];
    if (ending_ == RaceEndReason::none) {
        if (selected.progress.finished)
            ending_ = RaceEndReason::completed;
        else if (selected.vehicle->equipment().dying_timer != 0)
            ending_ = RaceEndReason::eliminated;
    }
    if (ending_ != RaceEndReason::none)
        selected_hud_item_ = 0;
    if (advance_audio(dt, error) != Result::ok)
        return Result::unavailable;
    if (advance_combat(dt, error) != Result::ok)
        return Result::unavailable;
    update_ranking();
    if (ending_ != RaceEndReason::none)
        phase_ = RacePhase::finished;
    // 00482339 branches on the pre-subtraction countdown. A step that first
    // clamps the countdown to zero still skips the warning until the next step.
    warning_countdown_ready_ = countdown_ == 0;
    countdown_ = std::max(0.f, countdown_ - dt);
    error.clear();
    return Result::ok;
}
std::vector<RaceAiCombatVehicleView> RaceSession::combat_views() const {
    std::vector<RaceAiCombatVehicleView> output;
    for (std::size_t i = 0; i < racers_.size(); ++i) {
        const auto& racer = racers_[i];
        const auto& v = *racer.vehicle;
        const auto side = v.combat()->slot(WeaponSlot::side);
        const auto& node = route_.point(racer.progress.current_node);
        output.push_back(
            {i + 1,
             v.equipment().dying_timer,
             v.state().position,
             node.lateral,
             racer.progress.current_node,
             {side.descriptor != nullptr,
              side.descriptor && weapon_is_guided(side.descriptor->name), side.active,
              racer.side_target, side.descriptor ? side.descriptor->name : std::string_view{}},
             route_.respawn(racer.progress.current_node, 0, racer.progress.on_branch)});
    }
    return output;
}
Result RaceSession::refresh_side_target(std::size_t owner, std::string& error) {
    auto views = combat_views();
    auto& racer = racers_[owner];
    const auto side = racer.vehicle->combat()->slot(WeaponSlot::side);
    if (!side.descriptor || !weapon_is_guided(side.descriptor->name)) {
        racer.side_target = -1;
        return Result::ok;
    }
    const auto& state = racer.vehicle->state();
    const bool wrong =
        race_missile_owner_wrong_way(racer.vehicle->transform().basis[2], state.position,
                                     route_.point(racer.progress.current_node + 1).center);
    return select_race_missile_target(views, owner, static_cast<int>(route_.nodes().size()),
                                      side.descriptor->name == "Back Missile", wrong,
                                      racer.side_target, error);
}
void RaceSession::activate_nitrous(std::size_t owner) {
    if (activate_vehicle_nitrous(racers_[owner].vehicle->equipment()))
        callbacks_.insert({RaceCallbackKey::Domain::nitrous, owner, 0});
}
Result RaceSession::combat_commands(std::size_t owner, float dt, const InputState& input,
                                    std::string& error) {
    const auto i = owner;
    dispatch_trace_.push_back({DispatchEvent::Stage::controller, i});
    {
        auto& racer = racers_[i];
        const auto allowed = [&] {
            return race_weapon_activation_allowed(countdown_, static_cast<float>(race_time_),
                                                  weapon_lock_seconds_,
                                                  racer.vehicle->equipment().dying_timer);
        };
        const auto activate = [&](std::size_t slot) -> Result {
            if (slot > 2)
                return Result::invalid_argument;
            if (!allowed())
                return Result::ok;
            racer.vehicle->combat()->request(
                static_cast<WeaponSlot>(slot), racer.vehicle->state().cached_speed,
                racer.vehicle->state().heading_degrees, racer.vehicle->state().pitch_degrees,
                racer.vehicle->body_orientation().body_pitch);
            return Result::ok;
        };
        if (i == player_index() && !player_autopilot_ && !racer.progress.finished) {
            std::array<bool, 3> present;
            for (std::size_t slot = 0; slot < 3; ++slot)
                present[slot] =
                    racer.vehicle->combat()->slot(static_cast<WeaponSlot>(slot)).descriptor !=
                    nullptr;
            RaceWeaponInputServices services;
            services.activate = activate;
            services.nitrous = [&] {
                if (allowed())
                    activate_nitrous(i);
                return Result::ok;
            };
            services.cycle = [&](int& selected) {
                if (!racer.progress.finished && racer.vehicle->equipment().dying_timer == 0) {
                    if (selected < 1)
                        selected = 1;
                    else {
                        ++selected;
                        while (
                            selected <= 5 &&
                            ((selected == 2 && initial_equipment_[i].nitrous_ammo == 0) ||
                             (selected >= 3 && !present[static_cast<std::size_t>(selected - 3)])))
                            ++selected;
                        if (selected > 6)
                            selected = 0; // Fresh native vehicle4ab0=1 enables the map branch.
                    }
                    selected_hud_item_ = selected;
                }
                return Result::ok;
            };
            RaceWeaponInput command;
            command.weapon_down = input.weapon_down;
            command.weapon_pressed = input.weapon_requested;
            command.current_down = input.current_weapon_down;
            command.current_pressed = input.current_weapon_requested;
            command.cycle_down = input.cycle_weapon_down;
            command.cycle_pressed = input.cycle_weapon_requested;
            command.nitrous_down = input.nitrous_down;
            command.nitrous_pressed = input.nitrous_requested;
            if (dispatch_race_weapon_input(command, weapon_input_, present, selected_hud_item_,
                                           services, error) != Result::ok)
                return Result::unavailable;
        } else if (allowed() && racer.vehicle->equipment().internal_hp > 0) {
            if (race_time_ >= .25) {
                const auto& flags =
                    route_.point(racer.progress.current_node, racer.progress.on_branch).flags;
                bool good = false, best = false;
                for (const auto& part : racer.loadout.drivetrain)
                    if (ResourceStore::canonical(part.type) == "NITROUS BOOST") {
                        const auto quality = ResourceStore::canonical(part.quality);
                        good = quality == "GOOD";
                        best = quality == "BEST";
                    }
                if (flags[0] != 0 || (flags[1] != 0 && (good || best)) || (flags[2] != 0 && best))
                    activate_nitrous(i);
            }
            // 0041aa67 performs route nitrous before its finished gate; weapon
            // AI is skipped after that gate, while the world continues updating.
            if (racer.progress.finished)
                return Result::ok;
            const auto views = combat_views();
            const auto& wheels = racer.vehicle->wheel_points();
            RaceAiCombatFrame frame{
                views,
                i,
                static_cast<int>(route_.nodes().size()),
                racer.weapon_mounts != 0,
                racer.ai.state().front,
                race_ai_rear_reference(wheels[0], wheels[2], racer.vehicle->state().position)};
            RaceAiCombatServices services;
            services.activate = activate;
            services.random = [this] { return random(); };
            services.weapon = [&](std::size_t slot, RaceAiCombatWeaponView& output) {
                const auto view = racer.vehicle->combat()->slot(static_cast<WeaponSlot>(slot));
                output = {view.descriptor != nullptr,
                          view.descriptor && weapon_is_guided(view.descriptor->name), view.active,
                          slot == 1 ? racer.side_target : -1,
                          view.descriptor ? view.descriptor->name : std::string_view{}};
                return Result::ok;
            };
            services.refresh_side_target = [&](RaceAiCombatWeaponView& output) {
                const auto result = refresh_side_target(i, error);
                if (result != Result::ok)
                    return result;
                return services.weapon(1, output);
            };
            services.side_wall_blocked = [&](bool& blocked) {
                const auto position = racer.vehicle->state().position;
                const auto walls = collision_->nearby_walls(position, position, 3, true);
                if (walls.size() > 1000) {
                    error = "Original AI side wall query capacity exceeded";
                    return Result::unavailable;
                }
                blocked = race_ai_side_wall_blocked(position,
                                                    racer.vehicle->state().heading_degrees, walls);
                return Result::ok;
            };
            if (advance_race_ai_combat(frame, racer.combat_ai, services, dt, racer.combat_trace,
                                       error) != Result::ok)
                return Result::unavailable;
        }
    }
    return Result::ok;
}
Result RaceSession::apply_combat_damage(std::size_t participant, std::size_t side, float hitpoints,
                                        bool internal, bool impulse, bool yaw,
                                        WeaponProjectileImpulse force, std::string& error) {
    if (participant >= racers_.size())
        return Result::invalid_argument;
    auto& racer = racers_[participant];
    auto& vehicle = *racer.vehicle;
    VehicleDamagePermission permission{racer.progress.completed_laps >= options_.laps,
                                       vehicle.equipment().dying_timer != 0, true};
    const auto previous_hp = vehicle.equipment().internal_hp;
    const auto damaged =
        internal ? damage_vehicle_internal(vehicle.equipment(), hitpoints, permission)
                 : damage_vehicle_contact(vehicle.equipment(), side, hitpoints, permission);
    if (damaged != Result::ok) {
        error = "Original weapon damage dispatch failed";
        return damaged;
    }
    if (vehicle.notify_internal_damage(previous_hp) != Result::ok) {
        error = "Original selected elimination audio unavailable";
        return Result::unavailable;
    }
    if (impulse) {
        auto& state = vehicle.collision_state();
        const auto& p = vehicle.parameters();
        apply_vehicle_impulse(state.velocity, force.force, 1, p.inverse_mass, false);
        if (yaw)
            apply_vehicle_yaw_impulse(p, state, force.force, force.application, 1, false);
        apply_vehicle_tilt(p, state, vehicle.wheel_points(), force.force, force.application, 1,
                           false, false);
    }
    return Result::ok;
}
Result RaceSession::advance_combat(float dt, std::string& error) {
    ParticleRandom shared([this] { return random(); });
    const auto impact_event = [&](bool scene, std::size_t owner, WeaponSlot slot,
                                  std::size_t projectile, std::string_view weapon,
                                  const WeaponProjectileHit& hit, Vec3 velocity,
                                  float missile_count) {
        Impact* output{};
        for (auto& impact : impact_pools_)
            if (impact.scene == scene && impact.owner == owner && impact.slot == slot &&
                impact.projectile == projectile) {
                output = &impact;
                break;
            }
        if (!output) {
            Impact impact;
            impact.scene = scene;
            impact.owner = owner;
            impact.slot = slot;
            impact.projectile = projectile;
            const auto result =
                impact.pool.configure(weapon, special_effects_scalar_, missile_count);
            if (result != Result::ok) {
                error = "Original impact pool unavailable: " + std::string(weapon);
                return result;
            }
            impact_pools_.push_back(std::move(impact));
            output = &impact_pools_.back();
        }
        const auto index = static_cast<std::size_t>(output - impact_pools_.data());
        if (output->pool.begin({hit.point, velocity, 1}))
            callbacks_.insert({RaceCallbackKey::Domain::impact, index, 0});
        if (output->pool.requires_secondary_sprite() && output->sprite.begin(hit.point, velocity))
            callbacks_.insert({RaceCallbackKey::Domain::impact_sprite, index, 0});
        return Result::ok;
    };
    std::vector<VehicleCombatTarget> targets;
    std::vector<WeaponProjectileTarget> projectile_targets;
    std::vector<TurretTarget> turret_targets;
    for (std::size_t i = 0; i < racers_.size(); ++i) {
        const auto& racer = racers_[i];
        const auto& v = *racer.vehicle;
        const auto corners = vehicle_pair_footprint(v.parameters(), v.state());
        targets.push_back({i, static_cast<std::int32_t>(i + 1), v.state().position,
                           v.parameters().height, corners, v.state().air_time, v.state().velocity});
        projectile_targets.push_back({i, static_cast<std::int32_t>(i + 1), v.state().position,
                                      v.parameters().height, corners});
        turret_targets.push_back({i, v.state().position, v.state().velocity,
                                  v.equipment().dying_timer,
                                  racer.progress.completed_laps >= options_.laps,
                                  racer.turret_exempt, i != player_index(), difficulty_});
    }
    std::vector<std::vector<Vec3>> missile_positions(racers_.size());
    std::vector<JammerWeaponView> jammer_weapons;
    for (std::size_t i = 0; i < racers_.size(); ++i) {
        for (const auto& missile : racers_[i].vehicle->combat()->guided_missiles())
            missile_positions[i].push_back(missile.position);
        if (!missile_positions[i].empty())
            jammer_weapons.push_back({i + 1, missile_positions[i]});
    }
    std::vector<VehicleCombatFrame> all_frames;
    std::vector<VehicleCombatServices> all_services;
    for (std::size_t i = 0; i < racers_.size(); ++i) {
        auto& racer = racers_[i];
        auto& vehicle = *racer.vehicle;
        auto frame = combat_pose(vehicle, static_cast<std::int32_t>(i + 1));
        frame.route_node = racer.progress.current_node;
        frame.on_branch = racer.progress.on_branch;
        frame.targets = targets;
        frame.jammer_weapons = jammer_weapons;
        if (racer.active_side_target >= 0) {
            const auto& target = racers_[static_cast<std::size_t>(racer.active_side_target)];
            frame.guided_target =
                GuidedWeaponTarget{target.vehicle->state().position, target.progress.current_node};
        }
        VehicleCombatServices services;
        services.select_guided_target = [&, i] {
            const auto result = refresh_side_target(i, error);
            if (result == Result::ok)
                racers_[i].active_side_target = racers_[i].side_target;
            return result;
        };
        services.random = [this] { return random(); };
        services.impact = [&, i](const VehicleCombatImpact& impact) {
            const auto slot = vehicle.combat()->slot(impact.slot);
            if (!slot.descriptor)
                return Result::invalid_argument;
            if (impact.rear_explosion) {
                if (impact.projectile >= 5)
                    return Result::invalid_argument;
                const auto launched = rear_blasts_[i][impact.projectile].begin(impact.hit.point);
                if (launched.primary)
                    callbacks_.insert(
                        {RaceCallbackKey::Domain::rear_primary, i, impact.projectile});
                if (launched.secondary)
                    callbacks_.insert(
                        {RaceCallbackKey::Domain::rear_secondary, i, impact.projectile});
                return Result::ok;
            }
            const auto count = vehicle.combat()->guided_missiles().size();
            return impact_event(false, i, impact.slot, impact.projectile, slot.descriptor->name,
                                impact.hit, impact.velocity,
                                count ? static_cast<float>(count) : 1.0f);
        };
        services.damage = [&](const VehicleCombatDamage& damage) {
            return apply_combat_damage(damage.participant, damage.contact_side, damage.hitpoints,
                                       damage.internal_damage, damage.has_impulse, damage.apply_yaw,
                                       damage.impulse, error);
        };
        services.jam = [&](std::size_t index, float seconds) {
            if (index >= jammer_weapons.size())
                return Result::invalid_argument;
            return racers_[jammer_weapons[index].chassis_identity - 1].vehicle->combat()->jam(
                seconds);
        };
        const auto pool_for = [&, i](VehicleCombatEffectKey key, WeaponTrailPool*& pool) {
            for (auto& trail : vehicle_trails_)
                if (trail.participant == i && trail.key.slot == key.slot &&
                    trail.key.projectile == key.projectile && trail.key.kind == key.kind) {
                    pool = &trail.pool;
                    return Result::ok;
                }
            const auto slot = vehicle.combat()->slot(key.slot);
            if (!slot.descriptor)
                return Result::unavailable;
            VehicleTrail trail;
            trail.participant = i;
            trail.key = key;
            const auto result =
                trail.pool.configure(slot.descriptor->name, special_effects_scalar_);
            if (result != Result::ok) {
                error = "Original weapon particle pool unavailable: " +
                        std::string(slot.descriptor->name);
                return result;
            }
            vehicle_trails_.push_back(std::move(trail));
            pool = &vehicle_trails_.back().pool;
            return Result::ok;
        };
        services.effect_count = [pool_for](VehicleCombatEffectKey key, std::size_t& count) {
            WeaponTrailPool* pool{};
            const auto result = pool_for(key, pool);
            if (result == Result::ok)
                count = pool->active_count();
            return result;
        };
        services.effect_step = [pool_for, &shared](const VehicleCombatEffectStep& step) {
            WeaponTrailPool* pool{};
            const auto result = pool_for(step.key, pool);
            return result == Result::ok ? pool->advance(step.delta, step.emitting, step.position,
                                                        step.velocity, shared, step.heading)
                                        : result;
        };
        all_frames.push_back(frame);
        all_services.push_back(std::move(services));
    }
    SceneCombatServices scene;
    scene.impact = [&](const SceneCombatImpact& impact) {
        const auto& pose = scene_combat_.turrets().poses()[impact.key.turret];
        TurretParameters parameters;
        if (parse_turret_parameters(pose.name, parameters) != Result::ok)
            return Result::invalid_argument;
        return impact_event(true, impact.key.turret, WeaponSlot::side, impact.key.projectile,
                            parameters.weapon, impact.hit, impact.velocity, 1);
    };
    scene.damage = [&](const SceneCombatDamageEvent& damage) {
        return apply_combat_damage(damage.participant, damage.contact_side, damage.hitpoints, false,
                                   damage.has_impulse, damage.has_impulse, damage.impulse, error);
    };
    const auto scene_pool = [&](SceneCombatTrailKey key, WeaponTrailPool*& output) {
        for (auto& trail : scene_trails_)
            if (trail.key.turret == key.turret && trail.key.projectile == key.projectile &&
                trail.key.rocket == key.rocket) {
                output = &trail.pool;
                return Result::ok;
            }
        SceneTrail trail;
        trail.key = key;
        const auto result = trail.pool.configure(key.rocket ? "Rocket Launcher" : "Plasma Blaster",
                                                 special_effects_scalar_);
        if (result != Result::ok)
            return result;
        scene_trails_.push_back(std::move(trail));
        output = &scene_trails_.back().pool;
        return Result::ok;
    };
    scene.trail_count = [&](SceneCombatTrailKey key, std::size_t& count) {
        WeaponTrailPool* pool{};
        const auto result = scene_pool(key, pool);
        if (result == Result::ok)
            count = pool->active_count();
        return result;
    };
    scene.advance_trail = [&](SceneCombatTrailKey key, float delta, bool emitting, Vec3 source,
                              Vec3 velocity) {
        WeaponTrailPool* pool{};
        const auto result = scene_pool(key, pool);
        return result == Result::ok ? pool->advance(delta, emitting, source, velocity, shared)
                                    : result;
    };
    VehicleCombatServices trap_services;
    trap_services.damage = all_services.front().damage;
    trap_services.effect_count = [&](VehicleCombatEffectKey key, std::size_t& count) {
        if (key.projectile >= scene_electron_trails_.size())
            return Result::invalid_argument;
        count = scene_electron_trails_[key.projectile].active_count();
        return Result::ok;
    };
    trap_services.effect_step = [&](const VehicleCombatEffectStep& step) {
        if (step.key.projectile >= scene_electron_trails_.size())
            return Result::invalid_argument;
        return scene_electron_trails_[step.key.projectile].advance(
            step.delta, step.emitting, step.position, step.velocity, shared);
    };
    if (chimneys_enabled_)
        for (auto& chimney : chimneys_)
            if (chimney.pool.advance(dt, chimney.position, start_.camera().transform.origin,
                                     shared) != Result::ok)
                return Result::unavailable;
    if (scene_combat_.advance_controllers(
            dt, turret_targets, error,
            race_world_weapons_unlocked(countdown_, static_cast<float>(race_time_),
                                        weapon_lock_seconds_)) != Result::ok)
        return Result::unavailable;
    if (scene_electron_traps_.advance_world(dt, error) != Result::ok)
        return Result::unavailable;
    const auto append_audio = [&](auto& owner) {
        const auto events = owner.drain_audio_events();
        for (const auto& event : events)
            if (event.sample != CombatSample::jammer)
                audio_events_.push_back(event);
    };
    append_audio(scene_combat_);
    append_audio(scene_electron_traps_);
    // 00480b40 updates the active camera after direct world effects and before
    // 004dc9e1 enters the global callback manager.
    for (auto& racer : racers_)
        if (racer.chase_camera.update(racer.vehicle->state(), racer.vehicle->parameters(),
                                      collision_, dt, racer.camera) != Result::ok)
            return Result::unavailable;
    if (start_.update(countdown_, dt, *racers_[player_index()].vehicle, *collision_,
                      racers_[player_index()].progress.finished) != Result::ok) {
        error = "Original race start camera update unavailable";
        return Result::unavailable;
    }
    const float callback_dt = countdown_ >= 3.0f ? dt + dt : dt;
    const auto process_callback = [&](RaceCallbackKey key) -> Result {
        using Domain = RaceCallbackKey::Domain;
        // Native callbacks dereference the live vehicles, including velocity
        // changes made by preceding callbacks in this same captured-count pass.
        for (std::size_t owner = 0; owner < racers_.size(); ++owner) {
            const auto& v = *racers_[owner].vehicle;
            targets[owner].velocity = v.state().velocity;
            turret_targets[owner].velocity = v.state().velocity;
            turret_targets[owner].inactive_timer = v.equipment().dying_timer;
            const auto missiles = v.combat()->guided_missiles();
            for (std::size_t missile = 0; missile < missiles.size(); ++missile)
                missile_positions[owner][missile] = missiles[missile].position;
        }
        if (key.domain == Domain::vehicle) {
            auto& frame = all_frames[key.owner];
            const auto& racer = racers_[key.owner];
            frame.guided_target.reset();
            if (racer.active_side_target >= 0) {
                const auto& target = racers_[static_cast<std::size_t>(racer.active_side_target)];
                frame.guided_target = GuidedWeaponTarget{target.vehicle->state().position,
                                                         target.progress.current_node};
            }
            return racer.vehicle->combat()->dispatch_callback(key.token, callback_dt, frame,
                                                              *collision_, &route_,
                                                              all_services[key.owner], error);
        }
        if (key.domain == Domain::scene)
            return scene_combat_.dispatch_callback(
                key.token, callback_dt, turret_targets, projectile_targets, *collision_, scene,
                error,
                race_world_weapons_unlocked(countdown_, static_cast<float>(race_time_),
                                            weapon_lock_seconds_));
        if (key.domain == Domain::scene_electron)
            return scene_electron_traps_.dispatch_callback(key.token, callback_dt, targets,
                                                           trap_services, error);
        if (key.domain == Domain::impact) {
            auto& pool = impact_pools_[key.owner].pool;
            const auto result = pool.advance(callback_dt, shared);
            if (!pool.active())
                callbacks_.remove(key);
            return result;
        }
        if (key.domain == Domain::impact_sprite) {
            auto& sprite = impact_pools_[key.owner].sprite;
            const auto result = sprite.advance(callback_dt);
            if (!sprite.particle().active)
                callbacks_.remove(key);
            return result;
        }
        if (key.domain == Domain::rear_primary || key.domain == Domain::rear_secondary) {
            auto& pool = rear_blasts_[key.owner][key.token];
            const bool secondary = key.domain == Domain::rear_secondary;
            const auto result = secondary ? pool.advance_secondary(callback_dt, shared)
                                          : pool.advance_primary(callback_dt, shared);
            if (secondary ? !pool.secondary_active() : !pool.primary_active())
                callbacks_.remove(key);
            return result;
        }
        if (key.domain == Domain::vehicle_sparks) {
            auto& pool = vehicle_particles_[key.owner].sparks;
            const auto result = pool.advance(callback_dt, true, {}, {}, shared);
            if (!pool.active())
                callbacks_.remove(key);
            return result;
        }
        if (key.domain == Domain::nitrous) {
            auto& racer = racers_[key.owner];
            auto& vehicle = *racer.vehicle;
            const auto transform = vehicle.transform();
            const auto forward = transform.basis[2];
            NitrousStep boost;
            const auto result = advance_vehicle_nitrous(
                vehicle.equipment(), callback_dt, forward,
                [&](const NitrousStep& step, std::size_t& count) {
                    auto& state_mut = vehicle.collision_state();
                    state_mut.velocity = add(state_mut.velocity, step.velocity_delta);
                    const auto magnitude = std::sqrt(forward.x * forward.x + forward.y * forward.y +
                                                     forward.z * forward.z);
                    const auto mount = vehicle.combat()->node_world_pose(
                        "RearWeaponMount",
                        combat_pose(vehicle, static_cast<std::int32_t>(key.owner + 1)));
                    if (!mount) {
                        error = "Original runtime RearWeaponMount pose unavailable";
                        return Result::unavailable;
                    }
                    auto source = mount->origin;
                    source = sub(source, {forward.x / magnitude * .5f, forward.y / magnitude * .5f,
                                          forward.z / magnitude * .5f});
                    const auto advanced = nitrous_trails_[key.owner].advance(
                        callback_dt, step.emitting, source, state_mut.velocity, shared);
                    count = nitrous_trails_[key.owner].active_count();
                    return advanced;
                },
                boost);
            if (boost.stopped)
                callbacks_.remove(key);
            return result;
        }
        if (key.domain == Domain::weather) {
            const auto& camera = start_.camera();
            return weather_.advance(
                callback_dt,
                {camera.transform.origin, camera.heading_degrees, camera.pitch_degrees, 0},
                *collision_, [this] { return random(); }, error);
        }
        error = "Unknown original race callback domain";
        return Result::unavailable;
    };
    const auto dispatched = callbacks_.dispatch(
        [&](RaceCallbackKey key) {
            const auto result = process_callback(key);
            if (key.domain == RaceCallbackKey::Domain::vehicle) {
                append_audio(*racers_[key.owner].vehicle->combat());
                const auto& vehicle = *racers_[key.owner].vehicle;
                const auto rear = vehicle.combat()->slot(WeaponSlot::rear);
                if (rear.descriptor && rear.descriptor->name == "Missile Jammer") {
                    std::optional<CombatAudioCommand> command;
                    if (combat_jammer_audio(!rear.ready, audio_voice_alive_[2 + key.owner],
                                            vehicle.state().position, vehicle.state().velocity,
                                            audio_resources_, command) != Result::ok)
                        return Result::unavailable;
                    if (command)
                        audio_loop_commands_.push_back({2 + key.owner, *command});
                }
            }
            if (key.domain == RaceCallbackKey::Domain::scene)
                append_audio(scene_combat_);
            if (key.domain == RaceCallbackKey::Domain::scene_electron)
                append_audio(scene_electron_traps_);
            return result;
        },
        error);
    for (std::size_t owner = 0; owner < racers_.size(); ++owner)
        racers_[owner].vehicle->combat()->refresh_visuals(all_frames[owner]);
    return dispatched;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00484d8f
Result RaceSession::advance_audio(float dt, std::string& error) {
    std::vector<CombatAudioSource> fire, nitrous;
    for (std::size_t i = 0; i < racers_.size(); ++i) {
        const auto& vehicle = *racers_[i].vehicle;
        const auto& health = vehicle.equipment();
        fire.push_back({i, vehicle.state().position, vehicle.state().velocity,
                        health.maximum_internal_hp > 0 &&
                            health.internal_hp < health.maximum_internal_hp * .5f});
        nitrous.push_back(
            {i, vehicle.state().position, vehicle.state().velocity, health.nitrous_timer != 0});
    }
    // 00484d8f/00484e27 precede world/global callbacks and the next camera update.
    const auto listener = start_.camera().transform.origin;
    for (std::size_t voice = 0; voice < 2; ++voice) {
        std::optional<CombatAudioCommand> command;
        auto& audio = voice == 0 ? fire_audio_ : nitrous_audio_;
        const auto& sources = voice == 0 ? fire : nitrous;
        if (audio.advance(dt, listener, sources, audio_voice_alive_[voice], audio_resources_,
                          command) != Result::ok) {
            error = "Original shared combat loop audio command unavailable";
            return Result::unavailable;
        }
        if (command)
            audio_loop_commands_.push_back({voice, *command});
    }
    return Result::ok;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0047fd27
void RaceSession::update_ranking() {
    ranking_ = finished_;
    std::vector<std::size_t> running;
    for (std::size_t i = 0; i < racers_.size(); ++i)
        if (std::find(finished_.begin(), finished_.end(), i) == finished_.end())
            running.push_back(i);
    std::stable_sort(running.begin(), running.end(), [&](auto a, auto b) {
        const auto& x = racers_[a].progress;
        const auto& y = racers_[b].progress;
        return x.completed_laps != y.completed_laps
                   ? x.completed_laps > y.completed_laps
                   : route_.ranking_progress(x) > route_.ranking_progress(y);
    });
    ranking_.insert(ranking_.end(), running.begin(), running.end());
    for (std::size_t i = 0; i < ranking_.size(); ++i)
        racers_[ranking_[i]].rank = static_cast<int>(i + 1);
}
FrontendFrame RaceSession::frontend_frame(std::vector<FrontendRacer>& storage) const {
    storage.clear();
    for (std::size_t i = 0; i < racers_.size(); ++i)
        storage.push_back({racers_[i].vehicle->state().position, i, racers_[i].passive_jammer,
                           racers_[i].vehicle->equipment().dying_timer != 0,
                           racers_[i].progress.finished});
    FrontendFrame frame;
    frame.racers = storage;
    frame.ranking = ranking_;
    frame.player_index = player_index();
    const auto& r = racers_[player_index()];
    const auto& v = r.vehicle->state();
    frame.speed = v.cached_speed;
    frame.rpm = v.rpm;
    frame.gear_index = static_cast<int>(v.gear_index);
    frame.reverse = v.reverse_engaged;
    if (paused_)
        frame.status_text = "PAUSED";
    frame.start_countdown = start_.displayed_remaining();
    // 00482339..004823af: the warning uses strict race-time comparison,
    // unlike the selected equipment icon's brightness predicate.
    frame.weapon_lock_warning = warning_countdown_ready_ &&
                                static_cast<float>(race_time_) < weapon_lock_seconds_ &&
                                r.vehicle->equipment().dying_timer == 0;
    frame.race_info = FrontendRaceInfo{r.progress.completed_laps, options_.laps, r.rank,
                                       static_cast<int>(racers_.size())};
    FrontendEquipmentFrame equipment;
    equipment.selected_item = selected_hud_item_;
    equipment.nitrous_ammunition = r.vehicle->equipment().nitrous_ammo;
    const auto hud_threshold = countdown_ != 0 ? std::trunc(static_cast<float>(race_time_) + 2.0f)
                                               : static_cast<float>(weapon_lock_seconds_);
    equipment.weapons_unlocked = hud_threshold <= static_cast<float>(race_time_);
    equipment.internal_hp = r.vehicle->equipment().internal_hp;
    equipment.maximum_internal_hp = r.vehicle->equipment().maximum_internal_hp;
    equipment.armor_hp = r.vehicle->equipment().armor_hp;
    equipment.maximum_armor_hp = r.vehicle->equipment().maximum_armor_hp;
    for (std::size_t slot = 0; slot < 3; ++slot) {
        const auto weapon = r.vehicle->combat()->slot(static_cast<WeaponSlot>(slot));
        equipment.ammunition[slot] = weapon.ammunition;
        equipment.present[slot] = weapon.descriptor != nullptr;
    }
    frame.equipment = equipment;
    return frame;
}
RaceDebugSnapshot RaceSession::debug_snapshot() const {
    RaceDebugSnapshot result;
    result.combat_status = "Recovered Single Race combat; original-runtime comparison pending";
    result.selected_hud_item = selected_hud_item_;
    result.diagnostic_weapon_lock_seconds = weapon_lock_seconds_;
    result.turret_launched = scene_combat_.launched_projectiles();
    result.turret_impacts = scene_combat_.vehicle_impacts();
    for (const auto& turret : scene_combat_.turrets().poses())
        result.turrets.push_back({turret.name, turret.world_position, turret.heading, turret.pitch,
                                  turret.target ? static_cast<int>(*turret.target) : -1});
    for (std::size_t i = 0; i < racers_.size(); ++i) {
        const auto& racer = racers_[i];
        const auto& equipment = racer.vehicle->equipment();
        RaceEquipmentDebugState state;
        state.participant = static_cast<int>(i);
        state.character = racer.character;
        state.tires = racer.loadout.tires.type + "/" + racer.loadout.tires.quality;
        state.armor = racer.loadout.armor.type + "/" + racer.loadout.armor.quality;
        state.selected_weapons = racer.loadout.weapons;
        for (const auto& item : racer.loadout.drivetrain)
            state.drivetrain.push_back(item.type + "/" + item.quality);
        state.internal_hp = equipment.internal_hp;
        state.maximum_internal_hp = equipment.maximum_internal_hp;
        state.armor_hp = equipment.armor_hp;
        state.nitrous_ammo = equipment.nitrous_ammo;
        state.nitrous_timer = equipment.nitrous_timer;
        state.dying_timer = equipment.dying_timer;
        state.ai_side_delay = racer.combat_ai.side_delay_4ad4;
        state.ai_rear_timer = racer.combat_ai.rear_timer_0c;
        for (std::size_t slot = 0; slot < 3; ++slot) {
            const auto view = racer.vehicle->combat()->slot(static_cast<WeaponSlot>(slot));
            state.weapons[slot] = {view.descriptor ? std::string(view.descriptor->name)
                                                   : std::string{},
                                   view.ammunition,
                                   view.descriptor != nullptr,
                                   view.active,
                                   view.ready,
                                   view.pending,
                                   view.deployment_roll,
                                   view.elapsed,
                                   slot == 1 ? racer.active_side_target : -1,
                                   slot == 1 ? racer.side_target : -1};
        }
        result.equipment.push_back(std::move(state));
    }
    result.track = options_.track;
    result.player_character = options_.car;
    result.phase = paused_                          ? "Paused"
                   : phase_ == RacePhase::countdown ? "Countdown"
                   : phase_ == RacePhase::racing    ? "Racing"
                                                    : "Finished";
    result.total_laps = options_.laps;
    result.participant_count = static_cast<int>(racers_.size());
    result.simulation_ticks = ticks_;
    result.race_time = race_time_;
    for (std::size_t i = 0; i < racers_.size(); ++i) {
        const auto& r = racers_[i];
        const auto& v = r.vehicle->state();
        if (i != player_index()) {
            const auto& a = r.ai.state();
            const auto& o = r.ai.output();
            RaceAiDebugState state;
            state.ai_index = static_cast<int>(i);
            state.state = a.behavior;
            state.substate = -1; // Diagnostic branch has no confirmed native enum field.
            state.character = r.character;
            state.state_name = "Unknown";
            state.substate_name = std::string(o.branch);
            state.completed_laps = r.progress.completed_laps;
            state.rank = r.rank;
            state.route_node = r.progress.current_node;
            state.target_node = o.target_node;
            state.position = v.position;
            state.target = o.target;
            state.speed = v.cached_speed;
            state.target_speed = o.target_speed;
            state.steering = v.steering_degrees;
            state.throttle = v.engine_throttle;
            state.brake = v.brake_amount;
            state.recovery_time = a.recovery_timer;
            result.ai.push_back(std::move(state));
            result.lines.push_back(
                {v.position, o.target, RaceDebugKind::target, static_cast<int>(i), false, true});
            int next_gate = 0;
            while (next_gate < 5 && r.progress.checkpoints[next_gate])
                ++next_gate;
            if (next_gate == 5)
                next_gate = 0;
            const int node_index = route_.checkpoints()[next_gate];
            const auto& gate = route_.point(node_index);
            result.lines.push_back({gate.left, gate.right, RaceDebugKind::checkpoint,
                                    static_cast<int>(i), false, true});
            result.labels.push_back({gate.center, "Next gate AI " + std::to_string(i),
                                     RaceDebugKind::checkpoint, static_cast<int>(i)});
        }
        const auto& ai_state = r.ai.state();
        const auto& ai_output = r.ai.output();
        std::string label = r.character + ": ";
        if (i == player_index() && !player_autopilot_)
            label += "Player control";
        else
            label += "Unknown (" + std::to_string(ai_state.behavior) + ") / " +
                     std::string(ai_output.branch);
        if (r.progress.finished)
            label += " [Finished]";
        result.labels.push_back(
            {v.position, std::move(label), RaceDebugKind::vehicle_state, static_cast<int>(i)});
        // Pair collision is solved in XZ at Y=0. Lift only the diagnostic copy
        // to the vehicle's elevation; never flatten the displayed world geometry.
        auto corners = vehicle_pair_footprint(r.vehicle->parameters(), v);
        for (auto& corner : corners)
            corner.y = v.position.y;
        for (int k = 0; k < 4; ++k)
            result.lines.push_back(
                {corners[k], corners[(k + 1) % 4], RaceDebugKind::collision, static_cast<int>(i)});
    }
    const auto nodes = route_.nodes();
    // Lap.bin stores several non-ground Y values; diagnostics project onto the road.
    // This only affects copied debug vertices, never route/AI/progression data.
    const auto debug_ground = [&](Vec3 value) {
        const auto hit = collision_->sample({value.x, 10000, value.z});
        if (hit.hit) {
            value.y = hit.point.y + .15f;
            return value;
        }
        // Route centers can lie just outside the surface mesh (e.g. Dark Forest
        // nodes16/21). Their serialized Y is not a usable road elevation.
        // Find a nearby real surface for the diagnostic copy; never use raw Y.
        for (float radius : {.5f, 1.f, 2.f, 4.f, 8.f, 16.f})
            for (const auto offset : std::array<Vec3, 8>{{{1, 0, 0},
                                                          {-1, 0, 0},
                                                          {0, 0, 1},
                                                          {0, 0, -1},
                                                          {.7071f, 0, .7071f},
                                                          {-.7071f, 0, .7071f},
                                                          {.7071f, 0, -.7071f},
                                                          {-.7071f, 0, -.7071f}}}) {
                const auto nearby = collision_->sample(
                    {value.x + offset.x * radius, 10000, value.z + offset.z * radius});
                if (nearby.hit) {
                    value.y = nearby.point.y + .15f;
                    return value;
                }
            }
        value.y = std::numeric_limits<float>::quiet_NaN(); // Omit unprojectable debug geometry.
        return value;
    };
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        const auto& node = nodes[i];
        const auto& next = nodes[(i + 1) % nodes.size()];
        result.lines.push_back(
            {debug_ground(node.main.center), debug_ground(next.main.center), RaceDebugKind::route});
        result.labels.push_back(
            {debug_ground(node.main.center), std::to_string(i), RaceDebugKind::route});
        if (node.branch || next.branch)
            result.lines.push_back(
                {debug_ground(node.branch ? node.branch->center : node.main.center),
                 debug_ground(next.branch ? next.branch->center : next.main.center),
                 RaceDebugKind::route, -1, true});
    }
    for (int gate : route_.checkpoints()) {
        const auto& node = route_.point(gate);
        result.lines.push_back(
            {debug_ground(node.left), debug_ground(node.right), RaceDebugKind::checkpoint});
        result.labels.push_back(
            {debug_ground(node.center), "Gate " + std::to_string(gate), RaceDebugKind::checkpoint});
    }
    result.lines.insert(result.lines.end(), collision_lines_.begin(), collision_lines_.end());
    return result;
}
} // namespace roadwars
