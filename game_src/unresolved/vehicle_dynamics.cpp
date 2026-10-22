#include "game_src/unresolved/vehicle_dynamics.hpp"
#include "game_src/core/not_implemented.hpp"
#include "game_src/unresolved/vehicle_input.hpp"
#include "game_src/unresolved/wall_response.hpp"
#include <algorithm>
#include <cmath>

namespace roadwars {
namespace {
float length(Vec3 v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }
Vec3 scale(Vec3 v, float f) { return {v.x * f, v.y * f, v.z * f}; }
Vec3 add(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 subtract(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
Vec3 normalized(Vec3 v) {
    const float n = length(v);
    return n == 0 ? v : scale(v, 1 / n);
}
float cosine(Vec3 a, Vec3 b) {
    float value = dot(a, b) / (length(a) * length(b));
    if (value <= 1) {
        if (value < -1)
            value = -1;
    } else
        value = 1;
    return value;
}
constexpr float kDegToRad = 0.017455555498600006f;
constexpr float kRadToDeg = 57.288352966308594f;
float angle(Vec3 a, Vec3 b) {
    return static_cast<float>(std::acos(static_cast<double>(cosine(a, b))) * kRadToDeg);
}
Vec3 rotate(Vec3 vector, Vec3 axis, float radians) {
    const float s = std::sin(radians), c = std::cos(radians);
    return add(add(scale(vector, c), scale(cross(axis, vector), s)),
               scale(axis, dot(axis, vector) * (1 - c)));
}
Vec3 point(const Transform& t, Vec3 v) {
    return add(t.origin,
               add(add(scale(t.basis[0], v.x), scale(t.basis[1], v.y)), scale(t.basis[2], v.z)));
}
float moment_lever(Vec3 force, Vec3 application, Vec3 center) {
    const Vec3 displacement = subtract(center, application);
    const float radius = length(displacement);
    Vec3 flat_force = force;
    flat_force.y = 0;
    float heading = angle({0, 0, 1}, normalized(flat_force));
    if (flat_force.x < 0) {
        heading = 360 - heading;
        if (heading == 360)
            heading = 0;
    }
    Vec3 flat_displacement = displacement;
    flat_displacement.y = 0;
    flat_displacement = normalized(flat_displacement);
    const Vec3 rotated = rotate({0, 0, 1}, {0, 1, 0}, heading * kDegToRad);
    float between = angle(rotated, flat_displacement);
    const Vec3 test = rotate({0, 0, 1}, {0, 1, 0}, (heading - between) * kDegToRad);
    if (std::abs(test.x - flat_displacement.x) < 0.005f &&
        std::abs(test.y - flat_displacement.y) < 0.005f &&
        std::abs(test.z - flat_displacement.z) < 0.005f)
        between = 360 - between;
    return static_cast<float>(std::sin(static_cast<double>(between * kDegToRad)) * radius);
}
} // namespace
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0040d4cb
Result load_vehicle_metrics(ResourceStore& resources, std::string_view vehicle,
                            VehicleParameters& output, std::string& error) {
    Configuration config;
    if (config.load(resources, "Cars/" + std::string(vehicle) + "/Data/Data.ini") != Result::ok) {
        error = "Vehicle metric configuration is unavailable";
        return Result::not_found;
    }
    VehicleParameters result;
    auto read = [&](std::string_view section, std::string_view key, float& value) {
        if (config.number(section, key, value) == Result::ok && std::isfinite(value))
            return true;
        error = "Missing or invalid vehicle configuration: " + std::string(section) + "/" +
                std::string(key);
        return false;
    };
    if (!read("METRICS", "MASS(LBS)", result.mass_kg) ||
        !read("METRICS", "ENGINETORQUE(FT_LBS)", result.torque_nm) ||
        !read("METRICS", "FRONTALAREA(FT^2)", result.frontal_area_m2) ||
        !read("METRICS", "DRAGCOEFFICIENT", result.drag_coefficient) ||
        !read("METRICS", "REARENDRATIO", result.rear_end_ratio) ||
        !read("METRICS", "MAXWHEELANGLE", result.max_wheel_angle_degrees))
        return Result::invalid_argument;
    result.mass_kg *= 0.453599989414f;
    result.torque_nm *= 1.35575044155f;
    result.frontal_area_m2 *= 0.0929030403495f;
    float count{};
    if (!read("GEARINFO", "NUMGEARS", count) || count < 1 || count > 32 ||
        count != std::floor(count))
        return Result::invalid_argument;
    for (int i = 1; i <= static_cast<int>(count); ++i) {
        float ratio{};
        if (!read("GEARINFO", "GEARRATIO" + std::to_string(i), ratio) || ratio <= 0)
            return Result::invalid_argument;
        result.gear_ratios.push_back(ratio);
    }
    output = std::move(result);
    return Result::ok;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0040d4cb
Result finalize_vehicle_parameters(VehicleParameters& p) {
    for (float value : {p.mass_kg, p.rear_wheel_radius, p.length, p.width, p.height, p.wheelbase}) {
        if (!std::isfinite(value) || value <= 0)
            return Result::invalid_argument;
    }
    if (p.gear_ratios.empty())
        return Result::invalid_argument;
    p.gravity_force = -p.mass_kg * 9.80000019073f;
    p.quarter_gravity_force = p.gravity_force / 4.0f;
    p.inverse_mass = 1.0f / p.mass_kg;
    p.drag_factor = 0.5f * p.drag_coefficient * p.frontal_area_m2 * 1.20000004768f;
    p.partial_brake_factor = p.mass_kg * 20000.0f;
    p.yaw_inertia = (p.width * p.width + p.length * p.length) * ((p.height / p.length) * p.mass_kg);
    p.pitch_roll_inertia = p.length * p.length * (0.08333333581686f * p.mass_kg);
    return Result::ok;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00412834
Result load_vehicle_geometry(ResourceStore& resources, std::string_view vehicle,
                             VehicleParameters& p, std::string& error) {
    MeshObject body, tire;
    std::vector<std::byte> bytes;
    const auto prefix = "Cars/" + std::string(vehicle);
    for (auto item : {std::pair{prefix + "/Body/SpatialObject.msh", &body},
                      std::pair{prefix + "/Tires/" + p.tires.type + "/SpatialObject.msh", &tire}}) {
        if (resources.read(item.first, bytes) != Result::ok) {
            error = "Missing vehicle geometry: " + item.first;
            return Result::not_found;
        }
        if (decode_msh(bytes, *item.second, error) != Result::ok)
            return Result::invalid_argument;
    }
    auto bounds = [](const MeshObject& object, Vec3& lower, Vec3& upper) {
        bool found = false;
        for (const auto& part : object.parts)
            for (auto vertex : part.positions) {
                if (!found) {
                    lower = upper = vertex;
                    found = true;
                }
                lower.x = std::min(lower.x, vertex.x);
                lower.y = std::min(lower.y, vertex.y);
                lower.z = std::min(lower.z, vertex.z);
                upper.x = std::max(upper.x, vertex.x);
                upper.y = std::max(upper.y, vertex.y);
                upper.z = std::max(upper.z, vertex.z);
            }
        return found;
    };
    const MeshObject* body_mesh = nullptr;
    for (const auto& child : body.children)
        if (child.name == "1024Body")
            body_mesh = &child;
    Vec3 lower{}, upper{};
    if (!body_mesh || !bounds(*body_mesh, lower, upper)) {
        error = "Original 1024Body mesh is unavailable";
        return Result::unavailable;
    }
    p.length = std::max(upper.x - lower.x, upper.z - lower.z);
    p.width = std::min(upper.x - lower.x, upper.z - lower.z);
    p.height = upper.y - lower.y;
    p.body_local_position = add(body.position, body_mesh->position);
    if (!bounds(tire, lower, upper)) {
        error = "Tire bounds unavailable";
        return Result::invalid_argument;
    }
    p.rear_wheel_radius = (upper.y - lower.y) * 0.5f;
    p.tire_half_width = std::min(upper.x - lower.x, upper.z - lower.z) * 0.5f;
    float tire_scale{};
    if (load_wheel_scale(resources, vehicle, p.tires, tire_scale, error) != Result::ok) {
        return Result::invalid_argument;
    }
    const float tire_x_extent = (upper.x - lower.x) * tire_scale;
    const float tire_z_extent = upper.z - lower.z;
    const float tire_width = std::min(tire_x_extent, tire_z_extent);
    const float tire_length = std::max(tire_x_extent, tire_z_extent);
    p.tire_half_width = tire_width * .5f;
    const float tire_corner = static_cast<float>(
        std::atan(static_cast<double>((tire_width * 0.5f) / (tire_length * 0.5f))) * kRadToDeg);
    p.tire_slip_lower_degrees = tire_corner * 0.5f;
    p.tire_slip_upper_degrees = 180.0f - tire_corner * 0.5f;
    std::array<Vec3, 4> axle{};
    for (std::size_t i = 0; i < 4; ++i) {
        bool found_axle = false, found_ground = false;
        const auto name = "Tire" + std::to_string(i + 1);
        for (const auto& child : body.children) {
            if (child.name == name) {
                axle[i] = add(body.position, child.position);
                found_axle = true;
            }
            if (child.name == name + "Ground") {
                p.wheel_ground_anchors[i] = add(body.position, child.position);
                found_ground = true;
            }
        }
        if (!found_axle || !found_ground) {
            error = "Missing original tire anchor: " + name;
            return Result::unavailable;
        }
    }
    const float dx = std::abs(axle[0].x - axle[2].x), dz = std::abs(axle[0].z - axle[2].z);
    p.wheelbase = std::sqrt(dx * dx + dz * dz);
    p.wheel_axle_anchors = axle;
    const auto& a = p.wheel_ground_anchors;
    const Vec3 forward = normalized(cross(subtract(a[3], axle[3]), subtract(a[3], a[2])));
    const Vec3 right = normalized(cross(subtract(a[3], axle[3]), subtract(a[3], a[1])));
    const Vec3 left = scale(right, -1);
    for (std::size_t i = 0; i < 4; ++i) {
        p.positive_yaw_levers[i] = moment_lever(left, a[i], {});
        p.negative_yaw_levers[i] = moment_lever(right, a[i], {});
    }
    (void)forward;
    p.fixed_angular_radius = (length(a[0]) + length(a[2])) * 0.5f;
    Configuration tires;
    if (tires.load(resources, "Cars/Shared/Tires/" + p.tires.type + "/" + p.tires.quality +
                                  "/Data.ini") != Result::ok ||
        tires.number("FRICTION", "ADDEDFRICTION", p.added_tire_friction) != Result::ok) {
        error = "Selected tire friction configuration unavailable";
        return Result::not_found;
    }
    const auto kind = ResourceStore::canonical(p.tires.type);
    if (kind == "STREET")
        p.tire_kind = 0;
    else if (kind == "SLICKS")
        p.tire_kind = 1;
    else if (kind == "OFF ROAD")
        p.tire_kind = 2;
    else {
        error = "Unknown original tire type: " + p.tires.type;
        return Result::invalid_argument;
    }
    return finalize_vehicle_parameters(p);
}
// Aggregate expressions from 0042b31a; coefficients must already include its material/tire modifiers.
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0042b31a
VehicleForceLimits vehicle_force_limits(const VehicleParameters& p,
                                        const std::array<float, 4>& longitudinal,
                                        const std::array<float, 4>& lateral,
                                        const std::array<float, 4>& rolling) {
    VehicleForceLimits result;
    result.traction = (longitudinal[2] + longitudinal[3]) * 0.5f * p.mass_kg * 9.80000019073f;
    result.partial_brake = (longitudinal[0] + longitudinal[1]) * 0.5f * p.mass_kg * 9.80000019073f;
    result.full_brake = (lateral[0] + lateral[1]) * 0.5f * p.mass_kg * 9.80000019073f;
    result.lateral =
        (lateral[0] + lateral[1] + lateral[2] + lateral[3]) * 0.25f * -p.quarter_gravity_force;
    result.yaw_damping = result.lateral;
    for (std::size_t i = 0; i < 4; ++i)
        result.rolling[i] = -p.quarter_gravity_force * rolling[i];
    return result;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0042bf6b
void apply_vehicle_impulse(Vec3& velocity, Vec3 direction, float impulse, float inverse_mass,
                           bool braking) {
    const Vec3 delta = scale(direction, impulse * inverse_mass);
    if (!braking || length(velocity) - length(delta) > 0)
        velocity = add(velocity, delta);
    else
        velocity = {};
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0042c02f
void apply_angular_delta(float& rate, float delta, bool braking) {
    if (!braking) {
        rate += delta;
        return;
    }
    // Preserve the original two sequential tests, including its zero-crossing behavior.
    if (rate > 0 && delta + rate < 0)
        rate = 0;
    if (rate >= 0 || delta + rate <= 0)
        rate += delta;
    else
        rate = 0;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0042bbf3
float vehicle_drive_force(const VehicleParameters& p, std::size_t gear, float health_ratio,
                          float traction_limit, float difficulty_multiplier) {
    float torque = p.torque_nm;
    for (auto upgrade : p.engine_upgrades)
        torque += static_cast<float>(upgrade);
    return std::min(((torque * p.rear_end_ratio * p.gear_ratios.at(gear)) / p.rear_wheel_radius) *
                        (health_ratio * 0.7f + 0.3f) * difficulty_multiplier,
                    traction_limit);
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004136b1
float vehicle_rpm(const VehicleParameters& p, const VehicleState& state, std::size_t gear,
                  float throttle) {
    // 00413688 assembly: wheel revolutions/second, then revolutions/minute.
    const float wheel_rpm =
        (state.cached_speed / (p.rear_wheel_radius * 6.2831854820251465f)) * 60.0f;
    float rpm = wheel_rpm * p.rear_end_ratio * p.gear_ratios.at(gear);
    if (std::all_of(state.air_time.begin(), state.air_time.end(), [](float t) { return t != 0; }))
        rpm += std::abs(throttle) * 2000.0f;
    return rpm;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00413200
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0041328f
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004132db
void update_automatic_transmission(const VehicleParameters& p, VehicleState& state) {
    state.upshifted = false;
    if (!p.automatic_transmission)
        return;
    if (state.rpm > 6000.0f) {
        if (state.gear_index + 1 < p.gear_ratios.size()) {
            ++state.gear_index;
            state.upshifted = true;
        }
    } else if ((state.rpm < 4000.0f && state.engine_throttle > 0) || state.rpm < 3000.0f) {
        if (state.gear_index > 0)
            --state.gear_index;
    }
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0042cac8
void apply_vehicle_engine(const VehicleParameters& p, VehicleState& state, Vec3 forward,
                          float throttle, float drive_force, float dt) {
    if (throttle == 0)
        return;
    float factor = 1.70000004768f - state.rpm * 0.000166666664882f;
    if (factor > 1)
        factor = 1;
    const bool rear_left_air = state.air_time[2] != 0, rear_right_air = state.air_time[3] != 0;
    if (rear_left_air && rear_right_air)
        factor = 0;
    else if (rear_left_air != rear_right_air)
        factor *= 0.5f;
    apply_vehicle_impulse(state.velocity, forward, dt * drive_force * throttle * factor,
                          p.inverse_mass, false);
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0042ce7f
void apply_vehicle_brake(const VehicleParameters& p, VehicleState& state,
                         const VehicleForceLimits& limits, float brake, float dt) {
    const float speed = length(state.velocity);
    if (brake == 0 || speed == 0 || (state.air_time[0] != 0 && state.air_time[1] != 0))
        return;
    const float force = brake < 1 ? std::min(p.partial_brake_factor * brake, limits.partial_brake)
                                  : limits.full_brake;
    apply_vehicle_impulse(state.velocity, scale(state.velocity, -1 / speed), dt * force,
                          p.inverse_mass, true);
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0042d424
void apply_vehicle_drag(const VehicleParameters& p, VehicleState& state, Vec3 original_wind,
                        float dt) {
    const float speed = length(state.velocity);
    if (speed == 0)
        return;
    const float magnitude = state.cached_speed * p.drag_factor * state.cached_speed;
    apply_vehicle_impulse(state.velocity, scale(state.velocity, -1 / speed), magnitude * dt,
                          p.inverse_mass, true);
    apply_vehicle_impulse(state.velocity, original_wind, magnitude * dt * 2, p.inverse_mass, true);
}

namespace {
struct VehicleAxes {
    Vec3 forward{}, up{}, right{}, left{};
};
VehicleAxes vehicle_axes(const VehicleParameters& p, const VehicleState& state,
                         const std::array<Vec3, 4>& wheels) {
    const Vec3 axle = point(original_vehicle_transform(state.position, state.heading_degrees,
                                                       state.pitch_degrees, state.roll_degrees),
                            p.wheel_axle_anchors[3]);
    const Vec3 vertical = subtract(wheels[3], axle);
    VehicleAxes result;
    result.forward = normalized(cross(vertical, subtract(wheels[3], wheels[2])));
    result.up = normalized(cross(subtract(wheels[1], wheels[3]), subtract(wheels[1], wheels[0])));
    result.right = normalized(cross(vertical, subtract(wheels[3], wheels[1])));
    result.left = scale(result.right, -1);
    return result;
}
float ground_count(const VehicleState& state) {
    return static_cast<float>(std::count(state.air_time.begin(), state.air_time.end(), 0.0f));
}
void wrap_heading(float& heading) {
    if (heading < 0)
        heading += 360;
    if (heading >= 360)
        heading -= 360;
}
void clamp_tilt(float& tilt, float& rate) {
    if (tilt > 60) {
        tilt = 60;
        rate = 0;
    } else if (tilt < -60) {
        tilt = -60;
        rate = 0;
    }
}
} // namespace
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0042c16a
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0042c3ed
void apply_vehicle_tilt(const VehicleParameters& p, VehicleState& state,
                        const std::array<Vec3, 4>& wheels, Vec3 force, Vec3 application, float dt,
                        bool fixed_radius, bool braking, bool pitch_only) {
    const float radius =
        fixed_radius ? p.fixed_angular_radius : length(subtract(application, state.position));
    const Vec3 a = subtract(wheels[0], wheels[1]), b = subtract(wheels[0], wheels[2]);
    const Vec3 relative = subtract(application, state.position);
    auto apply = [&](Vec3 plane, Vec3 reference, float& rate) {
        const float force_angle = angle(force, plane);
        const float moment = static_cast<float>(
            std::cos(static_cast<double>(force_angle * kDegToRad)) * (length(force) * radius));
        float delta = (moment / p.pitch_roll_inertia) * kRadToDeg * dt;
        const float application_angle = angle(reference, relative);
        if (application_angle > 90)
            delta = -delta;
        delta = static_cast<float>(
            std::abs(std::cos(static_cast<double>(application_angle * kDegToRad))) * delta);
        apply_angular_delta(rate, delta, braking);
    };
    apply(normalized(cross(a, b)), b, state.pitch_rate);
    if (!pitch_only)
        apply(normalized(cross(b, a)), a, state.roll_rate);
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0042c0c0
void apply_vehicle_yaw_impulse(const VehicleParameters& p, VehicleState& state, Vec3 force,
                               Vec3 application, float dt, bool braking, float fixed_lever) {
    Vec3 flat_application = application, flat_center = state.position;
    flat_application.y = 0;
    flat_center.y = 0;
    const float moment =
        length(force) *
        (fixed_lever == -1.0f ? moment_lever(force, flat_application, flat_center) : fixed_lever);
    apply_angular_delta(state.yaw_rate, (moment / p.yaw_inertia) * kRadToDeg * dt, braking);
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0042cc1b
void apply_vehicle_gravity(const VehicleParameters& p, VehicleState& state,
                           const std::array<Vec3, 4>& wheels, Vec3 up, float dt) {
    const float count = ground_count(state);
    for (std::size_t i = 0; i < 4; ++i)
        if (state.air_time[i] != 0)
            apply_vehicle_tilt(p, state, wheels, {0, p.quarter_gravity_force, 0}, wheels[i], dt,
                               true, false);
    const Vec3 force = add({0, p.gravity_force, 0}, scale(up, -count * p.quarter_gravity_force));
    apply_vehicle_impulse(state.velocity, force, dt, p.inverse_mass, false);
    if (count == 4) {
        const float velocity_angle = angle(up, state.velocity);
        if (velocity_angle > 90) {
            const float magnitude = static_cast<float>(
                std::sin(static_cast<double>((velocity_angle - 90) * kDegToRad)) *
                (length(state.velocity) * p.mass_kg));
            // Original pushes scalar 1.0, not dt, for this velocity correction.
            apply_vehicle_impulse(state.velocity, scale(up, magnitude), 1, p.inverse_mass, false);
        }
    }
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0042cfa6
void apply_vehicle_friction(const VehicleParameters& p, VehicleState& state,
                            const VehicleForceLimits& limits, Vec3 forward, Vec3 up, Vec3 right,
                            float dt) {
    if (length(state.velocity) == 0)
        return;
    const float up_angle = angle(up, state.velocity);
    if (up_angle <= 75 || up_angle >= 105)
        return;
    const float forward_angle = angle(forward, state.velocity);
    const float count = ground_count(state);
    float magnitude = limits.rolling[0];
    Vec3 direction = scale(normalized(state.velocity), -1);
    if (forward_angle >= 0.5f && forward_angle <= 179.5f) {
        const float speed = length(state.velocity);
        apply_vehicle_impulse(state.velocity, direction, count * magnitude * dt, p.inverse_mass,
                              true);
        if (speed < 9.80000019073f && count == 4) {
            state.velocity = scale(
                forward, static_cast<float>(
                             std::cos(static_cast<double>(forward_angle * kDegToRad)) * speed));
        } else {
            magnitude = limits.lateral;
            direction = cosine(right, state.velocity) >= 0 ? scale(right, -1) : right;
            direction = normalized(direction);
        }
    }
    apply_vehicle_impulse(state.velocity, direction, count * magnitude * dt, p.inverse_mass, true);
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0042d533
void apply_vehicle_turn(const VehicleParameters& p, VehicleState& state, Vec3 forward, Vec3 up,
                        Vec3 right, float throttle, float front_grip, float dt) {
    const float steering = state.steering_degrees, speed = length(state.velocity);
    if (steering == 0 || speed == 0 || (state.air_time[0] != 0 && state.air_time[1] != 0))
        return;
    const float tangent =
        static_cast<float>(std::tan((90.0 - static_cast<double>(std::abs(steering))) * kDegToRad));
    float radius = tangent * p.wheelbase;
    const float force = (speed * speed * p.mass_kg) / radius;
    if (p.mass_kg * 9.80000019073f * front_grip < force)
        radius = (speed * speed) / (9.80000019073f * front_grip);
    float yaw_delta = 0;
    if (state.yaw_rate < 180 && state.yaw_rate > -180)
        yaw_delta = (steering / p.max_wheel_angle_degrees) * throttle * 20 * 9.80000019073f * dt;
    const Vec3 direction = steering <= 0 ? scale(right, -1) : right;
    float turn = (speed / radius) * kRadToDeg * dt;
    if (turn != 0) {
        state.cached_slip_angle = angle(forward, state.velocity);
        const Vec3 rotated =
            rotate(forward, up, (state.cached_slip_angle >= 90 ? -90.0f : 90.0f) * kDegToRad);
        if (cosine(rotated, direction) < 0)
            turn = -turn;
        const float influence =
            std::min(std::abs(steering / p.max_wheel_angle_degrees) * 1.5f, 1.0f);
        state.heading_degrees += turn * influence;
        wrap_heading(state.heading_degrees);
        state.yaw_rate = yaw_delta + state.yaw_rate;
    }
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0042d88c
void apply_vehicle_yaw_damping(const VehicleParameters& p, VehicleState& state,
                               const VehicleForceLimits& limits, float dt) {
    for (std::size_t i = 0; i < 4 && state.yaw_rate != 0; ++i)
        if (state.air_time[i] == 0) {
            const bool positive_lever = i < 2 ? state.yaw_rate >= 0 : state.yaw_rate < 0;
            const float lever =
                positive_lever ? p.positive_yaw_levers[i] : p.negative_yaw_levers[i];
            const float impulse = (limits.yaw_damping + limits.rolling[i]) * dt;
            const float delta = (lever / p.yaw_inertia) * kRadToDeg * impulse;
            apply_angular_delta(state.yaw_rate, delta, true);
        }
}

Result VehicleDynamics::initialize(VehicleParameters parameters, const TrackCollision& track,
                                   std::string& error) {
    if (finalize_vehicle_parameters(parameters) != Result::ok) {
        error = "Vehicle geometry/parameters are incomplete";
        return Result::invalid_argument;
    }
    parameters_ = std::move(parameters);
    disabled_contact_1c_ = 0; // Constructor0040d4cb; diagnostic initialize reconstructs this state.
    track_ = &track;
    state_ = {};
    state_.position = track.spawn().position;
    state_.heading_degrees = track.spawn().heading_degrees;
    body_ = {};
    wall_contact_count_ = 0;
    particle_impacts_.clear();
    collision_audio_.reset();
    tire_audio_input_ = {};
    tire_particle_sources_ = {};
    for (std::size_t i = 0; i < 4; ++i)
        local_wheel_heights_[i] = parameters_.wheel_ground_anchors[i].y;
    cache_wheels();
    return Result::ok;
}
void VehicleDynamics::cache_wheels(bool sample_contacts) {
    const auto t = transform();
    for (std::size_t i = 0; i < 4; ++i) {
        Vec3 local = parameters_.wheel_ground_anchors[i];
        local.y = local_wheel_heights_[i];
        wheel_points_[i] = point(t, local);
        auto query = wheel_points_[i];
        query.y += 3.0f;
        if (sample_contacts)
            contacts_[i] = track_->sample(query);
    }
}
// Single-player, undamaged, stock Street/Poor keyboard driving profile.
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0042da4b
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00480b40
void VehicleDynamics::update(float dt, const InputState& input) {
    update_internal(dt, input, nullptr);
}
void VehicleDynamics::update_controlled(float dt, const VehicleControls& controls) {
    update_internal(dt, {}, &controls);
}
void VehicleDynamics::reset_to(const TrackSpawn& spawn) {
    ++pose_generation_;
    state_ = {};
    state_.position = spawn.position;
    state_.heading_degrees = spawn.heading_degrees;
    body_ = {};
    collision_audio_.reset();
    particle_impacts_.clear();
    tire_particle_sources_ = {};
    tire_audio_input_ = {};
    for (std::size_t i = 0; i < 4; ++i)
        local_wheel_heights_[i] = parameters_.wheel_ground_anchors[i].y;
    cache_wheels();
}
void VehicleDynamics::update_internal(float dt, const InputState& input,
                                      const VehicleControls* controls) {
    begin_update(dt, input, controls);
    solve_race_walls(dt);
    finish_race_update(dt);
}
void VehicleDynamics::begin_update(float dt, const InputState& input,
                                   const VehicleControls* controls) {
    step_pending_ = false;
    particle_impacts_.clear();
    tire_particle_sources_ = {};
    if (!track_ || !std::isfinite(dt) || dt < 0)
        not_implemented("vehicle_input_contract", "Invalid vehicle runtime input");
    if (input.reset_requested) {
        std::string error;
        if (initialize(parameters_, *track_, error) != Result::ok)
            not_implemented("test_profile_reset", "Test profile spawn reset failed");
        return;
    }
    if (collision_audio_.advance(dt) != Result::ok)
        not_implemented("collision_audio_delta", "Invalid collision audio time step");
    if (dt == 0)
        return;
    step_previous_ = state_;
    const auto axes = vehicle_axes(parameters_, state_, wheel_points_);
    // 00424d3e refreshes 5ba0 before force integration; 0042d533 may refresh it again.
    state_.cached_slip_angle = angle(axes.forward, state_.velocity);
    step_effect_contacts_ = contacts_;
    step_effect_speed_ = state_.cached_speed;
    state_.steering_degrees =
        advance_keyboard_steering(state_.steering_degrees, parameters_.max_wheel_angle_degrees,
                                  (input.steering > 0) - (input.steering < 0), dt);
    const auto command =
        keyboard_driving_command(input.throttle > 0, input.brake > 0, length(state_.velocity) != 0,
                                 state_.cached_slip_angle, state_.brake_amount, dt);
    state_.engine_throttle = command.engine_throttle;
    state_.brake_amount = input.handbrake ? 1.0f : command.brake_amount;
    if (controls) {
        state_.engine_throttle = controls->engine_throttle;
        state_.brake_amount = controls->brake_amount;
        state_.steering_degrees = controls->steering_degrees;
    }
    if (command.force_first_gear)
        state_.gear_index = 0;
    state_.reverse_engaged = state_.engine_throttle < 0;
    state_.rpm = vehicle_rpm(parameters_, state_, state_.gear_index, state_.engine_throttle);
    std::array<float, 4> longitudinal{}, lateral{}, rolling{};
    for (std::size_t i = 0; i < 4; ++i) {
        float modifier = 0;
        const auto& contact = contacts_[i];
        if (contact.material_kind == static_cast<std::uint32_t>(parameters_.tire_kind))
            modifier += parameters_.added_tire_friction + parameters_.added_tire_friction;
        else if (contact.lateral_grip > 0.4f)
            modifier -= parameters_.tire_kind == 0 && contact.material_kind == 2 ? 0.05f : 0.1f;
        float value = modifier + contact.longitudinal_grip;
        longitudinal[i] = value + value;
        value = modifier + contact.lateral_grip;
        lateral[i] = value + value;
        rolling[i] =
            std::min(state_.cached_speed / 26.0f, 1.0f) * contact.rolling_resistance + 0.02f;
        if (longitudinal[i] <= 0)
            longitudinal[i] = 0.01f;
        if (lateral[i] <= 0)
            lateral[i] = 0.01f;
        if (rolling[i] <= 0)
            rolling[i] = 0.01f;
    }
    apply_oil_slick_grip(oil_contacts_, state_.cached_speed, longitudinal, lateral, rolling);
    if (equipment_.internal_hp <= 0 && state_.cached_speed < 10 && disabled_contact_1c_ > 0) {
        const float value = state_.cached_speed * .2f / 10;
        longitudinal.fill(value);
        lateral.fill(value);
        rolling.fill(value);
    }
    for (std::size_t wheel = 0; wheel < 4; ++wheel) {
        longitudinal[wheel] *= equipment_.traction_multiplier;
        lateral[wheel] *= equipment_.traction_multiplier;
    }
    const auto limits = vehicle_force_limits(parameters_, longitudinal, lateral, rolling);
    longitudinal_grip_ =
        (longitudinal[0] + longitudinal[1] + longitudinal[2] + longitudinal[3]) * .25f;
    const float health_ratio = equipment_.maximum_internal_hp > 0
                                   ? equipment_.internal_hp / equipment_.maximum_internal_hp
                                   : 1.0f;
    const float drive =
        vehicle_drive_force(parameters_, state_.gear_index, health_ratio, limits.traction);
    apply_vehicle_engine(parameters_, state_, axes.forward, state_.engine_throttle, drive, dt);
    apply_vehicle_gravity(parameters_, state_, wheel_points_, axes.up, dt);
    apply_vehicle_brake(parameters_, state_, limits, state_.brake_amount, dt);
    apply_vehicle_friction(parameters_, state_, limits, axes.forward, axes.up, axes.right, dt);
    apply_vehicle_drag(parameters_, state_, {0, -1, 0}, dt);
    apply_vehicle_turn(parameters_, state_, axes.forward, axes.up, axes.right,
                       state_.engine_throttle, (longitudinal[0] + longitudinal[1]) * 0.5f, dt);
    const Vec3 displacement = scale(state_.velocity, dt);
    apply_vehicle_yaw_damping(parameters_, state_, limits, dt);
    state_.position = add(state_.position, displacement);
    if (state_.yaw_rate != 0) {
        state_.heading_degrees += dt * state_.yaw_rate;
        wrap_heading(state_.heading_degrees);
    }
    if (state_.pitch_rate != 0) {
        state_.pitch_degrees += dt * state_.pitch_rate;
        clamp_tilt(state_.pitch_degrees, state_.pitch_rate);
    }
    if (state_.roll_rate != 0) {
        state_.roll_degrees += dt * state_.roll_rate;
        clamp_tilt(state_.roll_degrees, state_.roll_rate);
    }
    cache_wheels();
    if (update_contact_orientation(state_, body_, wheel_points_, contacts_, dt) != Result::ok) {
        if (respawn_handler_) {
            reset_to(respawn_handler_());
            return;
        }
        not_implemented(
            "93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004185fe",
            "Original missing-ground lap-based respawn");
    }
    step_pending_ = true;
}
void VehicleDynamics::solve_race_walls(float dt) {
    collision_damage_result_ = Result::ok;
    if (!step_pending_)
        return;
    const auto collision_body = [&](const VehicleState& state) {
        CollisionBody body;
        body.position = state.position;
        body.heading_degrees = state.heading_degrees;
        body.front_extension = parameters_.front_extension;
        const float half_width = parameters_.width * 0.5f, half_length = parameters_.length * 0.5f;
        body.diagonal_radius = std::sqrt(half_width * half_width + half_length * half_length);
        body.corner_angle_degrees = static_cast<float>(
            std::atan(static_cast<double>(half_width / half_length)) * kRadToDeg);
        body.radius = parameters_.length;
        const auto basis = original_vehicle_transform(state.position, state.heading_degrees,
                                                      state.pitch_degrees, state.roll_degrees);
        body.edge_normals = {scale(basis.basis[2], -1), scale(basis.basis[0], -1), basis.basis[2],
                             basis.basis[0]};
        return body;
    };
    // RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0047fff6
    // 0047fff6 decrements the original 42d8 cooldown immediately before wall handling.
    for (unsigned iteration = 0; iteration < 101; ++iteration) {
        const auto walls =
            track_->nearby_walls(state_.position, step_previous_.position, parameters_.length);
        const auto contact =
            solve_wall_contact(collision_body(step_previous_), collision_body(state_), walls);
        if (!contact.hit && !contact.restore_previous)
            break;
        ++wall_contact_count_;
        // 0042e9e3 pushes refresh=1 before preserving local pre-response speed.
        if (contact.hit && !contact.restore_previous)
            state_.cached_speed = length(state_.velocity);
        Vec3 response_force;
        apply_wall_response(parameters_, state_, contact, dt, &response_force);
        if (contact.restore_previous)
            break;
        // 0042e8f0: the original effect trigger shares a 0.5-second sound/effect gate.
        auto position = contact.contact_point;
        position.y = state_.position.y + parameters_.body_local_position.y;
        if (collision_audio_.wall_contact(state_.cached_speed, position)) {
            particle_impacts_.push_back({position, contact.normal, 1.5f});
            if (particle_impact_handler_)
                particle_impact_handler_(particle_impacts_.back());
        }
        if (iteration == 0 && collision_damage_handler_) {
            collision_damage_result_ =
                collision_damage_handler_(scale(response_force, .75f), contact.contact_point);
            if (collision_damage_result_ != Result::ok)
                return;
        }
    }
}
void VehicleDynamics::finish_race_update(float dt) {
    (void)dt;
    if (!step_pending_)
        return;
    step_pending_ = false;
    // 00480b40 shifts after force/collision work using the cached preforce RPM.
    state_.cached_speed = length(state_.velocity);
    update_automatic_transmission(parameters_, state_);
    // 0042c670(0) refreshes 5b70 without replacing earlier ground/material records.
    cache_wheels(false);
    const auto effect_wheel_points = wheel_points_;
    // 0042a11c (wheel landings and chassis strikes) follows wall response and speed refresh.
    update_wheel_suspension(parameters_, state_, body_, local_wheel_heights_, wheel_points_,
                            contacts_, &collision_audio_);
    update_chassis_ground_strikes(parameters_, state_, wheel_points_, *track_,
                                  point(transform(), parameters_.body_local_position));
    cache_wheels();
    // RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00413425
    // The effect predicates do not change the original physical solver.
    const bool moving = length(state_.velocity) != 0;
    tire_audio_input_ = {false, step_effect_contacts_[0].longitudinal_grip,
                         step_effect_contacts_[0].skid_sound_path, state_.position};
    for (std::size_t wheel = 0; wheel < 4; ++wheel) {
        auto& source = tire_particle_sources_[wheel];
        source.position = effect_wheel_points[wheel];
        source.position.y += 0.10000000149011612f;
        source.color_argb = step_effect_contacts_[wheel].particle_color_argb;
        const bool grounded = state_.air_time[wheel] == 0;
        const bool slide = moving && state_.cached_speed > 2.0f &&
                           state_.cached_slip_angle > parameters_.tire_slip_lower_degrees &&
                           state_.cached_slip_angle < parameters_.tire_slip_upper_degrees;
        const bool full_brake = moving && state_.brake_amount >= 1.0f;
        const bool dust =
            step_effect_speed_ >= 1.0f && step_effect_contacts_[wheel].particle_dust != 0;
        source.emitting = grounded && (slide || full_brake || dust);
        source.skid_enabled = grounded && (slide || full_brake);
        tire_audio_input_.any_skid = tire_audio_input_.any_skid || source.skid_enabled;
        source.skid_color_argb = step_effect_contacts_[wheel].skid_color_argb;
    }
}
Transform VehicleDynamics::transform() const {
    return original_vehicle_transform(state_.position, state_.heading_degrees, state_.pitch_degrees,
                                      state_.roll_degrees);
}
} // namespace roadwars
