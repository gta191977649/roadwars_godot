#include "game_src/unresolved/guided_weapon_runtime.hpp"
#include <algorithm>
#include <cmath>

namespace roadwars {
namespace {
constexpr float kRadians = .017455555498600006f;
constexpr float kDegrees = 57.288352966308594f;
Vec3 add(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 sub(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 scale(Vec3 v, float s) { return {v.x * s, v.y * s, v.z * s}; }
bool finite(Vec3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
float heading_of(Vec3 v) {
    const float length = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    float cosine = v.z / length;
    if (cosine <= 1) {
        if (cosine < -1)
            cosine = -1;
    } else
        cosine = 1;
    float heading = static_cast<float>(std::acos(static_cast<double>(cosine)) * kDegrees);
    if (v.x < 0) {
        heading = 360 - heading;
        if (heading == 360)
            heading = 0;
    }
    return heading;
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
// Same recovered 004dd310 XZ helper used by RaceRoute; keep its endpoint/collinear rules.
bool intersect(Vec3 a, Vec3 b, Vec3 c, Vec3 d) {
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
Result GuidedWeaponRuntime::initialize(std::string_view name, WeaponParameters parameters,
                                       GuidedWeaponShape shape, std::string& error) {
    const auto* identity = find_weapon_descriptor(name);
    if (!identity || !weapon_is_guided(identity->name)) {
        error = "Unsupported original guided weapon identity";
        return Result::unavailable;
    }
    if (!std::isfinite(parameters.ammunition_count) || parameters.ammunition_count < 1 ||
        parameters.ammunition_count > 4 ||
        std::floor(parameters.ammunition_count) != parameters.ammunition_count ||
        !std::isfinite(parameters.acceleration) || !std::isfinite(parameters.top_speed) ||
        !std::isfinite(parameters.rotational_speed) ||
        !std::isfinite(parameters.tracking_seconds) || parameters.tracking_seconds < 0 ||
        !std::isfinite(shape.width) || !std::isfinite(shape.height) || shape.width < 0 ||
        shape.height < 0 ||
        (parameters.ammunition_count > 1 && (shape.width == 0 || shape.height == 0))) {
        error = "Invalid original guided weapon parameters";
        return Result::invalid_argument;
    }
    kind_ = identity->name == "Back Missile"    ? Kind::back
            : identity->name == "Swarmers"      ? Kind::swarmers
            : identity->name == "The Excalibur" ? Kind::excalibur
                                                : Kind::guided;
    parameters_ = parameters;
    shape_ = shape;
    missiles_.assign(static_cast<std::size_t>(parameters.ammunition_count), {});
    initialized_ = ready_ = true;
    active_ = update_pending_ = false;
    age_ = tracking_timer_ = 0;
    route_node_ = -1;
    tracking_target_.reset();
    return Result::ok;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0044f365
// Recovered launch/state/formation; native audio, flare/cache and target ownership are separate.
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00434bd7
// Excalibur's predecessor target is supplied by the actual race ranking owner.
Result GuidedWeaponRuntime::launch(const WeaponFireRequest& request, bool& launched,
                                   std::string& error, int owner_node, bool owner_branch) {
    launched = false;
    if (!initialized_)
        return Result::unavailable;
    if (!finite(request.origin) || !std::isfinite(request.heading) ||
        !std::isfinite(request.pitch) || !std::isfinite(request.speed) || request.speed < 0 ||
        (kind_ == Kind::excalibur && owner_node < 0)) {
        error = "Invalid original guided launch state";
        return Result::invalid_argument;
    }
    if (active_ || !ready_)
        return Result::ok;
    const float heading = request.heading + (kind_ == Kind::back ? 170 : 10);
    const float pitch = request.pitch * kRadians, yaw = heading * kRadians;
    initial_direction_ = {std::sin(yaw) * std::cos(pitch), std::sin(pitch),
                          std::cos(yaw) * std::cos(pitch)};
    vertical_speed_ = request.speed * std::sin(pitch);
    horizontal_speed_ = request.speed - vertical_speed_;
    const float side = (request.heading + 90) * kRadians;
    const Vec3 sideways{std::sin(side), 0, std::cos(side)};
    const Vec3 up{0, std::cos(pitch), -std::sin(pitch)};
    for (std::size_t i = 0; i < missiles_.size(); ++i) {
        auto& missile = missiles_[i];
        const auto flare_roll = missile.flare_roll;
        missile = {};
        missile.flare_roll = flare_roll;
        missile.position = request.origin;
        if (missiles_.size() > 1) {
            missile.position =
                add(missile.position, scale(sideways, shape_.width * .75f * (i % 2 ? 1 : -1)));
            if (1 < i)
                missile.position = add(missile.position, scale(up, -shape_.height));
        }
        missile.previous_position = missile.position;
        missile.heading = heading;
        if (360 <= missile.heading)
            missile.heading -= 360;
        if (missile.heading < 0)
            missile.heading += 360;
        missile.visible = true;
    }
    age_ = 0;
    tracking_timer_ = parameters_.tracking_seconds;
    route_node_ = owner_node;
    tracking_target_.reset();
    branch_ = owner_branch;
    active_ = launched = true;
    ready_ = false;
    return Result::ok;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00434d5f
// Track geometry/ranking pointers are borrowed as typed route and target views.
Result GuidedWeaponRuntime::excalibur_target(const GuidedWeaponTarget& target,
                                             const RaceRoute& route, Vec3& output) {
    if (route.nodes().empty() || route_node_ < 0 || target.route_node < 0 ||
        static_cast<std::size_t>(route_node_) >= route.nodes().size() ||
        static_cast<std::size_t>(target.route_node) >= route.nodes().size())
        return Result::invalid_argument;
    const auto& missile = missiles_[0];
    const auto crossing = [&](int node, bool& branch_hit) {
        const auto& line = route.nodes()[static_cast<std::size_t>(node)];
        const bool main =
            intersect(missile.previous_position, missile.position, line.main.left, line.main.right);
        branch_hit = line.branch && intersect(missile.previous_position, missile.position,
                                              line.branch->left, line.branch->right);
        return main || branch_hit;
    };
    bool branch_hit{};
    const int next = route.wrap(route_node_ + 1);
    if (crossing(next, branch_hit)) {
        route_node_ = next;
        branch_ = branch_hit;
    } else {
        const int previous = route.wrap(route_node_ - 1);
        if (crossing(previous, branch_hit)) {
            route_node_ = previous;
            branch_ = branch_hit;
        }
    }
    int ahead = target.route_node - route_node_;
    if (ahead < 0)
        ahead += static_cast<int>(route.nodes().size());
    output = ahead < 2 ? target.position : route.respawn(route.wrap(route_node_ + 1), 0, branch_);
    return Result::ok;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0044fc04
// Motion/guidance phase only; caller must run the original collision/explosion phase afterward.
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004350b0
Result GuidedWeaponRuntime::begin_update(float dt, std::optional<GuidedWeaponTarget> target,
                                         const RaceRoute* route,
                                         const GuidedWeaponServices& services, std::string& error) {
    if (!initialized_)
        return Result::unavailable;
    if (update_pending_ || !std::isfinite(dt) || dt < 0 || (target && !finite(target->position))) {
        error = "Invalid original guided update state";
        return Result::invalid_argument;
    }
    std::optional<Vec3> aim;
    if (target) {
        aim = target->position;
        if (kind_ == Kind::excalibur) {
            if (!route) {
                error = "Original Excalibur route guidance is unavailable";
                return Result::unavailable;
            }
            const auto result = excalibur_target(*target, *route, *aim);
            if (result != Result::ok) {
                error = "Invalid original Excalibur target route";
                return result;
            }
        }
    }
    const float launch_phase = kind_ == Kind::back ? .1f : .5f;
    tracking_target_ = aim;
    const float next_tracking = tracking_timer_ + dt;
    const bool tracking =
        aim && launch_phase < age_ &&
        parameters_.tracking_seconds / parameters_.ammunition_count + parameters_.tracking_seconds <
            next_tracking;
    if ((tracking && !services.random) || (launch_phase < age_ && !services.ground)) {
        error = "Original guided weapon random or ground service is unavailable";
        return Result::unavailable;
    }
    const float vertical_acceleration = parameters_.acceleration * initial_direction_.y;
    if (horizontal_speed_ < parameters_.top_speed) {
        horizontal_speed_ += (parameters_.acceleration - std::abs(vertical_acceleration)) * dt;
        vertical_speed_ += vertical_acceleration * dt;
    }
    tracking_timer_ = next_tracking;
    if (tracking) {
        tracking_timer_ = 0;
        for (auto& missile : missiles_) {
            if (missile.jam_remaining != 0) {
                missile.jam_remaining -= dt;
                if (missile.jam_remaining < 0)
                    missile.jam_remaining = 0;
            }
            if (!missile.impacted && missile.jam_remaining == 0) {
                int noise = missiles_.size() > 1 ? static_cast<int>(services.random() % 8) : 0;
                if (services.random() % 2)
                    noise = -noise;
                const Vec3 noisy_target{aim->x + noise, aim->y + noise, aim->z + noise};
                float heading = heading_of(sub(noisy_target, missile.position));
                if (360 <= heading)
                    heading -= 360;
                missile.heading = turn(missile.heading, heading, dt * parameters_.rotational_speed);
            }
        }
    }
    for (auto& missile : missiles_) {
        missile.previous_position = missile.position;
        if (missile.impacted)
            continue;
        missile.flare_roll += 360 * dt;
        const float yaw = missile.heading * kRadians;
        missile.velocity = {std::sin(yaw) * horizontal_speed_, 0,
                            std::cos(yaw) * horizontal_speed_};
        missile.position.x += dt * missile.velocity.x;
        missile.position.z += dt * missile.velocity.z;
        if (age_ <= launch_phase)
            missile.position.y += dt * vertical_speed_;
        else {
            float ground{};
            auto query = missile.position;
            query.y += 5;
            const auto result = services.ground(query, ground);
            if (result != Result::ok || !std::isfinite(ground)) {
                error = "Original guided weapon ground query failed";
                return result == Result::ok ? Result::invalid_argument : result;
            }
            missile.position.y -= 98 * dt;
            if (missile.position.y < ground + .5f)
                missile.position.y = ground + .5f;
        }
    }
    // 0044fc04 performs motion/guidance/trails before checking active+270.
    // A cancelled/expired object's last registered callback still updates caches.
    if (active_)
        age_ += dt;
    update_pending_ = true;
    return Result::ok;
}
Result GuidedWeaponRuntime::record_impact(std::size_t index) {
    if (!update_pending_ || index >= missiles_.size())
        return Result::invalid_argument;
    missiles_[index].impacted = true;
    missiles_[index].jam_remaining = 0;
    return Result::ok;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0044f863
// Native trail owner supplies the live-particle count used by the original completion predicate.
Result GuidedWeaponRuntime::finish_update(std::span<const std::size_t> particles) {
    if (!update_pending_ || particles.size() != missiles_.size())
        return Result::invalid_argument;
    bool finished = true;
    for (std::size_t i = 0; i < missiles_.size(); ++i)
        finished = finished && missiles_[i].impacted && particles[i] == 0;
    if (!active_) {
        for (auto& missile : missiles_)
            missile.visible = false;
        ready_ = true;
        age_ = 0;
    } else if (finished || (kind_ == Kind::excalibur ? 20 : 5) < age_)
        active_ = false;
    update_pending_ = false;
    return Result::ok;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0044f81d
Result GuidedWeaponRuntime::jam(float seconds) {
    if (!initialized_ || !std::isfinite(seconds))
        return Result::invalid_argument;
    for (auto& missile : missiles_)
        missile.jam_remaining = seconds;
    return Result::ok;
}
} // namespace roadwars
