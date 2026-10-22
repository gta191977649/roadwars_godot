#include "game_src/unresolved/race_session.hpp"
#include "game_src/unresolved/contact_orientation.hpp"
#include "game_src/unresolved/race_collision.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace roadwars {
namespace {
Vec3 add(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 sub(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
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
    const float w = p.width * .5f, l = p.length * .5f;
    body.diagonal_radius = std::sqrt(w * w + l * l);
    body.corner_angle_degrees =
        static_cast<float>(std::atan(static_cast<double>(w / l)) * 57.288352966308594);
    return collision_footprint(body);
}
} // namespace
RaceSession::~RaceSession() {
    for (auto& racer : racers_)
        if (racer.vehicle)
            racer.vehicle->set_respawn_handler({});
}
std::uint32_t RaceSession::random() {
    // 005170b0: original CRT sequence; seed selection belongs to the diagnostic host.
    random_state_ = random_state_ * 0x343fdu + 0x269ec3u;
    return (random_state_ >> 16) & 0x7fffu;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004a527a
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00484189
Result RaceSession::load(ResourceStore& resources, const TrackCollision& collision,
                         const RaceOptions& options, VehicleDynamics& player, std::string& error) {
    if (options.laps < 1 || options.laps > 10 || options.ai_count < 0 || options.ai_count > 5) {
        error = "Race requires 1..10 laps and 0..5 AI";
        return Result::invalid_argument;
    }
    for (auto& racer : racers_)
        if (racer.vehicle)
            racer.vehicle->set_respawn_handler({});
    racers_.clear();
    spawns_.clear();
    options_ = options;
    collision_ = &collision;
    random_state_ = options.seed;
    if (route_.load(resources, options.track, error) != Result::ok)
        return Result::unavailable;
    Configuration config;
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
        racers_.push_back(std::move(racer));
    }
    for (std::size_t i = 0; i < racers_.size(); ++i)
        racers_[i].vehicle->set_respawn_handler([this, i] { return rescue(i); });
    if (start_.load(resources, world, options.car, spawns_, collision, error) != Result::ok)
        return Result::unavailable;
    return restart(error);
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0048355e
Result RaceSession::restart(std::string& error) {
    if (racers_.empty())
        return Result::unavailable;
    race_time_ = 0;
    countdown_ = initial_countdown_;
    ticks_ = 0;
    scanner_ = 0;
    paused_ = false;
    phase_ = RacePhase::countdown;
    finished_.clear();
    collision_lines_.clear();
    for (std::size_t i = 0; i < racers_.size(); ++i) {
        auto& racer = racers_[i];
        if (racer.vehicle->initialize(racer.vehicle->parameters(), *collision_, error) !=
            Result::ok)
            return Result::unavailable;
        racer.vehicle->reset_to(spawns_[i]);
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
    start_.reset();
    if (start_.update(countdown_, 0, *racers_[player_index()].vehicle, *collision_) != Result::ok)
        return Result::unavailable;
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
    if (paused_ || phase_ == RacePhase::finished || dt == 0)
        return Result::ok;
    if (input.reset_requested)
        return restart(error);
    ++ticks_;
    scanner_ = (scanner_ + 1) % racers_.size();
    const bool go = countdown_ < 1;
    if (go) {
        race_time_ += dt;
        phase_ = RacePhase::racing;
    }
    std::vector<VehicleState> previous;
    std::vector<RaceAiObstacle> obstacles;
    for (const auto& r : racers_) {
        previous.push_back(r.vehicle->state());
        obstacles.push_back({r.vehicle->state().position, r.vehicle->state().velocity,
                             footprint(*r.vehicle), true});
    }
    for (std::size_t i = 0; i < racers_.size(); ++i) {
        auto& r = racers_[i];
        const bool ai = i != player_index() || player_autopilot_;
        if (!go)
            r.vehicle->begin_race_update(dt, VehicleControls{0, 1, 0});
        else if (r.progress.finished)
            r.vehicle->begin_race_update(
                dt, VehicleControls{0, 0, r.vehicle->state().steering_degrees});
        else if (ai) {
            std::vector<RaceAiObstacle> others;
            for (std::size_t j = 0; j < obstacles.size(); ++j)
                if (j != i)
                    others.push_back(obstacles[j]);
            const float grip = r.vehicle->longitudinal_grip();
            if (r.ai.update(route_, r.progress, r.vehicle->state(), r.vehicle->parameters(), others,
                            grip, static_cast<float>(race_time_), scanner_ == i, dt,
                            error) != Result::ok)
                return Result::unavailable;
            const auto& controls = r.ai.output();
            if (controls.relocate) {
                auto pose = rescue(i);
                pose.position = controls.relocation;
                r.vehicle->reset_to(pose);
            }
            previous[i] = r.vehicle->state();
            r.vehicle->begin_race_update(dt, VehicleControls{controls.engine_throttle,
                                                             controls.brake_amount,
                                                             controls.steering_degrees});
        } else
            r.vehicle->begin_race_update(dt, input);
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
    const auto walls = [&](std::size_t i, float remaining) {
        auto& r = racers_[i];
        const auto before = r.vehicle->wall_contact_count();
        r.vehicle->solve_race_walls(remaining);
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
        if (resolve_vehicle_collision(*racers_[a].vehicle, *racers_[b].vehicle, previous[a],
                                      previous[b], remaining, &collision_lines_, &leftover)) {
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
    if (contact_overflow) {
        error = "Original pair collision worklist capacity exceeded";
        return Result::unavailable;
    }
    // 00480b40 completes wheel/suspension/transmission after the shared collision phase.
    for (auto& racer : racers_)
        racer.vehicle->finish_race_update(dt);
    for (std::size_t i = 0; i < racers_.size(); ++i) {
        auto& r = racers_[i];
        if (!go || r.progress.finished)
            continue;
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
    update_ranking();
    if (racers_[player_index()].progress.finished)
        phase_ = RacePhase::finished;
    for (auto& racer : racers_)
        if (racer.chase_camera.update(racer.vehicle->state(), racer.vehicle->parameters(),
                                      collision_, dt, racer.camera) != Result::ok)
            return Result::unavailable;
    if (start_.update(countdown_, dt, *racers_[player_index()].vehicle, *collision_) !=
        Result::ok) {
        error = "Original race start camera update unavailable";
        return Result::unavailable;
    }
    countdown_ = std::max(0.f, countdown_ - dt);
    error.clear();
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
        storage.push_back(
            {racers_[i].vehicle->state().position, i, false, false, racers_[i].progress.finished});
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
    frame.race_info = FrontendRaceInfo{r.progress.completed_laps, options_.laps, r.rank,
                                       static_cast<int>(racers_.size())};
    return frame;
}
RaceDebugSnapshot RaceSession::debug_snapshot() const {
    RaceDebugSnapshot result;
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
