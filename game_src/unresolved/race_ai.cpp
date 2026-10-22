#include "game_src/unresolved/race_ai.hpp"
#include <algorithm>
#include <cmath>

namespace roadwars {
namespace {
constexpr float kRadians = 0.017455555498600006f;
constexpr float kDegrees = 57.288352966308594f;
constexpr float kGravity = 9.800000190734863f;
constexpr float kMph = 0.4470277726650238f;
constexpr std::array<float, 8> kModeSpeeds{2, 0, -2, -4, 2, 2, .5f, 0};
Vec3 add(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 sub(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 scale(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
float length(Vec3 a) { return std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z); }
// 0047c820 deliberately ignores Y; 0047c742 is the separate 3D helper.
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0047c820
float distance(Vec3 a, Vec3 b) {
    const float x = a.x - b.x, z = a.z - b.z;
    return std::sqrt(z * z + x * x);
}
float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3 direction(float heading) {
    const auto radians = heading * kRadians;
    return {std::sin(radians), 0, std::cos(radians)};
}
float normalize_angle(float value) {
    if (value > 360)
        value -= 360;
    else if (value < 0)
        value += 360;
    return value;
}
float heading_of(Vec3 value) {
    const float magnitude = length(value);
    if (magnitude == 0)
        return 0;
    float angle = static_cast<float>(
        std::acos(static_cast<double>(std::clamp(value.z / magnitude, -1.0f, 1.0f))) * kDegrees);
    if (value.x < 0)
        angle = 360 - angle;
    return angle == 360 ? 0 : angle;
}
bool contains(const std::array<Vec3, 3>& triangle, Vec3 p) {
    unsigned result{}, previous = 2;
    for (unsigned index = 0; index < 3; ++index) {
        const auto& current = triangle[index];
        const auto& last = triangle[previous];
        if (((current.z <= p.z && p.z < last.z) &&
             (p.x - current.x) * (last.z - current.z) < (p.z - current.z) * (last.x - current.x)) ||
            ((last.z <= p.z && p.z < current.z) &&
             (p.z - current.z) * (last.x - current.x) < (p.x - current.x) * (last.z - current.z)))
            result ^= 1;
        previous = index;
    }
    return result != 0;
}
bool intersects(const std::array<Vec3, 3>& triangle, const RaceAiObstacle& obstacle) {
    // 00460245 checks footprint vertices, followed by a center check. It is
    // intentionally not a general polygon intersection predicate.
    for (const auto p : obstacle.footprint)
        if (contains(triangle, p))
            return true;
    return contains(triangle, obstacle.position);
}
float shortest_lerp(float current, float destination, float amount) {
    // 004603f3 normalizes each argument once and follows the shorter arc.
    current = normalize_angle(current);
    destination = normalize_angle(destination);
    const float original_difference = destination - current;
    if (std::abs(original_difference) <= 180)
        current += original_difference * amount;
    else if (destination <= current)
        current += ((360 - current) + destination) * amount;
    else
        current -= ((360 - destination) + current) * amount;
    if ((original_difference < 0 && destination - current > 0) ||
        (original_difference > 0 && destination - current < 0))
        current = destination;
    if (current > 180)
        current -= 360;
    return current;
}
bool finite(Vec3 a) { return std::isfinite(a.x) && std::isfinite(a.y) && std::isfinite(a.z); }
} // namespace

// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0041bcb7
float race_ai_heading_delta(float heading, Vec3 from, Vec3 to) {
    const float destination = normalize_angle(heading_of(sub(to, from)));
    heading = normalize_angle(heading);
    if (std::abs(destination - heading) <= 180)
        return destination - heading;
    if (destination <= heading)
        return (360 - heading) + destination;
    return -((360 - destination) + heading);
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0042bd5c
float race_ai_corner_speed(float distance, float degrees, float grip) {
    if (degrees == 0)
        return 10000;
    return static_cast<float>(
        std::sqrt(static_cast<double>(grip * kGravity * (distance / (degrees * kRadians)))));
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0042bdc7
float race_ai_braking_speed(float distance, float terminal, float grip) {
    // Preserve the original minus sign and terminal-plus-root expression.
    const float root = static_cast<float>(
        std::sqrt(static_cast<double>(terminal * terminal - 2.0f * grip * kGravity * distance)));
    return root >= 0 ? terminal + root : terminal;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00418f73
int race_ai_behavior(float health, float maximum, bool front, bool rear) {
    if (health < maximum && rear)
        return 3;
    if (maximum - health >= 25)
        return 2;
    if (rear)
        return front ? 5 : 4;
    return front ? 6 : 1;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00419597
void RaceAi::scan(const VehicleState& vehicle, std::span<const RaceAiObstacle> obstacles) {
    state_.front = state_.rear = state_.left = state_.right = false;
    state_.front_strength = state_.rear_strength = state_.left_strength = state_.right_strength = 0;
    state_.front_speed = 0;
    constexpr std::array<std::array<float, 3>, 4> sectors{
        {{176, 184, 20}, {-4, 4, 20}, {-140, -60, 6}, {60, 140, 6}}};
    for (const auto& obstacle : obstacles) {
        if (!obstacle.active)
            continue;
        for (std::size_t sector = 0; sector < sectors.size(); ++sector) {
            const auto& parameters = sectors[sector];
            const std::array<Vec3, 3> triangle{
                vehicle.position,
                add(vehicle.position,
                    scale(direction(vehicle.heading_degrees + parameters[0]), parameters[2])),
                add(vehicle.position,
                    scale(direction(vehicle.heading_degrees + parameters[1]), parameters[2]))};
            if (!intersects(triangle, obstacle))
                continue;
            const float strength =
                std::max(0.0f, 1.0f - distance(vehicle.position, obstacle.position) / 20);
            if (sector == 0 && state_.rear_strength < strength) {
                state_.rear = true;
                state_.rear_strength = strength;
            } else if (sector == 1 && state_.front_strength < strength) {
                state_.front = true;
                state_.front_strength = strength;
                state_.front_speed = length(obstacle.velocity);
            } else if (sector == 2 && state_.left_strength < strength) {
                state_.left = true;
                state_.left_strength = strength;
            } else if (sector == 3 && state_.right_strength < strength) {
                state_.right = true;
                state_.right_strength = strength;
            }
            break; // Original priority is rear, front, left, right.
        }
    }
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0041906a
void RaceAi::advance_offset(const RaceRoute& route, const RaceRouteProgress& progress, float dt) {
    const auto& next = route.point(progress.current_node + 1);
    const float current = state_.lateral_offset + next.optimal_offset;
    const float right = next.right_bound - current;
    const float left = current - next.left_bound;
    if (state_.offset_cooldown > 0)
        state_.offset_cooldown -= dt;
    if (!state_.left && !state_.right && !state_.rear && !state_.front &&
        state_.desired_offset != 0 && state_.offset_cooldown <= 0) {
        state_.offset_cooldown = 1;
        state_.desired_offset += state_.desired_offset < 0 ? 1 : -1;
        if (std::abs(state_.desired_offset) < 1)
            state_.desired_offset = 0;
    }
    if (!state_.left && state_.right && state_.offset_cooldown <= 0) {
        state_.offset_cooldown = 4;
        state_.desired_offset += 3;
    }
    if (state_.left && !state_.right && state_.offset_cooldown <= 0) {
        state_.offset_cooldown = 4;
        state_.desired_offset -= 3;
    }
    if ((state_.behavior == 5 || state_.behavior == 6) && state_.offset_cooldown <= 0) {
        state_.offset_cooldown = 3;
        state_.desired_offset += right <= left ? -2 : 2;
    }
    if ((state_.behavior == 3 || state_.behavior == 4) && state_.offset_cooldown <= 0) {
        state_.offset_cooldown = 5;
        state_.desired_offset =
            std::clamp(state_.desired_offset + (right <= left ? 3.5f : -3.5f), -5.0f, 5.0f);
    }
    if (progress.finished) {
        state_.desired_offset = 100;
        state_.offset_cooldown = 1;
    }
    const float step = (progress.finished ? 100.0f : 2.0f) * dt;
    if (state_.desired_offset < state_.lateral_offset)
        state_.lateral_offset = std::max(state_.desired_offset, state_.lateral_offset - step);
    else if (state_.desired_offset > state_.lateral_offset)
        state_.lateral_offset = std::min(state_.desired_offset, state_.lateral_offset + step);
}

// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0041aa67
Result RaceAi::update(const RaceRoute& route, RaceRouteProgress& progress,
                      const VehicleState& vehicle, const VehicleParameters& parameters,
                      std::span<const RaceAiObstacle> obstacles, float grip, float race_time,
                      bool refresh, float dt, std::string& error) {
    if (route.nodes().size() < 4 || !finite(vehicle.position) || !finite(vehicle.velocity) ||
        !std::isfinite(dt) || dt < 0 || !std::isfinite(grip) || grip < 0 ||
        !std::isfinite(race_time) || !std::isfinite(parameters.max_wheel_angle_degrees) ||
        parameters.max_wheel_angle_degrees <= 0) {
        error = "Invalid original AI input";
        return Result::invalid_argument;
    }
    output_ = {};
    output_.steering_degrees = vehicle.steering_degrees;
    const float speed = vehicle.cached_speed;
    if (race_time < .25f) {
        output_.engine_throttle = progress.finished ? 0.0f : 1.0f;
        output_.branch = progress.finished ? "Finished" : "Initial acceleration";
        error.clear();
        return Result::ok;
    }
    if (refresh) {
        scan(vehicle, obstacles);
        state_.behavior = race_ai_behavior(1, 1, state_.front, state_.rear);
        state_.moving_backward = dot(direction(vehicle.heading_degrees), vehicle.velocity) <= 0;
    }
    advance_offset(route, progress, dt);
    const int base = route.wrap(progress.current_node + 1);
    bool branch_latch = false;
    std::array<Vec3, 6> points{};
    // 004182ee is called in -1,0,1,2,3 order, followed by -2 during recovery.
    for (int relative = -1; relative <= 3; ++relative)
        points[static_cast<std::size_t>(relative + 2)] =
            route.sample(base, relative, progress.on_branch, progress.branch_recent,
                         state_.behavior, state_.lateral_offset, branch_latch);
    const float previous_distance = distance(points[1], points[2]);
    if (previous_distance <= 0) {
        error = "Original AI route contains coincident lookahead samples";
        return Result::invalid_argument;
    }
    const float fraction = 1 - distance(vehicle.position, points[2]) / previous_distance;
    Vec3 target = points[2];
    if (previous_distance <= 6) {
        target.x += (points[4].x - points[3].x) * fraction + (points[3].x - points[2].x);
        target.z += (points[4].z - points[3].z) * fraction + (points[3].z - points[2].z);
    } else {
        target.x += (points[3].x - points[2].x) * fraction;
        target.z += (points[3].z - points[2].z) * fraction;
    }
    target.y = vehicle.position.y;
    output_.target = target;
    output_.target_node = base;
    Vec3 flat_position = vehicle.position;
    flat_position.y = 0;
    Vec3 flat_target = target;
    flat_target.y = 0;
    float steering = race_ai_heading_delta(vehicle.heading_degrees, flat_position, flat_target);
    float target_speed = 10000;
    const float mode_speed =
        kModeSpeeds[static_cast<std::size_t>(std::clamp(state_.behavior, 0, 7))] * kMph;
    const auto corner = [&](Vec3 a, Vec3 b, float heading, float& delta) {
        delta = race_ai_heading_delta(heading, a, b);
        return std::abs(delta) < 3
                   ? 10000.0f
                   : race_ai_corner_speed(distance(a, b), std::abs(delta), grip) + mode_speed;
    };
    float first{}, second{}, third{}, fourth{};
    target_speed = corner(flat_position, flat_target, vehicle.heading_degrees, first);
    corner(flat_target, points[3], vehicle.heading_degrees + first, second);
    corner(points[3], points[4], vehicle.heading_degrees + first + second, third);
    const float terminal =
        corner(points[4], points[5], vehicle.heading_degrees + first + second + third, fourth);
    const float approach = race_ai_braking_speed(distance(vehicle.position, points[3]) +
                                                     distance(points[3], points[5]),
                                                 terminal, grip) +
                           mode_speed;
    target_speed = std::min(target_speed, approach);
    const float hint = route.point(progress.current_node, progress.on_branch).speed_hint;
    if (hint > 0)
        target_speed = std::min(target_speed, hint);
    else if (hint < 0)
        target_speed = std::max(target_speed, std::abs(hint));
    output_.target_speed = target_speed;

    if (state_.recovery_timer >= 1) {
        if (state_.recovery_timer < 10)
            state_.recovery_timer = 101.8f;
        else if (state_.recovery_timer < 100) {
            state_.recovery_timer = 0;
            state_.recovery_accumulation += 7.5f;
        }
        state_.recovery_timer -= dt;
        const auto backward = route.sample(base, -2, progress.on_branch, progress.branch_recent,
                                           state_.behavior, state_.lateral_offset, branch_latch);
        const auto backward_to_vehicle = sub(vehicle.position, backward);
        auto signed_angle = [](Vec3 direction) {
            float result = static_cast<float>(
                std::acos(static_cast<double>(
                    length(direction) ? std::clamp(direction.z / length(direction), -1.0f, 1.0f)
                                      : 1.0f)) *
                kDegrees);
            return direction.x < 0 ? -result : result;
        };
        steering =
            (signed_angle(direction(vehicle.heading_degrees)) - signed_angle(backward_to_vehicle)) /
            2;
        if (state_.recovery_timer <= 100.6f)
            output_.brake_amount = .99f;
        else
            output_.engine_throttle = -1;
        output_.branch = "Wall recovery";
    } else {
        if (target_speed > speed)
            output_.engine_throttle = target_speed - speed < 3 ? 1 - speed / target_speed : 1;
        else if (target_speed < speed)
            output_.brake_amount = speed - target_speed < 2 ? .99f - target_speed / speed : .99f;
        if (state_.recovery_accumulation > 0) {
            state_.recovery_accumulation = std::max(0.0f, state_.recovery_accumulation - dt);
            if (state_.recovery_accumulation >= 15) {
                output_.relocate = true;
                output_.relocation =
                    route.sample(base, 0, progress.on_branch, progress.branch_recent,
                                 state_.behavior, state_.lateral_offset, branch_latch);
                progress.current_node = route.wrap(progress.current_node + 1);
                if (!route.nodes()[static_cast<std::size_t>(progress.current_node)].branch)
                    progress.on_branch = false;
                state_.recovery_accumulation = 0;
            }
        }
    }
    if (state_.front && state_.front_strength > .7f && speed > 7.5f) {
        if (state_.front_speed < length(vehicle.velocity))
            output_.brake_amount = .99f;
    } else if (state_.front && state_.front_strength > .55f && speed > 5) {
        if (state_.front_speed < length(vehicle.velocity) && output_.engine_throttle != 0)
            output_.engine_throttle = 1;
    }
    if (speed > 2.5f && state_.moving_backward && state_.recovery_timer < 100)
        output_.brake_amount = 1;
    else if (speed < 5) {
        if (output_.engine_throttle == 0)
            output_.engine_throttle = 1;
        output_.brake_amount = 0;
    }
    steering = std::clamp(steering, -parameters.max_wheel_angle_degrees,
                          parameters.max_wheel_angle_degrees);
    output_.heading_error = steering;
    float destination = steering;
    if (std::abs(steering) >= 5)
        destination =
            steering < 0 ? -parameters.max_wheel_angle_degrees : parameters.max_wheel_angle_degrees;
    else if (std::abs(steering) >= 2)
        destination = steering * 5 + 5;
    else
        destination = steering * 3 + 2;
    output_.steering_degrees =
        shortest_lerp(vehicle.steering_degrees, destination, std::min(dt * 7, 1.0f));
    if (progress.finished) {
        output_.engine_throttle = output_.brake_amount = 0;
        output_.branch = "Finished";
    }
    error.clear();
    return Result::ok;
}
// Wall timer subset of 0042e8f0. The collider supplies actual iteration count.
void RaceAi::wall_update(unsigned iterations, float throttle, float dt) {
    if (iterations && throttle > 0) {
        state_.recovery_timer += dt * static_cast<float>(iterations);
        state_.wall_clear_time = 0;
    } else if (!iterations) {
        state_.wall_clear_time += dt;
        if (state_.wall_clear_time > 1.5f)
            state_.recovery_timer = 0;
    }
}
} // namespace roadwars
