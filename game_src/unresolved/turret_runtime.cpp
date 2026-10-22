#include "game_src/unresolved/turret_runtime.hpp"
#include <algorithm>
#include <cmath>

namespace roadwars {
namespace {
constexpr float kRadians = 0.017455555498600006f;
constexpr float kDegrees = 57.288352966308594f;
Vec3 add(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 subtract(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 scale(Vec3 a, float value) { return {a.x * value, a.y * value, a.z * value}; }
float length(Vec3 a) { return std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z); }
bool finite(Vec3 a) { return std::isfinite(a.x) && std::isfinite(a.y) && std::isfinite(a.z); }
float angle(Vec3 a, Vec3 b) {
    const float divisor = length(a) * length(b);
    if (divisor == 0)
        return 0;
    return static_cast<float>(std::acos(static_cast<double>(
               std::clamp((a.x * b.x + a.y * b.y + a.z * b.z) / divisor, -1.0f, 1.0f)))) *
           kDegrees;
}
Vec3 heading_vector(float heading) {
    const float radians = heading * kRadians;
    return {std::sin(radians), 0, std::cos(radians)};
}
bool close(Vec3 a, Vec3 b, float epsilon) {
    return b.x - epsilon < a.x && a.x < b.x + epsilon && b.y - epsilon < a.y &&
           a.y < b.y + epsilon && b.z - epsilon < a.z && a.z < b.z + epsilon;
}
float heading_towards(Vec3 from, Vec3 to, float reference) {
    from.y = to.y = 0;
    auto direction = subtract(to, from);
    const float magnitude = length(direction);
    if (magnitude == 0)
        return reference;
    direction = scale(direction, 1 / magnitude);
    const float difference = angle(heading_vector(reference), direction);
    return close(heading_vector(reference - difference), direction, .005f) ? 360 - difference
                                                                           : difference;
}
float turn(float current, float desired, float amount) {
    if (current == desired)
        return current;
    const float delta = current - desired;
    if (delta < -180 || (0 < delta && delta < 180)) {
        current = amount <= std::abs(delta) ? current - amount : desired;
        if (current < 0)
            current += 360;
    } else {
        current = amount <= std::abs(delta) ? current + amount : desired;
        if (360 <= current)
            current -= 360;
    }
    return current;
}
bool number(std::string_view name, std::size_t at, std::size_t count, float& output) {
    if (at + count > name.size())
        return false;
    unsigned value{};
    for (std::size_t i = 0; i < count; ++i) {
        if (name[at + i] < '0' || name[at + i] > '9')
            return false;
        value = value * 10 + static_cast<unsigned>(name[at + i] - '0');
    }
    output = static_cast<float>(value);
    return true;
}
} // namespace
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004c7c80
// Name decoding and scalar setup only; native mesh/weapon construction is separate.
Result parse_turret_parameters(std::string_view name, TurretParameters& output) {
    if (!name.starts_with("Turret"))
        return Result::not_found;
    TurretParameters parsed;
    if (!number(name, 7, 3, parsed.initial_yaw) || name.size() < 12)
        return Result::invalid_argument;
    std::size_t code_at{};
    if (name[11] == 'F' || name[11] == 'f') {
        parsed.unrestricted_yaw = true;
        code_at = 15;
    } else {
        if (!number(name, 11, 3, parsed.left_limit) || !number(name, 15, 3, parsed.right_limit))
            return Result::invalid_argument;
        code_at = 18;
    }
    const auto code = name.size() >= code_at + 2 ? name.substr(code_at, 2) : std::string_view{};
    if (code == "_C" || code == "_c") {
        parsed.weapon = "20mm Cannon";
        parsed.added_speed = 90;
        parsed.fire_interval = .5f;
    } else if (code == "_V" || code == "_v") {
        parsed.weapon = "Vulcanized Gun";
        parsed.added_speed = 70;
        parsed.fire_interval = .5f;
    } else if (code == "_G" || code == "_g") {
        parsed.weapon = "Gatling Gun";
        parsed.added_speed = 60;
        parsed.fire_interval = 1;
    } else if (code == "_P" || code == "_p") {
        parsed.weapon = "Plasma Blaster";
        parsed.added_speed = 110;
        parsed.fire_interval = .5f;
    } else if (code == "_M" || code == "_m") {
        parsed.weapon = "Machine Gun";
        parsed.added_speed = 40;
        parsed.fire_interval = 1;
    } else {
        parsed.weapon = "Rocket Launcher";
        parsed.fire_interval = 5;
    }
    output = std::move(parsed);
    return Result::ok;
}
Result TurretRuntime::initialize(TurretParameters parameters, Vec3 origin, float pitch) {
    if (!finite(origin) || !std::isfinite(pitch) || !std::isfinite(parameters.initial_yaw) ||
        !std::isfinite(parameters.left_limit) || !std::isfinite(parameters.right_limit) ||
        !std::isfinite(parameters.ammunition_acceleration) ||
        !std::isfinite(parameters.barrel_length) || !std::isfinite(parameters.reference_heading) ||
        !std::isfinite(parameters.base_speed) || !std::isfinite(parameters.added_speed) ||
        !std::isfinite(parameters.fire_interval) || parameters.fire_interval <= 0 ||
        !std::isfinite(parameters.maximum_range()) || parameters.maximum_range() <= 0)
        return Result::invalid_argument;
    parameters_ = std::move(parameters);
    origin_ = origin;
    initial_pitch_ = pitch;
    fire_timer_ = parameters_.fire_interval;
    samples_ = 0;
    velocity_deltas_ = {};
    previous_velocity_ = average_delta_ = {};
    initialized_ = true;
    reset();
    return Result::ok;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004c86c0
void TurretRuntime::reset() {
    scan_timer_ = 0;
    target_.reset();
    yaw_ = desired_yaw_ = parameters_.initial_yaw;
    desired_pitch_ = pitch_ = initial_pitch_;
    // 004c86c0 does not reset the firing timer or prediction history.
}
const TurretTarget* TurretRuntime::find_target(std::span<const TurretTarget> targets) const {
    if (!target_)
        return nullptr;
    for (const auto& value : targets)
        if (value.id == *target_)
            return &value;
    return nullptr;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004c8fde
void TurretRuntime::select_target(std::span<const TurretTarget> targets) {
    const auto previous = target_;
    target_.reset();
    float nearest = parameters_.maximum_range();
    for (const auto& value : targets) {
        if (value.inactive_timer != 0 || value.invulnerable || value.not_a_turret_target)
            continue;
        const float distance = length(subtract(value.position, origin_));
        if (distance < nearest) {
            nearest = distance;
            target_ = value.id;
        }
    }
    if (target_ && target_ != previous) {
        samples_ = 0;
        average_delta_ = {};
        previous_velocity_ = find_target(targets)->velocity;
    }
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004c914e
Vec3 TurretRuntime::predict(const TurretTarget& target) {
    if (samples_ == velocity_deltas_.size())
        std::move(velocity_deltas_.begin() + 1, velocity_deltas_.end(), velocity_deltas_.begin());
    else
        ++samples_;
    velocity_deltas_[samples_ - 1] = subtract(target.velocity, previous_velocity_);
    average_delta_ = {};
    for (std::size_t i = 0; i < samples_; ++i)
        average_delta_ = add(average_delta_, velocity_deltas_[i]);
    average_delta_ = scale(average_delta_, 1 / static_cast<float>(samples_));
    previous_velocity_ = target.velocity;
    float tolerance = .03f;
    if (target.computer_controlled)
        tolerance = .25f;
    else if (target.difficulty == 0)
        tolerance = .25f;
    else if (target.difficulty == 1)
        tolerance = .15f;
    else if (target.difficulty == 2)
        tolerance = .09f;
    float lower = 0, upper = 1.5f, time = 1.5f;
    while (tolerance < upper - lower) {
        const auto future =
            add(target.position, scale(add(target.velocity, scale(average_delta_, time)), time));
        const float distance = length(subtract(future, origin_));
        const float speed = time * parameters_.ammunition_acceleration + parameters_.base_speed +
                            parameters_.added_speed;
        if (0 < distance - speed * time) {
            lower = time;
            time = (upper - time) * .5f + time;
        } else {
            upper = time;
            time = (time - lower) * .5f + lower;
        }
    }
    // The original final lead adds average_delta * t, not the t^2 used in its search.
    return upper < 1.5f ? add(target.position,
                              add(scale(target.velocity, upper), scale(average_delta_, upper)))
                        : origin_;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004c959f
bool TurretRuntime::aligned() const {
    return yaw_ == desired_yaw_ && pitch_ == desired_pitch_ && samples_ == 5;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004c8729
// Recovered target/prediction/rotation dispatch; native render cache writes are not ported.
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004c8e7b
// Emits the original fire call's arguments; the caller must execute the weapon implementation.
Result TurretRuntime::advance(float dt, std::span<const TurretTarget> targets, bool enabled,
                              std::optional<TurretFireEvent>& fired) {
    fired.reset();
    if (!initialized_)
        return Result::unavailable;
    if (!std::isfinite(dt) || dt < 0 || targets.size() > 6)
        return Result::invalid_argument;
    for (std::size_t i = 0; i < targets.size(); ++i) {
        if (!finite(targets[i].position) || !finite(targets[i].velocity) ||
            !std::isfinite(targets[i].inactive_timer))
            return Result::invalid_argument;
        for (std::size_t j = 0; j < i; ++j)
            if (targets[i].id == targets[j].id)
                return Result::invalid_argument;
    }
    scan_timer_ += dt;
    if (!enabled)
        target_.reset();
    else if (3 < scan_timer_) {
        scan_timer_ = 0;
        select_target(targets);
    }
    const auto* selected = find_target(targets);
    if (!selected)
        target_.reset();
    predicted_ = selected ? predict(*selected) : origin_;
    float yaw = parameters_.initial_yaw, pitch = 0;
    if (!close(predicted_, origin_, .05f)) {
        yaw = heading_towards(origin_, predicted_, parameters_.reference_heading);
        auto vector = subtract(origin_, predicted_);
        const Vec3 horizontal{vector.x, 0, vector.z};
        pitch = angle(vector, horizontal);
        if (predicted_.y < origin_.y)
            pitch = -pitch;
        if (pitch < 0)
            pitch += 360;
    }
    desired_yaw_ = yaw;
    desired_pitch_ = pitch;
    if (!parameters_.unrestricted_yaw) {
        const float delta = parameters_.initial_yaw - yaw;
        const float distance = angle(heading_vector(parameters_.initial_yaw), heading_vector(yaw));
        if (delta < -180 || (0 < delta && delta < 180)) {
            if (parameters_.left_limit < distance) {
                yaw = parameters_.initial_yaw - parameters_.left_limit;
                if (yaw < 0)
                    yaw += 360;
            }
        } else if (parameters_.right_limit < distance) {
            yaw = parameters_.initial_yaw + parameters_.right_limit;
            if (360 <= yaw)
                yaw -= 360;
        }
    }
    yaw_ = turn(yaw_, yaw, 180 * dt);
    if (pitch_ != pitch) {
        pitch_ = turn(pitch_, pitch, 180 * dt);
        if (20 < pitch_ && pitch_ < 180)
            pitch_ = 20;
        else if (180 < pitch_ && pitch_ < 340)
            pitch_ = 340;
    }
    if (enabled) {
        fire_timer_ += dt;
        if (selected && !close(predicted_, origin_, .05f) && aligned() &&
            parameters_.fire_interval <= fire_timer_) {
            fire_timer_ = 0;
            // 0064a1b0 is -X: positive pitch raises the barrel before its Y rotation.
            const float p = pitch_ * kRadians, h = yaw_ * kRadians;
            const Vec3 direction{std::sin(h) * std::cos(p), std::sin(p), std::cos(h) * std::cos(p)};
            fired = TurretFireEvent{
                *target_,   add(origin_, scale(direction, parameters_.barrel_length)),
                predicted_, yaw_,
                pitch_,     parameters_.base_speed};
        }
    }
    return Result::ok;
}
} // namespace roadwars
