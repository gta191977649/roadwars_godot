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
    integer(session.ticks());
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
    return result;
}
void validate_result(const RaceSession& session) {
    require(session.phase() == RacePhase::finished,
            "Race did not finish within 900 simulated seconds");
    const auto racers = session.racers();
    const auto& player = racers[session.player_index()];
    require(player.progress.finished && player.progress.completed_laps == session.options().laps,
            "Player finish did not follow real lap progression");
    require(player.finish_time > 0 && player.finish_time == session.time(),
            "Invalid player finish time");
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
    for (int index = 0; index < session.options().laps; ++index)
        require(player.lap_times[index] > 0, "Finished lap has no measured time");
    std::vector<FrontendRacer> hud_racers;
    const auto hud = session.frontend_frame(hud_racers);
    require(hud.race_info && hud.race_info->completed_laps == session.options().laps &&
                hud.race_info->total_laps == session.options().laps &&
                hud.race_info->rank == player.rank &&
                hud.race_info->racer_count == static_cast<int>(racers.size()) &&
                hud.racers.size() == racers.size() && hud.racers[hud.player_index].finished,
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
    const auto initial = state_words(session);
    for (int cycle = 0; cycle < 3; ++cycle) {
        session.set_paused(true);
        require(session.advance(1, {}, error) == Result::ok && state_words(session) == initial,
                "Paused race advanced state");
        session.set_paused(false);
        require(session.restart(error) == Result::ok && state_words(session) == initial,
                "Restart did not restore the initial race");
    }
    // Verify route topology against decoded resource order, including the wrap
    // and the first segment entering each optional branch.
    const auto snapshot = session.debug_snapshot();
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
    auto tick = [&](float dt, const InputState& input) {
        if (session.phase() == RacePhase::finished)
            return Result::ok;
        const auto result = session.advance(dt, input, error);
        require(result == Result::ok, error);
        const auto state = state_words(session);
        if (session.phase() == RacePhase::countdown)
            for (const auto& racer : session.racers())
                require(racer.vehicle->state().engine_throttle == 0,
                        "A participant accelerated during the countdown");
        if (session.ticks() == 600) {
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
    while (session.phase() != RacePhase::finished && session.ticks() < 60 * 900) {
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
    validate_result(session);
    if (reference)
        require(compared == reference->size(), "Race finished with a different tick count");
    const auto finished = state_words(session);
    require(session.advance(1, {}, error) == Result::ok && state_words(session) == finished,
            "Finished result changed after additional updates");
    std::cout << "ROADWARS_RACE_SESSION_OK track=" << options.track << " car=" << options.car
              << " ai=" << options.ai_count << " time=" << session.time()
              << " ticks=" << session.ticks() << " debug_reads=" << read_debug << '\n';
    for (const auto& racer : session.racers())
        std::cout << "RACE_RESULT " << racer.character << " laps=" << racer.progress.completed_laps
                  << " rank=" << racer.rank << " node=" << racer.progress.current_node << '\n';
    require(session.restart(error) == Result::ok, error);
    const auto restarted = state_words(session);
    if (restarted != initial) {
        const auto mismatch =
            std::mismatch(restarted.begin(), restarted.end(), initial.begin(), initial.end());
        throw std::runtime_error("Post-finish restart divergence at state word " +
                                 std::to_string(mismatch.first - restarted.begin()));
    }
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
