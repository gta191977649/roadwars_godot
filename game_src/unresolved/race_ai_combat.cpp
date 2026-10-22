#include "game_src/unresolved/race_ai_combat.hpp"
#include "game_src/unresolved/mesh_scene.hpp"
#include "game_src/unresolved/resource_store.hpp"
#include "game_src/unresolved/track_collision.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>

namespace roadwars {
namespace {
bool intersects(Vec3 a, Vec3 b, Vec3 c, Vec3 d) {
    const float x = a.x - b.x, z = a.z - b.z, u = -(c.x - d.x), v = -(c.z - d.z);
    if ((u == 0 && v == 0) || (x == 0 && z == 0))
        return false;
    const auto between = [](float p, float q, float r) {
        return (p <= q && r <= p) || (p <= r && q <= p);
    };
    if (u == 0 && x == 0) {
        if (a.x != c.x)
            return false;
        if (between(a.z, c.z, d.z) || between(b.z, c.z, d.z))
            return true;
    }
    if (v == 0 && z == 0) {
        if (a.z != c.z)
            return false;
        if (between(a.x, c.x, d.x) || between(b.x, c.x, d.x))
            return true;
    }
    if (u != 0 && x != 0 && z / x == v / u)
        return false;
    float t{}, s{};
    if (v == 0 || u == 0) {
        if (u == 0) {
            if (v != 0)
                t = (c.x - a.x) / x;
        } else
            t = (c.z - a.z) / z;
    } else
        t = ((c.z - a.z) + (c.x - a.x) * (-v / u)) / (x * (-v / u) + z);
    if (x == 0 || z == 0) {
        if (z == 0) {
            if (x != 0)
                s = (c.z - a.z) / v;
        } else
            s = (c.x - a.x) / u;
    } else
        s = ((c.z - a.z) + (c.x - a.x) * (-z / x)) / (u * (-z / x) + v);
    return !(t > 0 || t < -1 || s > 0 || s < -1);
}
} // namespace
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0041a1cd
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0047c368
bool race_ai_side_wall_blocked(Vec3 position, float heading, std::span<const WallRecord> walls) {
    const float angle = (heading + 90.0f) * .017455555498600006f;
    const Vec3 end{position.x + std::sin(angle) * 3, position.y, position.z + std::cos(angle) * 3};
    for (const auto& wall : walls)
        if (intersects(position, end, wall.lower, wall.upper))
            return true;
    return false;
}
namespace {
float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
bool same_ascii(std::string_view first, std::string_view second) {
    if (first.size() != second.size())
        return false;
    const auto lower = [](unsigned char c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; };
    for (std::size_t i = 0; i < first.size(); ++i)
        if (lower(static_cast<unsigned char>(first[i])) !=
            lower(static_cast<unsigned char>(second[i])))
            return false;
    return true;
}
bool opposing(const RaceAiCombatVehicleView& self, const RaceAiCombatVehicleView& other) {
    return self.chassis_identity != other.chassis_identity && other.disabled_134 == 0.0f;
}
Result unavailable(std::string& error, std::string_view name) {
    error = "Original AI combat service unavailable: ";
    error += name;
    return Result::unavailable;
}
Result read_weapon(const RaceAiCombatServices& services, std::size_t slot,
                   RaceAiCombatWeaponView& weapon, std::string& error) {
    if (!services.weapon)
        return unavailable(error, "weapon field view");
    const auto result = services.weapon(slot, weapon);
    if (result != Result::ok)
        error = "Original AI combat weapon field view failed";
    return result;
}
Result activate(const RaceAiCombatServices& services, std::size_t slot, RaceAiCombatTrace& trace,
                std::string& error) {
    if (!services.activate)
        return unavailable(error, "004242cb activation");
    ++trace.activation_attempts[slot];
    const auto result = services.activate(slot);
    if (result != Result::ok)
        error = "Original AI combat activation failed";
    return result;
}
bool find_mount(const MeshObject& object, std::string_view name) {
    if (object.name == name)
        return true;
    for (const auto& child : object.children)
        if (find_mount(child, name))
            return true;
    return false;
}
bool route_ahead(const RaceAiCombatVehicleView& first, const RaceAiCombatVehicleView& second,
                 int route_node_count) {
    const auto delta = static_cast<std::int64_t>(first.route_node) - second.route_node;
    if (delta == 0) {
        const auto distance = [](const RaceAiCombatVehicleView& v) {
            const float x = v.position.x - v.route_sample.x;
            const float z = v.position.z - v.route_sample.z;
            return std::sqrt(z * z + x * x);
        };
        return distance(first) > distance(second);
    }
    return std::abs(delta) <= route_node_count / 2 ? delta > 0 : delta < 0;
}
} // namespace

// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004831c3
Result load_original_weapon_keys(ResourceStore& resources, OriginalWeaponKeys& output,
                                 std::string& error) {
    Configuration config;
    const auto loaded = config.load(resources, "Controllers/Keyboard/Data.ini");
    if (loaded != Result::ok) {
        error = "Original keyboard weapon bindings unavailable";
        return loaded;
    }
    OriginalWeaponKeys decoded;
    const std::array<std::pair<std::string_view, int*>, 6> fields{
        {{"ACTIVATE_WEAPON", &decoded.current},
         {"ACTIVATE_FRONT_WEAPON", &decoded.front},
         {"ACTIVATE_SIDE_WEAPON", &decoded.side},
         {"ACTIVATE_REAR_WEAPON", &decoded.rear},
         {"ACTIVATE_NITROUS_BOOST", &decoded.nitrous},
         {"DISPLAY", &decoded.cycle}}};
    for (const auto& [name, destination] : fields) {
        float code{};
        if (config.number("INPUTS", name, code) != Result::ok || code < 0 || code > 255 ||
            std::floor(code) != code) {
            error = "Invalid original keyboard binding: ";
            error += name;
            return Result::invalid_argument;
        }
        *destination = static_cast<int>(code);
    }
    output = decoded;
    return Result::ok;
}

// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0040face
unsigned race_weapon_mount_count(const MeshObject& mesh) {
    unsigned count{};
    for (const auto* name :
         {"ULRORMount1", "ULRORMount2", "ULRORMount3", "RearWeaponMount", "FrontWeaponMount"})
        if (find_mount(mesh, name))
            ++count;
    return count;
}

// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00485372
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004242cb
bool race_weapon_activation_allowed(float countdown, float race_time, int locked_seconds,
                                    float disabled_134) {
    if (!std::isfinite(countdown) || !std::isfinite(race_time) || !std::isfinite(disabled_134) ||
        disabled_134 != 0)
        return false;
    std::int32_t threshold = locked_seconds;
    if (countdown != 0) {
        const double remaining = static_cast<double>(race_time) + 2.0;
        if (remaining < std::numeric_limits<std::int32_t>::min() ||
            remaining > std::numeric_limits<std::int32_t>::max())
            return false;
        threshold = static_cast<std::int32_t>(remaining);
    }
    return static_cast<float>(threshold) < race_time;
}

// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004ba95f
// Track weapon gate only; direct world effects retain separate ownership.
bool race_world_weapons_unlocked(float countdown, float race_time, int locked_seconds) {
    if (!std::isfinite(countdown) || !std::isfinite(race_time))
        return false;
    double threshold = locked_seconds;
    if (countdown != 0) {
        const double value = static_cast<double>(race_time) + 2;
        if (value < std::numeric_limits<std::int32_t>::min() ||
            value > std::numeric_limits<std::int32_t>::max())
            return false;
        threshold = static_cast<std::int32_t>(value);
    }
    return static_cast<float>(threshold) <= race_time;
}

// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0048355e
void reset_race_ai_side_delay(RaceAiCombatState& state, std::uint32_t random_value) {
    state.side_delay_4ad4 = static_cast<float>(random_value % 30) + 15;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0048355e
void reset_race_ai_rear_timer(RaceAiCombatState& state, std::uint32_t random_value) {
    state.rear_timer_0c = static_cast<float>(random_value % 1500) / 1000.0f;
}

// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00424d3e
Result dispatch_race_weapon_input(const RaceWeaponInput& input, RaceWeaponInputState& state,
                                  std::array<bool, 3> present, int selected,
                                  const RaceWeaponInputServices& services, std::string& error) {
    const bool cycle_edge = input.cycle_pressed || (input.cycle_down && !state.previous_down[5]);
    state.previous_down[5] = input.cycle_down;
    if (cycle_edge) {
        if (!services.cycle)
            return unavailable(error, "00489bf8 HUD item cycle");
        const auto result = services.cycle(selected);
        if (result != Result::ok) {
            error = "Original HUD item cycle failed";
            return result;
        }
    }
    const std::array<bool, 5> down{input.current_down, input.weapon_down[0], input.weapon_down[1],
                                   input.weapon_down[2], input.nitrous_down};
    const std::array<bool, 5> pressed{input.current_pressed, input.weapon_pressed[0],
                                      input.weapon_pressed[1], input.weapon_pressed[2],
                                      input.nitrous_pressed};
    const auto activate_slot = [&](std::size_t slot) {
        if (!services.activate)
            return unavailable(error, "player 004242cb activation");
        return services.activate(slot);
    };
    const auto activate_boost = [&] {
        if (!services.nitrous)
            return unavailable(error, "004246c0 nitrous activation");
        return services.nitrous();
    };
    for (std::size_t action = 0; action < down.size(); ++action) {
        const bool edge = pressed[action] || (down[action] && !state.previous_down[action]);
        state.previous_down[action] = down[action];
        if (!edge)
            continue;
        Result result = Result::ok;
        if (action == 0) {
            if (selected == 2)
                result = activate_boost();
            else if (selected >= 3 && selected <= 5)
                result = activate_slot(static_cast<std::size_t>(selected - 3));
        } else if (action == 4) {
            result = activate_boost();
        } else if (present[action - 1]) {
            result = activate_slot(action - 1);
        }
        if (result != Result::ok) {
            if (error.empty())
                error = "Original player weapon command unavailable";
            return result;
        }
    }
    return Result::ok;
}

// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00415bc8
bool race_missile_owner_wrong_way(Vec3 forward, Vec3 position, Vec3 next_main_center) {
    return dot(forward, {position.x - next_main_center.x, position.y - next_main_center.y,
                         position.z - next_main_center.z}) > 0;
}

// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004511a0
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0044f950
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0044fbf1
Result select_race_missile_target(std::span<const RaceAiCombatVehicleView> vehicles,
                                  std::size_t owner, int count, bool back_missile, bool wrong_way,
                                  int& target, std::string& error) {
    target = -1;
    if (owner >= vehicles.size() || count <= 0 ||
        vehicles.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        error = "Invalid original missile target-selection frame";
        return Result::invalid_argument;
    }
    for (const auto& v : vehicles)
        if (v.route_node < 0 || v.route_node >= count) {
            error = "Invalid original missile candidate route node";
            return Result::invalid_argument;
        }
    const bool prefer_behind = back_missile != wrong_way;
    for (std::size_t i = 0; i < vehicles.size(); ++i) {
        if (i == owner || route_ahead(vehicles[owner], vehicles[i], count) != prefer_behind)
            continue;
        if (target < 0 || route_ahead(vehicles[target], vehicles[i], count) != prefer_behind)
            target = static_cast<int>(i);
    }
    return Result::ok;
}

// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0041a7e7
Vec3 race_ai_rear_reference(Vec3 wheel_0, Vec3 wheel_2, Vec3 position) {
    return {wheel_0.x - wheel_2.x + position.x, wheel_0.y - wheel_2.y + position.y,
            wheel_0.z - wheel_2.z + position.z};
}

// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0047c1b4
float race_ai_combat_angle(Vec3 first, Vec3 second) {
    const float first_length = std::sqrt(dot(first, first));
    const float second_length = std::sqrt(dot(second, second));
    const float cosine = std::clamp(static_cast<float>(static_cast<double>(dot(first, second)) /
                                                       (static_cast<double>(first_length) *
                                                        static_cast<double>(second_length))),
                                    -1.0f, 1.0f);
    const float radians = static_cast<float>(std::acos(static_cast<double>(cosine)));
    return radians * 57.288352966308594f; // 005f5250, including original float rounding.
}

// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0041a1cd
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0041a69a
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0041a7e7
Result advance_race_ai_combat(const RaceAiCombatFrame& frame, RaceAiCombatState& state,
                              const RaceAiCombatServices& services, float dt,
                              RaceAiCombatTrace& trace, std::string& error) {
    trace = {};
    if (!std::isfinite(dt) || dt < 0 || !std::isfinite(state.side_delay_4ad4) ||
        !std::isfinite(state.rear_timer_0c) || frame.self_index >= frame.vehicles.size()) {
        error = "Invalid original AI combat frame";
        return Result::invalid_argument;
    }
    if (!frame.enabled_4de0)
        return Result::ok;
    const auto& self = frame.vehicles[frame.self_index];
    RaceAiCombatWeaponView weapon;
    Result result;
    if (frame.direct_fire_4aec) {
        trace.side_branch = "Direct";
        if ((result = read_weapon(services, 0, weapon, error)) != Result::ok)
            return result;
        if (weapon.present && (result = activate(services, 0, trace, error)) != Result::ok)
            return result;
        if ((result = read_weapon(services, 1, weapon, error)) != Result::ok)
            return result;
        if (weapon.present && !weapon.guided_kind_28 &&
            (result = activate(services, 1, trace, error)) != Result::ok)
            return result;
    } else {
        trace.side_branch = "NoGuidedSide";
        if ((result = read_weapon(services, 1, weapon, error)) != Result::ok)
            return result;
        if (weapon.present && weapon.guided_kind_28) {
            state.side_delay_4ad4 -= dt;
            trace.side_branch = "SideDelay";
            if (state.side_delay_4ad4 <= 0) {
                if (!services.refresh_side_target)
                    return unavailable(error, "side virtual slot 1c target refresh");
                if ((result = services.refresh_side_target(weapon)) != Result::ok) {
                    error = "Original AI side target refresh failed";
                    return result;
                }
                trace.side_target = weapon.target_index;
                trace.side_branch = "NoTarget";
                if (weapon.target_index >= 0) {
                    if (static_cast<std::size_t>(weapon.target_index) >= frame.vehicles.size() ||
                        frame.route_node_count <= 0 || self.route_node < 0 ||
                        self.route_node >= frame.route_node_count) {
                        error = "Invalid original AI side target or route node";
                        return Result::invalid_argument;
                    }
                    const auto& target = frame.vehicles[weapon.target_index];
                    if (target.route_node < 0 || target.route_node >= frame.route_node_count) {
                        error = "Invalid original AI target route node";
                        return Result::invalid_argument;
                    }
                    const auto difference =
                        std::abs(static_cast<std::int64_t>(self.route_node) - target.route_node);
                    trace.side_branch = "RouteRange";
                    if (difference > 0 &&
                        (difference <= 4 || difference >= frame.route_node_count - 5)) {
                        if (!services.side_wall_blocked)
                            return unavailable(error, "0041a1cd radius-3 wall ray");
                        bool blocked{};
                        if ((result = services.side_wall_blocked(blocked)) != Result::ok) {
                            error = "Original AI side wall query failed";
                            return result;
                        }
                        trace.side_branch = blocked ? "WallBlocked" : "RouteAngle";
                        if (!blocked &&
                            race_ai_combat_angle(self.route_lateral, target.route_lateral) < 35) {
                            if (!services.random)
                                return unavailable(error, "shared original RNG");
                            if ((result = activate(services, 1, trace, error)) != Result::ok)
                                return result;
                            state.side_delay_4ad4 = static_cast<float>(services.random() % 15) + 10;
                            trace.side_branch = "SideAttempt";
                        }
                    }
                }
            }
        }
    }
    // Both rear helpers share +0c. They reset before scanning, including no-target scans.
    state.rear_timer_0c += dt;
    if ((result = read_weapon(services, 2, weapon, error)) != Result::ok)
        return result;
    if (weapon.present && same_ascii(weapon.name, "Missile Jammer") && state.rear_timer_0c > 3) {
        state.rear_timer_0c = 0;
        for (const auto& other : frame.vehicles) {
            const auto& incoming = other.side_weapon;
            if (opposing(self, other) && incoming.present && incoming.guided_kind_28 &&
                incoming.target_index == static_cast<int>(frame.self_index) &&
                incoming.active_270) {
                if ((result = activate(services, 2, trace, error)) != Result::ok)
                    return result;
                break;
            }
        }
    }
    if ((result = read_weapon(services, 2, weapon, error)) != Result::ok)
        return result;
    if (weapon.present && !same_ascii(weapon.name, "Missile Jammer") &&
        state.rear_timer_0c > 1.5f) {
        state.rear_timer_0c = 0;
        for (const auto& other : frame.vehicles) {
            if (!opposing(self, other))
                continue;
            const Vec3 delta{other.position.x - self.position.x, other.position.y - self.position.y,
                             other.position.z - self.position.z};
            const float angle = race_ai_combat_angle(frame.rear_reference, delta);
            const float distance = std::sqrt(dot(delta, delta));
            if (angle > 135 && angle < 215 && distance < 30 && distance > 5) {
                if (!services.random)
                    return unavailable(error, "shared original RNG");
                if ((result = activate(services, 2, trace, error)) != Result::ok)
                    return result;
                state.rear_timer_0c -= static_cast<float>(services.random() % 10) + 5;
                break;
            }
        }
    }
    return Result::ok;
}
} // namespace roadwars
