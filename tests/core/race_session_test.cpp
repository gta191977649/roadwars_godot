#include "game_src/core/simulation_clock.hpp"
#include "game_src/unresolved/race_collision.hpp"
#include "game_src/unresolved/race_session.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>

using namespace roadwars;
namespace {
void require(bool value, std::string_view message) {
    if (!value)
        throw std::runtime_error(std::string(message));
}
class Files final : public ResourceReader {
  public:
    explicit Files(std::filesystem::path root) : root_(std::move(root)) {}
    Result read(std::string_view name, std::vector<std::byte>& output) override {
        std::ifstream file(root_ / std::filesystem::path(name), std::ios::binary | std::ios::ate);
        if (!file)
            return Result::not_found;
        const auto size = file.tellg();
        if (size < 0)
            return Result::invalid_argument;
        output.resize(static_cast<std::size_t>(size));
        file.seekg(0);
        file.read(reinterpret_cast<char*>(output.data()), size);
        return file ? Result::ok : Result::invalid_argument;
    }

  private:
    std::filesystem::path root_;
};
using Words = std::vector<std::uint32_t>;
Words state_words(const RaceSession& session) {
    Words result;
    auto integer = [&](auto value) { result.push_back(static_cast<std::uint32_t>(value)); };
    auto number = [&](float value) {
        require(std::isfinite(value), "Nonfinite race state");
        integer(std::bit_cast<std::uint32_t>(value));
    };
    auto wide = [&](double value) {
        const auto bits = std::bit_cast<std::uint64_t>(value);
        integer(bits);
        integer(bits >> 32);
    };
    auto vector = [&](Vec3 value) {
        number(value.x);
        number(value.y);
        number(value.z);
    };
    integer(session.phase());
    integer(session.ending());
    integer(session.idle_return_requested());
    number(session.idle_return_remaining());
    integer(session.ticks());
    integer(session.random_state());
    wide(session.time());
    number(session.countdown());
    const auto& start = session.start_sequence();
    vector(start.camera().transform.origin);
    for (const auto axis : start.camera().transform.basis)
        vector(axis);
    number(start.displayed_remaining());
    for (int cue : start.sounds())
        integer(cue);
    for (const auto index : session.ranking())
        integer(index);
    for (const auto& racer : session.racers()) {
        const auto& vehicle = *racer.vehicle;
        const auto& equipment = vehicle.equipment();
        number(equipment.internal_hp);
        number(equipment.maximum_internal_hp);
        for (const auto hp : equipment.armor_hp)
            number(hp);
        for (const auto hp : equipment.maximum_armor_hp)
            number(hp);
        for (const auto upgrade : equipment.engine_upgrades)
            integer(upgrade);
        integer(equipment.nitrous_ammo);
        number(equipment.nitrous_timer);
        number(equipment.nitrous_duration_scalar);
        number(equipment.internal_repair_per_second);
        number(equipment.dying_timer);
        number(equipment.traction_multiplier);
        number(vehicle.disabled_contact_1c());
        require(vehicle.damage_visuals() != nullptr, "Participant lacks actual body damage owner");
        const auto& damage = *vehicle.damage_visuals();
        integer(damage.revision());
        integer(damage.marks().size());
        integer(damage.deformations().size());
        for (const auto& deformation : damage.deformations()) {
            vector(deformation.local_point);
            vector(deformation.displacement);
        }
        for (const auto& mark : damage.marks()) {
            integer(mark.side.value_or(0xffffffffu));
            for (const auto& rectangle : mark.rectangles) {
                integer(rectangle.left);
                integer(rectangle.top);
                integer(rectangle.right);
                integer(rectangle.bottom);
            }
        }
        number(racer.combat_ai.side_delay_4ad4);
        number(racer.combat_ai.rear_timer_0c);
        integer(racer.side_target);
        integer(racer.active_side_target);
        require(vehicle.combat() != nullptr, "Participant has no vehicle-owned combat");
        for (auto slot : {WeaponSlot::front, WeaponSlot::side, WeaponSlot::rear}) {
            const auto weapon = vehicle.combat()->slot(slot);
            integer(weapon.ammunition);
            integer(weapon.active);
            integer(weapon.ready);
            integer(weapon.pending);
            number(weapon.deployment_roll);
            integer(weapon.elapsed.has_value());
            if (weapon.elapsed)
                number(*weapon.elapsed);
        }
        for (const auto& visual : vehicle.combat()->visuals()) {
            integer(visual.slot);
            integer(visual.kind);
            integer(visual.index);
            integer(visual.visible);
            vector(visual.pose.origin);
            for (const auto axis : visual.pose.basis)
                vector(axis);
        }
        vector(racer.camera.transform.origin);
        for (const auto axis : racer.camera.transform.basis)
            vector(axis);
        const auto& state = vehicle.state();
        vector(state.position);
        vector(state.velocity);
        for (float value : {state.heading_degrees, state.pitch_degrees, state.roll_degrees,
                            state.yaw_rate, state.pitch_rate, state.roll_rate, state.rpm,
                            state.cached_speed, state.steering_degrees, state.cached_slip_angle,
                            state.engine_throttle, state.brake_amount})
            number(value);
        integer(state.gear_index);
        integer(state.reverse_engaged);
        integer(state.upshifted);
        for (float value : state.air_time)
            number(value);
        for (float value : vehicle.local_wheel_heights())
            number(value);
        integer(vehicle.wall_contact_count());
        const auto& progress = racer.progress;
        integer(progress.completed_laps);
        integer(progress.current_node);
        integer(progress.branch_recent);
        integer(progress.on_branch);
        integer(progress.finished);
        for (bool value : progress.checkpoints)
            integer(value);
        number(progress.route_percent);
        number(progress.previous_route_percent);
        integer(racer.rank);
        wide(racer.finish_time);
        for (float value : racer.lap_times)
            number(value);
        const auto& ai = racer.ai.state();
        integer(ai.behavior);
        for (float value :
             {ai.recovery_timer, ai.wall_clear_time, ai.recovery_accumulation, ai.desired_offset,
              ai.lateral_offset, ai.offset_cooldown, ai.front_strength, ai.rear_strength,
              ai.left_strength, ai.right_strength, ai.front_speed})
            number(value);
        for (bool value : {ai.moving_backward, ai.front, ai.rear, ai.left, ai.right})
            integer(value);
        const auto& output = racer.ai.output();
        for (float value : {output.engine_throttle, output.brake_amount, output.steering_degrees,
                            output.target_speed, output.heading_error})
            number(value);
        vector(output.target);
        vector(output.relocation);
        integer(output.target_node);
        integer(output.relocate);
        for (char value : output.branch)
            integer(static_cast<unsigned char>(value));
        integer(0);
        integer(vehicle.particle_impacts().size());
        for (const auto& impact : vehicle.particle_impacts()) {
            vector(impact.position);
            vector(impact.velocity);
            number(impact.simulation_scale);
        }
        for (const auto& source : vehicle.tire_particle_sources()) {
            vector(source.position);
            integer(source.emitting);
            integer(source.skid_enabled);
            integer(source.color_argb);
            integer(source.skid_color_argb);
        }
        integer(vehicle.collision_audio_events().size());
        for (const auto& event : vehicle.collision_audio_events()) {
            integer(event.sample);
            vector(event.position);
        }
    }
    integer(session.selected_hud_item());
    integer(session.scene_combat().launched_projectiles());
    integer(session.scene_combat().vehicle_impacts());
    const auto particle_pool = [&](const auto& pool) {
        integer(pool.draw_order().size());
        for (const auto slot : pool.draw_order()) {
            integer(slot);
            const auto& particle = pool.particles()[slot];
            vector(particle.position);
            vector(particle.velocity);
            number(particle.remaining_life);
            number(particle.size);
            integer(particle.color_argb);
            integer(particle.atlas_frame);
            integer(particle.active);
        }
    };
    for (const auto& trail : session.scene_trails())
        particle_pool(trail.pool);
    for (const auto& trail : session.vehicle_trails())
        particle_pool(trail.pool);
    for (const auto& trail : session.nitrous_trails())
        particle_pool(trail);
    for (const auto& effects : session.vehicle_particles()) {
        particle_pool(effects.sparks);
        particle_pool(effects.damage);
        for (const auto& tire : effects.tires)
            particle_pool(tire);
    }
    for (const auto& chimney : session.chimneys())
        particle_pool(chimney.pool);
    for (const auto& trail : session.scene_electron_trails())
        particle_pool(trail);
    for (const auto& trap : session.scene_electron_traps().poses()) {
        number(trap.timer);
        integer(trap.active);
        integer(trap.beam_visible);
        vector(trap.beam_pose.origin);
    }
    for (const auto& particle : session.weather().particles()) {
        integer(particle.visible);
        vector(particle.position);
        number(particle.age);
        number(particle.heading);
    }
    for (const auto& impact : session.impact_pools()) {
        particle_pool(impact.pool);
        const auto& sprite = impact.sprite.particle();
        integer(sprite.active);
        vector(sprite.position);
        number(sprite.remaining_life);
        number(sprite.size);
    }
    for (const auto& owner : session.rear_blast_pools())
        for (const auto& blast : owner) {
            integer(blast.primary_active());
            integer(blast.secondary_active());
            for (const auto slot : blast.primary_order()) {
                integer(slot);
                const auto& particle = blast.primary_particles()[slot];
                vector(particle.position);
                vector(particle.velocity);
                number(particle.remaining_life);
                integer(particle.color_argb);
                integer(particle.atlas_frame);
            }
            integer(0xffffffffu);
            for (const auto slot : blast.secondary_order()) {
                integer(slot);
                const auto& particle = blast.secondary_particles()[slot];
                vector(particle.position);
                vector(particle.velocity);
                number(particle.remaining_life);
                integer(particle.color_argb);
                integer(particle.atlas_frame);
            }
            integer(0xffffffffu);
        }
    return result;
}
void validate_result(const RaceSession& session) {
    require(session.phase() == RacePhase::finished,
            "Race did not finish within 900 simulated seconds");
    const auto racers = session.racers();
    const auto& player = racers[session.player_index()];
    require(session.ending() != RaceEndReason::none, "Terminal menu lacks a finish/death reason");
    if (session.ending() == RaceEndReason::completed) {
        require(player.progress.finished &&
                    player.progress.completed_laps == session.options().laps,
                "Player finish did not follow real lap progression");
        require(player.finish_time > 0 && player.finish_time < session.time(),
                "Idle return did not continue after finish");
    } else
        require(player.vehicle->equipment().dying_timer > 0,
                "Elimination lacks actual death state");
    auto ranks = session.ranking();
    for (std::size_t place = 0; place < ranks.size(); ++place)
        require(racers[ranks[place]].rank == static_cast<int>(place + 1),
                "Result rank differs from the ordered participant list");
    std::sort(ranks.begin(), ranks.end());
    for (std::size_t index = 0; index < racers.size(); ++index) {
        require(ranks[index] == index, "Ranking does not contain every participant exactly once");
        require(racers[index].rank >= 1 && racers[index].rank <= static_cast<int>(racers.size()),
                "Participant rank is outside the race");
    }
    for (int index = 0; index < player.progress.completed_laps; ++index)
        require(player.lap_times[index] > 0, "Finished lap has no measured time");
    std::vector<FrontendRacer> hud_racers;
    const auto hud = session.frontend_frame(hud_racers);
    for (std::size_t i = 0; i < racers.size(); ++i)
        require(hud.racers[i].lap_times == racers[i].lap_times &&
                    hud.racers[i].completed_laps == racers[i].progress.completed_laps &&
                    hud.racers[i].name == racers[i].character,
                "Live time table does not read the current participant lap arrays");
    for (std::size_t i = 0; i < racers.size(); ++i)
        require(hud.racers[i].hidden_on_map == racers[i].passive_jammer &&
                    hud.racers[i].eliminated == (racers[i].vehicle->equipment().dying_timer != 0),
                "HUD map status differs from actual original character/death fields");
    require(hud.race_info && hud.race_info->completed_laps == player.progress.completed_laps &&
                hud.race_info->total_laps == session.options().laps &&
                hud.race_info->rank == player.rank &&
                hud.race_info->racer_count == static_cast<int>(racers.size()) &&
                hud.racers.size() == racers.size() &&
                hud.racers[hud.player_index].finished == player.progress.finished,
            "HUD did not receive the actual race result");
}
struct Scenario {
    TrackCollision collision;
    VehicleParameters parameters;
    Scenario(ResourceStore& resources, const RaceOptions& options) {
        std::vector<std::byte> bytes;
        MeshObject world;
        std::string error;
        require(resources.read("Tracks/" + options.track + "/World/SpatialObject.msh", bytes) ==
                    Result::ok,
                "Cannot read race world");
        require(decode_msh(bytes, world, error) == Result::ok, error);
        require(collision.load(resources, options.track, world, error) == Result::ok, error);
        require(load_vehicle_metrics(resources, options.car, parameters, error) == Result::ok,
                error);
        require(load_vehicle_geometry(resources, options.car, parameters, error) == Result::ok,
                error);
    }
};
using Trace = std::vector<Words>;
Trace run(ResourceStore& resources, const Scenario& scenario, const RaceOptions& options,
          std::span<const double> cadence, const Trace* reference, bool read_debug) {
    std::string error;
    VehicleDynamics player;
    require(player.initialize(scenario.parameters, scenario.collision, error) == Result::ok, error);
    RaceSession session;
    require(session.load(resources, scenario.collision, options, player, error) == Result::ok,
            error);
    require(session.racers().size() == static_cast<std::size_t>(options.ai_count + 1),
            "Wrong race count");
    for (std::size_t i = 0; i < session.racers().size(); ++i) {
        const auto& position = session.racers()[i].vehicle->state().position;
        const auto ground = scenario.collision.sample(position);
        require(ground.hit && std::abs(position.y - (ground.point.y - .5f)) < 1e-5f,
                "Race start did not apply original ground projection");
        std::cout << "RACE_GRID slot=" << i + 1 << " position=" << position.x << ',' << position.y
                  << ',' << position.z << " ground_y=" << ground.point.y << '\n';
    }
    std::set<std::string> characters;
    for (const auto& racer : session.racers())
        require(characters.insert(ResourceStore::canonical(racer.character)).second,
                "Race roster contains a duplicate character");
    require(session.advance(-1, {}, error) == Result::invalid_argument, "Negative delta accepted");
    require(session.advance(std::numeric_limits<float>::quiet_NaN(), {}, error) ==
                Result::invalid_argument,
            "Nonfinite delta accepted");
    require(session.selected_hud_item() == 0, "Fresh native4ab0=1 did not select HUD map0");
    const auto initial = state_words(session);
    InputState short_press;
    short_press.weapon_requested[0] = true;
    short_press.nitrous_requested = true;
    require(session.advance(0, short_press, error) == Result::ok && state_words(session) == initial,
            "Zero-tick combat input mutated simulation");
    for (int cycle = 0; cycle < 3; ++cycle) {
        session.set_paused(true);
        require(session.advance(1, {}, error) == Result::ok && state_words(session) == initial,
                "Paused race advanced state");
        session.set_paused(false);
        auto expected_random = session.random_state();
        for (std::size_t draw = 0; draw < 4 * session.racers().size(); ++draw)
            original_random(expected_random);
        require(session.restart(error) == Result::ok && session.random_state() == expected_random &&
                    session.ticks() == 0,
                "Restart did not restore participants and consume original reset draws");
        require(session.load(resources, scenario.collision, options, player, error) == Result::ok &&
                    state_words(session) == initial,
                "Seeded initialization changed after restart");
    }
    // Verify route topology against decoded resource order, including the wrap
    // and the first segment entering each optional branch.
    const auto debug_state_before = state_words(session);
    const auto snapshot = session.debug_snapshot();
    const auto compact_snapshot = session.debug_snapshot(false);
    require(compact_snapshot.equipment.size() == snapshot.equipment.size() &&
                compact_snapshot.ai.size() == snapshot.ai.size() &&
                compact_snapshot.simulation_ticks == snapshot.simulation_ticks &&
                compact_snapshot.lines.size() < snapshot.lines.size() &&
                state_words(session) == debug_state_before,
            "Optional route diagnostics changed gameplay or omitted participant data");
    require(snapshot.equipment.size() == session.racers().size(),
            "Missing actual loadout diagnostics");
    for (std::size_t i = 0; i < session.racers().size(); ++i) {
        const auto& racer = session.racers()[i];
        const auto& diagnostic = snapshot.equipment[i];
        const auto& parameters = racer.vehicle->parameters();
        require(ResourceStore::canonical(parameters.tires.type) ==
                        ResourceStore::canonical(racer.loadout.tires.type) &&
                    ResourceStore::canonical(parameters.tires.quality) == "BEST" &&
                    parameters.engine_upgrades == racer.vehicle->equipment().engine_upgrades &&
                    diagnostic.selected_weapons == racer.loadout.weapons &&
                    diagnostic.armor_hp == racer.vehicle->equipment().armor_hp,
                "Race loadout/physics/debug state disagree");
    }
    const auto nodes = session.route().nodes();
    std::size_t route_edge = 0;
    std::vector<const RaceDebugLine*> route_lines;
    for (const auto& line : snapshot.lines)
        if (line.kind == RaceDebugKind::route)
            route_lines.push_back(&line);
    const auto expect_edge = [&](Vec3 from, Vec3 to, bool branch) {
        require(route_edge < route_lines.size(), "Missing route overlay segment");
        const auto& line = *route_lines[route_edge++];
        require(line.from.x == from.x && line.from.z == from.z && line.to.x == to.x &&
                    line.to.z == to.z && line.branch == branch,
                "Route overlay order differs from resource topology");
    };
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        const auto& node = nodes[i];
        const auto& next = nodes[(i + 1) % nodes.size()];
        expect_edge(node.main.center, next.main.center, false);
        if (node.branch || next.branch)
            expect_edge(node.branch ? node.branch->center : node.main.center,
                        next.branch ? next.branch->center : next.main.center, true);
    }
    require(route_edge == route_lines.size(), "Unexpected route overlay segments");
    if (ResourceStore::canonical(options.track) == "DARK FOREST") {
        for (const auto& label : snapshot.labels)
            if (label.kind == RaceDebugKind::route && (label.text == "16" || label.text == "21"))
                require(std::isfinite(label.position.y) && std::abs(label.position.y) < 100,
                        "Route debug used the invalid serialized node Y");
    }
    session.set_player_autopilot(true);
    SimulationClock clock;
    Trace trace;
    std::size_t compared{};
    bool terminal_key_checked{};
    constexpr std::uint64_t maximum_ticks = 60 * 900;
    auto tick = [&](float dt, const InputState& input) {
        if (session.idle_return_requested() || session.ticks() >= maximum_ticks)
            return Result::ok;
        struct PriorTarget {
            bool active;
            int ammunition, target;
        };
        std::vector<PriorTarget> prior_targets;
        for (const auto& racer : session.racers()) {
            const auto slot = racer.vehicle->combat()->slot(WeaponSlot::side);
            prior_targets.push_back({slot.active, slot.ammunition, racer.active_side_target});
        }
        auto actual_input = input;
        const bool check_terminal =
            session.ending() != RaceEndReason::none && !terminal_key_checked;
        if (check_terminal)
            actual_input.any_key_down = true;
        const auto result = session.advance(dt, actual_input, error);
        if (check_terminal) {
            require(std::abs(session.idle_return_remaining() - (10 - dt)) < .0001f,
                    "Terminal physical key did not reset original10-second idle timer");
            terminal_key_checked = true;
        }
        require(result == Result::ok, error);
        for (std::size_t owner = 0; owner < session.racers().size(); ++owner) {
            const auto& racer = session.racers()[owner];
            const auto slot = racer.vehicle->combat()->slot(WeaponSlot::side);
            if (slot.active && prior_targets[owner].active &&
                slot.ammunition == prior_targets[owner].ammunition)
                require(racer.active_side_target == prior_targets[owner].target,
                        "AI query target retargeted an already-active guided weapon");
        }
        const auto state = state_words(session);
        if (session.phase() == RacePhase::countdown)
            for (const auto& racer : session.racers())
                require(racer.vehicle->state().engine_throttle == 0,
                        "A participant accelerated during the countdown");
        if (session.ticks() == 600) {
            const auto events = session.dispatch_trace();
            require(events.size() == session.racers().size() * 2,
                    "Native controller/force dispatch omitted a live participant");
            require(events.front().stage == RaceSession::DispatchEvent::Stage::controller &&
                        events.front().participant == session.player_index(),
                    "Selected controller was not dispatched before all participant forces");
            std::size_t event = 1;
            for (std::size_t owner = 0; owner < session.racers().size(); ++owner) {
                if (owner != session.player_index()) {
                    require(events[event].stage == RaceSession::DispatchEvent::Stage::controller &&
                                events[event++].participant == owner,
                            "AI combat controller did not precede its own force phase");
                }
                require(events[event].stage == RaceSession::DispatchEvent::Stage::force &&
                            events[event++].participant == owner,
                        "Participant force phase violated native interleaving");
            }
            session.set_paused(true);
            require(session.advance(5, {}, error) == Result::ok && state_words(session) == state,
                    "Mid-race pause advanced physics or race timers");
            session.set_paused(false);
        }
        if (read_debug && session.ticks() % 15 == 0) {
            const auto debug = session.debug_snapshot();
            require(debug.ai.size() == static_cast<std::size_t>(options.ai_count) &&
                        debug.simulation_ticks == session.ticks() &&
                        debug.race_time == session.time() &&
                        debug.participant_count == options.ai_count + 1 && !debug.lines.empty(),
                    "Debug snapshot does not describe the actual race");
            std::size_t edges = 0;
            for (const auto& line : debug.lines) {
                if (line.kind != RaceDebugKind::collision || line.ai_index < 0)
                    continue;
                const auto& vehicle = *session.racers()[line.ai_index].vehicle;
                require(line.from.y == vehicle.state().position.y &&
                            line.to.y == vehicle.state().position.y,
                        "Collision overlay was flattened to world zero");
                const auto corners = vehicle_pair_footprint(vehicle.parameters(), vehicle.state());
                require(std::any_of(corners.begin(), corners.end(),
                                    [&](Vec3 point) {
                                        return point.x == line.from.x && point.z == line.from.z;
                                    }),
                        "Collision overlay differs from solver XZ footprint");
                ++edges;
            }
            require(edges == session.racers().size() * 4, "Missing collision overlay edges");
            for (const auto& ai : debug.ai) {
                require(ai.ai_index >= 0 && ai.ai_index < options.ai_count,
                        "Invalid debug AI index");
                const auto& original = session.racers()[ai.ai_index];
                require(ai.route_node == original.progress.current_node &&
                            ai.completed_laps == original.progress.completed_laps &&
                            ai.speed == original.vehicle->state().cached_speed,
                        "AI table differs from simulation state");
            }
            require(state_words(session) == state, "Debug reads mutated simulation state");
        }
        if (reference) {
            require(compared < reference->size(), "Race took more ticks than the reference");
            if ((*reference)[compared] != state)
                throw std::runtime_error("Race frame-rate/debug divergence at simulation tick " +
                                         std::to_string(compared + 1));
        } else
            trace.push_back(state);
        ++compared;
        return Result::ok;
    };
    std::size_t frame{};
    while (!session.idle_return_requested() && session.ticks() < maximum_ticks) {
        require(clock.advance(cadence[frame++ % cadence.size()], {}, tick) == Result::ok, error);
        if (!reference && session.ticks() % 3600 == 0) {
            const auto& racer = session.racers()[session.player_index()];
            std::cout << "RACE_PROGRESS ticks=" << session.ticks()
                      << " node=" << racer.progress.current_node
                      << " lap=" << racer.progress.completed_laps
                      << " position=" << racer.vehicle->state().position.x << ','
                      << racer.vehicle->state().position.y << ','
                      << racer.vehicle->state().position.z
                      << " speed=" << racer.vehicle->state().cached_speed
                      << " target_speed=" << racer.ai.output().target_speed
                      << " throttle=" << racer.ai.output().engine_throttle
                      << " brake=" << racer.ai.output().brake_amount
                      << " steer=" << racer.ai.output().steering_degrees
                      << " recovery=" << racer.ai.state().recovery_timer
                      << " behavior=" << racer.ai.state().behavior
                      << " branch=" << racer.ai.output().branch << '\n';
        }
    }
    require(session.start_sequence().sounds().size() == 4, "Countdown missed or repeated a sound");
    for (int i = 0; i < 4; ++i)
        require(session.start_sequence().sounds()[i] == i, "Countdown sound order is incorrect");
    require(session.idle_return_requested(),
            "Original finish/death idle menu was not requested within 900 seconds");
    validate_result(session);
    if (!reference) {
        const auto player_slot = session.player_index();
        const auto count = session.racers().size();
        require(session.select_native_camera(5) == Result::ok, "Track spectator mode unavailable");
        const auto first_target = session.spectator_target();
        require(session.select_native_camera(5) == Result::ok &&
                    session.spectator_target() == (first_target + 1) % count,
                "Repeated native camera key did not cycle the full participant array");
        const auto prior_target = session.spectator_target();
        require(session.select_native_camera(4) == Result::ok &&
                    session.spectator_target() == prior_target,
                "Changing camera mode advanced the native target on its first press");
        require(session.select_native_camera(4) == Result::ok &&
                    session.spectator_target() == (prior_target + 1) % count &&
                    session.player_index() == player_slot,
                "Native spectator cycling changed player ownership or failed wraparound");
        const auto time = session.time();
        session.toggle_time_table();
        require(session.time_table_remaining() == 0, "F11 did not hide the live table");
        session.toggle_time_table();
        require(session.time_table_remaining() == 15 && session.time() == time,
                "F11 did not restore15 seconds without advancing race time");
        session.acknowledge_idle_menu();
        require(!session.idle_return_requested() && session.idle_return_remaining() == 0,
                "Menu acknowledgement retained the synthetic Escape request");
    }
    if (reference)
        require(compared == reference->size(), "Race finished with a different tick count");
    const auto final_tick = session.ticks();
    require(session.advance(1.f / 60.f, {}, error) == Result::ok &&
                session.ticks() == final_tick + 1,
            "Original terminal race stopped world simulation before returning to menu");
    std::cout << "ROADWARS_RACE_SESSION_OK track=" << options.track << " car=" << options.car
              << " ai=" << options.ai_count << " time=" << session.time()
              << " ticks=" << session.ticks() << " debug_reads=" << read_debug << '\n';
    for (const auto& racer : session.racers())
        std::cout << "RACE_RESULT " << racer.character << " laps=" << racer.progress.completed_laps
                  << " rank=" << racer.rank << " node=" << racer.progress.current_node << '\n';
    if (!reference) {
        VehicleDynamics eliminated_player;
        require(eliminated_player.initialize(scenario.parameters, scenario.collision, error) ==
                    Result::ok,
                error);
        RaceSession eliminated;
        require(eliminated.load(resources, scenario.collision, options, eliminated_player, error) ==
                    Result::ok,
                error);
        eliminated_player.equipment().armor_hp[2] = 0;
        const auto origin = eliminated_player.state().position;
        const auto rear = eliminated_player.transform().basis[2];
        const auto half = eliminated_player.parameters().length * .5f;
        const Vec3 point{origin.x - rear.x * half, origin.y, origin.z - rear.z * half};
        require(apply_vehicle_collision_damage(eliminated_player, {1.e8f, 0, 0}, point, false, true,
                                               3) == Result::ok,
                "Recovered collision damage failed");
        require(eliminated_player.equipment().internal_hp == 0 &&
                    std::count_if(eliminated.audio_events().begin(),
                                  eliminated.audio_events().end(),
                                  [](const CombatAudioEvent& event) {
                                      return event.sample == CombatSample::eliminated;
                                  }) == 1,
                "Accepted selected lethal damage missed native elimination cue");
        require(eliminated.advance(1.f / 60.f, {}, error) == Result::ok, error);
        require(eliminated.ending() == RaceEndReason::eliminated &&
                    !eliminated.idle_return_requested(),
                "Actual selected death did not enter original idle-menu ending");
        require(!eliminated.racers()[eliminated.player_index()].progress.finished,
                "Death invented lap completion");
        const auto death_tick = eliminated.ticks();
        for (unsigned i = 0; i < 601 && !eliminated.idle_return_requested(); ++i)
            require(eliminated.advance(1.f / 60.f, {}, error) == Result::ok, error);
        require(eliminated.idle_return_requested() && eliminated.ticks() > death_tick,
                "Eliminated race froze or missed strict idle timeout");
        validate_result(eliminated);
        require(eliminated.restart(error) == Result::ok &&
                    eliminated.ending() == RaceEndReason::none &&
                    !eliminated.idle_return_requested() &&
                    eliminated_player.equipment().internal_hp > 0,
                "Restart retained selected death/ending state");
    }
    require(session.restart(error) == Result::ok, error);
    require(session.ticks() == 0 && session.time() == 0, "Restart retained combat simulation time");
    require(session.load(resources, scenario.collision, options, player, error) == Result::ok &&
                state_words(session) == initial,
            "Repeated initialization did not restore the seeded initial race");
    return trace;
}
} // namespace
int main(int argc, char** argv) {
    try {
        std::cout.setf(std::ios::unitbuf);
        Files empty_files({});
        ResourceStore empty_resources(empty_files);
        TrackCollision empty_collision;
        VehicleDynamics empty_player;
        RaceSession empty;
        std::string error;
        require(empty.restart(error) != Result::ok, "Empty session restarted");
        for (const auto [laps, count] : {std::pair{0, 5}, {11, 5}, {3, -1}, {3, 6}}) {
            RaceOptions options;
            options.laps = laps;
            options.ai_count = count;
            require(empty.load(empty_resources, empty_collision, options, empty_player, error) ==
                        Result::invalid_argument,
                    "Invalid race options accepted");
        }
        for (const auto [remaining, expected] : {std::pair{9.f, -1},
                                                 {4.f, -1},
                                                 {3.9f, 3},
                                                 {3.f, 3},
                                                 {2.99f, 2},
                                                 {1.f, 1},
                                                 {.99f, 0},
                                                 {0.f, -1}})
            require(RaceStart::digit(remaining) == expected, "Incorrect countdown digit boundary");
        if (argc < 2) {
            std::cout
                << "Race option/lifecycle contracts passed; original-asset acceptance is opt-in\n";
            return 0;
        }
        Files files(argv[1]);
        ResourceStore resources(files);
        require(resources.open() == Result::ok, "Cannot open original resources");
        RaceOptions options;
        options.laps = 1;
        if (argc > 2)
            options.track = argv[2];
        if (argc > 3)
            options.car = argv[3];
        if (argc > 4)
            options.ai_count = std::stoi(argv[4]);
        bool independence = false;
        for (int index = 5; index < argc; ++index) {
            const std::string_view argument(argv[index]);
            if (argument == "--frame-independent")
                independence = true;
            else if (argument.starts_with("--laps="))
                options.laps = std::stoi(std::string(argument.substr(7)));
            else
                throw std::runtime_error("Unknown race probe argument: " + std::string(argument));
        }
        Scenario scenario(resources, options);
        const std::array<double, 1> normal{1.0 / 60};
        const auto reference = run(resources, scenario, options, normal, nullptr, false);
        if (independence) {
            for (int fps : {10, 15, 30, 60, 75, 120, 144, 240}) {
                const std::array<double, 1> cadence{1.0 / fps};
                run(resources, scenario, options, cadence, &reference, true);
                std::cout << "RACE_FRAME_RATE_OK fps=" << fps << " ticks=" << reference.size()
                          << " bit-identical complete race state\n";
            }
            const std::array<double, 6> jitter{1.0 / 240, 1.0 / 30, .35, 1.0 / 144, .002, .9};
            run(resources, scenario, options, jitter, &reference, true);
            std::cout << "RACE_FRAME_INDEPENDENT_OK jitter=350/900ms no elapsed time dropped\n";
        }
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
