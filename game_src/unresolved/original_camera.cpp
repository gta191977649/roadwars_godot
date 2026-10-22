#include "game_src/unresolved/original_camera.hpp"
#include <algorithm>
#include <cmath>
#include <utility>

namespace roadwars {
namespace {
constexpr float kDegreesToRadians = 0.017455555498600006f;
constexpr float kRadiansToDegrees = 57.288352966308594f;
Vec3 add(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 subtract(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 scale(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
float length(Vec3 a) { return std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z); }
Vec3 normalized(Vec3 a) {
    const auto n = length(a);
    return n == 0 ? a : scale(a, 1 / n);
}
Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
float cosine(Vec3 a, Vec3 b) {
    float value = dot(a, b) / (length(a) * length(b));
    if (value <= 1) {
        if (value < -1)
            value = -1;
    } else
        value = 1;
    return value;
}
float angle(Vec3 a, Vec3 b) {
    return static_cast<float>(std::acos(static_cast<double>(cosine(a, b))) * kRadiansToDegrees);
}
float direction_heading(Vec3 a) {
    a = normalized(a);
    float heading = angle({0, 0, 1}, a);
    if (a.x < 0) {
        heading = 360 - heading;
        if (heading == 360)
            heading = 0;
    }
    return heading;
}
Vec3 vector(const Transform& t, Vec3 p) {
    return add(add(scale(t.basis[0], p.x), scale(t.basis[1], p.y)), scale(t.basis[2], p.z));
}
float approach(float value, float target, float up_step, float down_step) {
    if (value <= target) {
        if (value < target) {
            value += up_step;
            if (target < value)
                value = target;
        }
    } else {
        value -= down_step;
        if (value < target)
            value = target;
    }
    return value;
}
// Same XZ predicate as 004dd310/0047c368, including original collinear cases.
bool intersect(Vec3 a, Vec3 b, Vec3 c, Vec3 d, Vec3& point) {
    const float x = a.x - b.x, z = a.z - b.z, u = -(c.x - d.x), v = -(c.z - d.z);
    if ((u == 0 && v == 0) || (x == 0 && z == 0))
        return false;
    const auto between = [](float p, float q, float r) {
        return (p <= q && r <= p) || (p <= r && q <= p);
    };
    if (u == 0 && x == 0) {
        if (a.x != c.x)
            return false;
        if (between(a.z, c.z, d.z) || between(b.z, c.z, d.z)) {
            point.x = a.x;
            point.z = (a.z + b.z + c.z + d.z) / 4;
            return true;
        }
    }
    if (v == 0 && z == 0) {
        if (a.z != c.z)
            return false;
        if (between(a.x, c.x, d.x) || between(b.x, c.x, d.x)) {
            point.x = (a.x + b.x + c.x + d.x) / 4;
            point.z = a.z;
            return true;
        }
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
    if (t > 0 || t < -1 || s > 0 || s < -1)
        return false;
    point.x = x * t + a.x;
    point.z = z * t + a.z;
    return true;
}
} // namespace

// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0040d4cb
// Preserve original configuration axis remapping; only HOODZ enters the stock chase path.
Result load_original_camera_settings(ResourceStore& resources, std::string_view vehicle,
                                     OriginalCameraSettings& settings, std::string& error) {
    Configuration config;
    const auto prefix = "Cars/" + std::string(vehicle);
    if (config.load(resources, prefix + "/Data/Data.ini") != Result::ok) {
        error = "Original camera configuration unavailable";
        return Result::not_found;
    }
    OriginalCameraSettings result;
    if (config.number("CAMERAINFO", "HOODZ", result.hood_height_offset) != Result::ok ||
        !std::isfinite(result.hood_height_offset)) {
        error = "Original camera HOODZ setting unavailable";
        return Result::invalid_argument;
    }
    float in_x{}, in_y{}, in_z{}, hood_x{}, hood_y{};
    for (const auto& [key, value] :
         std::array<std::pair<std::string_view, float*>, 5>{{{"INCARX", &in_x},
                                                          {"INCARY", &in_y},
                                                          {"INCARZ", &in_z},
                                                          {"HOODX", &hood_x},
                                                          {"HOODY", &hood_y}}}) {
        if (config.number("CAMERAINFO", key, *value) != Result::ok || !std::isfinite(*value)) {
            error = "Original camera setting unavailable or invalid: " + std::string(key);
            return Result::invalid_argument;
        }
    }
    result.in_car_local_position = {-in_x, in_z, in_y};
    result.hood_local_position = {hood_x, result.hood_height_offset, hood_y};
    std::vector<std::byte> bytes;
    MeshObject body;
    if (resources.read(prefix + "/Body/SpatialObject.msh", bytes) != Result::ok) {
        error = "Original camera body mesh unavailable";
        return Result::not_found;
    }
    if (decode_msh(bytes, body, error) != Result::ok)
        return Result::invalid_argument;
    bool found = false;
    for (const auto& child : body.children)
        if (child.name == "1024Body") {
            result.body_local_position = child.position;
            result.body_local_angles = child.angles_degrees;
            found = true;
            break;
        }
    if (!found) {
        error = "Original camera 1024Body node unavailable";
        return Result::unavailable;
    }
    // 0040ee2c binds the native cockpit eye to HoodPoint (+44cc), not the unused
    // INCAR configuration. Root serialized translation is replaced by VehicleState.
    std::vector<std::size_t> path;
    const auto find = [&](auto&& self, const MeshObject& node) -> bool {
        if (node.name == "HoodPoint")
            return true;
        for (std::size_t i = 0; i < node.children.size(); ++i) {
            path.push_back(i);
            if (self(self, node.children[i]))
                return true;
            path.pop_back();
        }
        return false;
    };
    if (!find(find, body)) {
        error = "Original camera HoodPoint node unavailable";
        return Result::unavailable;
    }
    result.hood_point_local_position = original_mesh_node_pose(body, path, {}, {}).origin;
    settings = result;
    error.clear();
    return Result::ok;
}

CameraView next_camera_view(CameraView view) {
    switch (view) {
    case CameraView::near_chase:
        return CameraView::far_chase;
    case CameraView::far_chase:
        return CameraView::hood;
    case CameraView::hood:
        return CameraView::cockpit;
    case CameraView::cockpit:
        return CameraView::near_chase;
    }
    return CameraView::near_chase;
}

// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00409420
// Native cockpit eye/orientation, plus explicitly modern far-chase and hood modes.
Result camera_view_frame(CameraView view, const VehicleState& vehicle,
                         const VehicleParameters& parameters, const OriginalCameraSettings& settings,
                         const TrackCollision* track, const CameraFrame& near_frame,
                         CameraFrame& output) {
    if (view == CameraView::near_chase) {
        output = near_frame;
        return Result::ok;
    }
    const auto finite = [](Vec3 v) {
        return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
    };
    if (!finite(vehicle.position) || !std::isfinite(vehicle.heading_degrees) ||
        !std::isfinite(vehicle.pitch_degrees) || !std::isfinite(vehicle.roll_degrees))
        return Result::invalid_argument;
    if (view == CameraView::far_chase) {
        output = near_frame;
        const Vec3 direction = normalized(subtract(near_frame.transform.origin, near_frame.pivot));
        output.transform.origin = add(near_frame.transform.origin, scale(direction, 2.6f));
        output.transform.origin.y += .5f;
        if (track) {
            const float radius = length(subtract(output.transform.origin, output.pivot)) + 2;
            const auto walls = track->nearby_walls(output.transform.origin, output.transform.origin,
                                                   radius, true);
            const float wall_distance = original_camera_wall_distance(
                output.transform.origin, output.heading_degrees, output.pivot, parameters.length,
                walls);
            if (wall_distance != 0) {
                const float desired = std::max(.1f, wall_distance - 1);
                const auto direction_to_eye =
                    normalized(subtract(output.transform.origin, output.pivot));
                output.transform.origin = add(output.pivot, scale(direction_to_eye, desired));
            }
            const auto ground = track->sample(output.transform.origin);
            if (ground.hit)
                output.transform.origin.y = std::max(output.transform.origin.y, ground.point.y + .5f);
        }
        return finite(output.transform.origin) ? Result::ok : Result::invalid_argument;
    }
    if (view != CameraView::hood && view != CameraView::cockpit)
        return Result::invalid_argument;
    const auto pose = original_vehicle_transform(vehicle.position, vehicle.heading_degrees,
                                                  vehicle.pitch_degrees, vehicle.roll_degrees);
    const Vec3 local = view == CameraView::cockpit
                           ? settings.hood_point_local_position
                           : add(settings.body_local_position, settings.hood_local_position);
    if (!finite(local) || !finite(settings.body_local_angles))
        return Result::invalid_argument;
    const Vec3 eye = add(vehicle.position, vector(pose, local));
    output = original_camera_frame(eye, eye, vehicle.heading_degrees,
                                    vehicle.pitch_degrees + settings.body_local_angles.y);
    const auto orientation = original_vehicle_transform(
        eye, vehicle.heading_degrees, vehicle.pitch_degrees + settings.body_local_angles.y,
        vehicle.roll_degrees + settings.body_local_angles.z);
    output.transform.basis = {scale(orientation.basis[0], -1), orientation.basis[1],
                              scale(orientation.basis[2], -1)};
    output.vertical_fov_degrees = near_frame.vertical_fov_degrees;
    return Result::ok;
}

// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004603f3
float interpolate_original_camera_angle(float current, float target, float amount) {
    const auto wrap_once = [](float a) {
        if (a < 360) {
            if (a < 0)
                a += 360;
        } else
            a -= 360;
        return a;
    };
    target = wrap_once(target);
    current = wrap_once(current);
    const float difference = target - current;
    if (std::abs(difference) <= 180)
        current = (target - current) * amount + current;
    else if (target <= current)
        current = ((360 - current) + target) * amount + current;
    else
        current -= ((360 - target) + current) * amount;
    if (difference >= 0 || target - current <= 0) {
        if (difference > 0 && target - current < 0)
            current = target;
    } else
        current = target;
    return current;
}

CameraFrame original_camera_frame(Vec3 eye, Vec3 pivot, float heading, float pitch) {
    const auto pose = original_vehicle_transform(eye, heading, pitch, 0);
    CameraFrame result;
    result.transform = pose;
    result.pivot = pivot;
    result.transform.basis[0] = scale(pose.basis[0], -1);
    result.transform.basis[2] = scale(pose.basis[2], -1);
    result.heading_degrees = heading;
    result.pitch_degrees = pitch;
    return result;
}

// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0040ab6a
// Wall-distance portion. Track broadphase and final ground clearance belong to the caller.
float original_camera_wall_distance(Vec3 eye, float heading, Vec3 body_world_center,
                                    float vehicle_radius, std::span<const WallRecord> walls) {
    const Vec3 forward{std::sin(heading * kDegreesToRadians), 0,
                       std::cos(heading * kDegreesToRadians)};
    Vec3 from = subtract(eye, forward);
    const Vec3 to = add(body_world_center, forward);
    float obstruction{};
    for (unsigned iteration = 0; iteration < 51; ++iteration) {
        bool hit = false;
        Vec3 nearest{};
        float nearest_distance{};
        for (const auto& wall : walls) {
            Vec3 lower = wall.lower, upper = wall.upper, point{};
            lower.x += wall.normal.x;
            lower.z += wall.normal.z;
            upper.x += wall.normal.x;
            upper.z += wall.normal.z;
            if (!intersect(from, to, lower, upper, point))
                continue;
            point.y = from.y;
            const float distance = length(subtract(point, to));
            if (!hit || distance < nearest_distance) {
                hit = true;
                nearest = point;
                nearest_distance = distance;
            }
        }
        if (!hit)
            break;
        from.x = nearest.x;
        from.z = nearest.z;
        obstruction = std::max(length(subtract(from, to)), vehicle_radius * 0.6000000238418579f);
    }
    return obstruction;
}

Result OriginalChaseCamera::load(ResourceStore& resources, std::string_view vehicle,
                                 std::string& error) {
    const auto result = load_original_camera_settings(resources, vehicle, settings_, error);
    if (result != Result::ok)
        return result;
    loaded_ = true;
    reset();
    return Result::ok;
}
void OriginalChaseCamera::reset() {
    frame_ = {};
    initialized_ = false;
    look_back_ = false;
    follow_heading_ = follow_pitch_ = 0;
    distance_ = 6.5f;
    distance_offset_ = height_ = obstruction_distance_ = 0;
}

// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004094fb
// Active default chase path. Original intro/countdown/camera-acquisition dispatch is not fabricated.
void OriginalChaseCamera::compute_pose(const VehicleState& vehicle, const VehicleParameters&,
                                       float dt, const OriginalCameraView& view) {
    const auto vehicle_pose = original_vehicle_transform(
        vehicle.position, vehicle.heading_degrees, vehicle.pitch_degrees, vehicle.roll_degrees);
    const auto forward = vehicle_pose.basis[2];
    float desired_distance = 6.5f, pitch_offset = 0, vertical_offset = 0;
    if (view.nitrous_active) {
        desired_distance += .5f;
        vertical_offset = view.badlands ? .2f : .5f;
        pitch_offset = -7.5f;
    }
    if (obstruction_distance_ != 0)
        desired_distance = obstruction_distance_ - 1;
    if (desired_distance <= 0)
        desired_distance = .1f;
    distance_ = approach(distance_, desired_distance, 2 * dt, 4 * dt);
    float target_offset = vehicle.engine_throttle > 0 ? 1.0f
                          : vehicle.brake_amount > 0  ? -1.0f
                                                      : 0.0f;
    if (obstruction_distance_ != 0)
        target_offset = 0;
    distance_offset_ = approach(distance_offset_, target_offset, dt, dt);
    Vec3 direction =
        (view.follow_vehicle_heading || vehicle.cached_speed < 2) ? forward : vehicle.velocity;
    float desired_pitch{};
    if (std::abs(direction.y) >= 0.05000000074505806) {
        direction = normalized(direction);
        desired_pitch = angle({direction.x, 0, direction.z}, direction);
        if (cosine(direction, forward) < 0)
            desired_pitch = -desired_pitch;
    } else
        direction = normalized(direction);
    if (direction.y < 0)
        desired_pitch = -desired_pitch;
    const Vec3 desired_heading_vector =
        (view.follow_vehicle_heading || vehicle.cached_speed < 2) ? forward : vehicle.velocity;
    const float desired_heading =
        direction_heading(desired_heading_vector) + (view.look_back ? 180.0f : 0.0f);
    if (!initialized_ || look_back_ != view.look_back) {
        follow_heading_ = desired_heading;
        follow_pitch_ = desired_pitch;
        initialized_ = true;
        look_back_ = view.look_back;
    } else
        follow_heading_ = interpolate_original_camera_angle(
            follow_heading_, desired_heading, dt * (view.follow_vehicle_heading ? 7.0f : 2.5f));
    follow_pitch_ =
        interpolate_original_camera_angle(follow_pitch_, desired_pitch + pitch_offset, dt * 2);
    direction = original_vehicle_transform({}, follow_heading_, follow_pitch_, 0).basis[2];
    if (view.look_back)
        direction.y = -direction.y;
    float ratio = distance_ / desired_distance;
    if (ratio > 1)
        ratio = 1;
    const float desired_height =
        static_cast<float>(ratio * .6000000238418579f - distance_ * direction.y * 1.2f) +
        vertical_offset;
    height_ = (desired_height - height_) * dt * 4 + height_;
    Vec3 eye = vehicle.position;
    eye.x -= (distance_ + distance_offset_) * direction.x;
    eye.z -= (distance_ + distance_offset_) * direction.z;
    eye.y += settings_.body_local_position.y + settings_.hood_height_offset + height_;
    const Vec3 pivot = add(vehicle.position, vector(vehicle_pose, settings_.body_local_position));
    frame_ = original_camera_frame(eye, pivot, follow_heading_,
                                   view.look_back ? -follow_pitch_ : follow_pitch_);
}

Result OriginalChaseCamera::update(const VehicleState& vehicle, const VehicleParameters& parameters,
                                   const TrackCollision* track, float delta_seconds,
                                   CameraFrame& output, const OriginalCameraView& view) {
    if (!loaded_)
        return Result::unavailable;
    if (!std::isfinite(delta_seconds) || delta_seconds < 0 || !std::isfinite(vehicle.cached_speed))
        return Result::invalid_argument;
    compute_pose(vehicle, parameters, delta_seconds, view);
    obstruction_distance_ = 0;
    if (track) {
        const auto forward = original_vehicle_transform({}, frame_.heading_degrees, 0, 0).basis[2];
        const auto radius = length(
            subtract(subtract(frame_.transform.origin, forward), add(frame_.pivot, forward)));
        const auto walls =
            track->nearby_walls(frame_.transform.origin, frame_.transform.origin, radius, true);
        obstruction_distance_ =
            original_camera_wall_distance(frame_.transform.origin, frame_.heading_degrees,
                                          frame_.pivot, parameters.length, walls);
        const auto ground = track->sample(frame_.transform.origin);
        if (ground.hit && frame_.transform.origin.y <= ground.point.y + .5f)
            frame_.transform.origin.y = ground.point.y + .5f;
        if (obstruction_distance_ != 0)
            compute_pose(vehicle, parameters, delta_seconds, view);
    }
    output = frame_;
    return Result::ok;
}

// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004bb8eb
Result OriginalTrackCamera::load(const MeshObject& world, std::string& error) {
    anchors_.clear();
    const std::function<bool(const MeshObject&, Vec3, std::string_view, Vec3&)> find =
        [&](const MeshObject& node, Vec3 parent, std::string_view name, Vec3& found) {
            const auto position = add(parent, node.position);
            if (node.name == name) {
                found = position;
                return true;
            }
            for (const auto& child : node.children)
                if (find(child, position, name, found))
                    return true;
            return false;
        };
    for (unsigned i = 1; i <= 50; ++i) {
        Vec3 position;
        if (!find(world, {}, "Camera" + std::to_string(i), position))
            break;
        anchors_.push_back(position);
    }
    error.clear();
    return Result::ok;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004bb772
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004e401b
Result OriginalTrackCamera::update(Vec3 target, CameraFrame& output) const {
    if (!std::isfinite(target.x) || !std::isfinite(target.y) || !std::isfinite(target.z))
        return Result::invalid_argument;
    Vec3 anchor{};
    float best = 999999;
    if (!anchors_.empty())
        anchor = anchors_.front();
    for (const auto point : anchors_) {
        const float x = target.x - point.x, z = target.z - point.z;
        const float square = x * x + z * z;
        if (square < best) {
            best = square;
            anchor = point;
        }
    }
    auto direction = subtract(anchor, target);
    const float pitch = static_cast<float>(
        -std::atan(static_cast<double>(direction.y + 1) /
                   std::sqrt(static_cast<double>(direction.x * direction.x +
                                                 direction.z * direction.z + 1))) *
        kRadiansToDegrees);
    const float magnitude = length(direction);
    direction = magnitude != 0 ? scale(direction, 1 / magnitude) : Vec3{1, 0, 0};
    float heading =
        static_cast<float>(std::acos(static_cast<double>(direction.z)) * kRadiansToDegrees + 180);
    if (direction.x < 0)
        heading = 360 - heading;
    if (heading < 0)
        heading += 360;
    else if (heading >= 360)
        heading -= 360;
    output = original_camera_frame(anchor, target, heading, pitch);
    return Result::ok;
}

void MouseOrbitCamera::reset() {
    yaw_offset_ = pitch_offset_ = idle_ = 0;
    orbit_heading_ = orbit_pitch_ = radius_ = 0;
    aim_heading_offset_ = aim_pitch_offset_ = 0;
    state_ = MouseOrbitState::original;
}
Result MouseOrbitCamera::update(const CameraFrame& original, float dt, MouseOrbitInput mouse,
                                CameraFrame& output) {
    if (!std::isfinite(dt) || dt < 0 || !std::isfinite(mouse.relative_x) ||
        !std::isfinite(mouse.relative_y) || !std::isfinite(settings_.degrees_per_pixel) ||
        settings_.degrees_per_pixel < 0 || !std::isfinite(settings_.idle_seconds) ||
        settings_.idle_seconds < 0 || !std::isfinite(settings_.return_rate_per_second) ||
        settings_.return_rate_per_second < 0 || !std::isfinite(settings_.minimum_pitch_degrees) ||
        !std::isfinite(settings_.maximum_pitch_degrees) ||
        settings_.minimum_pitch_degrees >= settings_.maximum_pitch_degrees)
        return Result::invalid_argument;
    // This optional mouse feature uses geometric degrees; recovered chase formulas above
    // keep their original conversion constants and behavior.
    constexpr float radians_per_degree = 0.017453292519943295f;
    constexpr float degrees_per_radian = 57.29577951308232f;
    const auto angle_delta = [](float target, float current) {
        return std::remainder(target - current, 360.0f);
    };
    const auto heading = [&](Vec3 forward) {
        return std::atan2(forward.x, forward.z) * degrees_per_radian;
    };
    const auto pitch = [&](Vec3 forward) {
        return std::atan2(forward.y, std::hypot(forward.x, forward.z)) * degrees_per_radian;
    };
    const Vec3 target_eye = subtract(original.transform.origin, original.pivot);
    const float target_radius = length(target_eye);
    if (!std::isfinite(target_radius))
        return Result::invalid_argument;
    const auto target_forward = scale(original.transform.basis[2], -1);
    const float target_heading = heading(scale(target_eye, -1));
    const float target_pitch = pitch(scale(target_eye, -1));
    const float target_view_heading = heading(target_forward);
    const float target_view_pitch = pitch(target_forward);
    const float target_aim_heading = angle_delta(target_view_heading, target_heading);
    const float target_aim_pitch = target_view_pitch - target_pitch;
    if (state_ == MouseOrbitState::original) {
        orbit_heading_ = target_heading;
        orbit_pitch_ = target_pitch;
        radius_ = target_radius;
        aim_heading_offset_ = target_aim_heading;
        aim_pitch_offset_ = target_aim_pitch;
    }
    if (mouse.relative_x != 0 || mouse.relative_y != 0) {
        if (radius_ <= 0)
            return Result::invalid_argument;
        orbit_heading_ =
            std::remainder(orbit_heading_ + mouse.relative_x * settings_.degrees_per_pixel, 360.0f);
        orbit_pitch_ = std::clamp(orbit_pitch_ - mouse.relative_y * settings_.degrees_per_pixel,
                                  settings_.minimum_pitch_degrees, settings_.maximum_pitch_degrees);
        state_ = MouseOrbitState::manual_hold;
        idle_ = 0;
    } else if (state_ != MouseOrbitState::original) {
        float return_seconds = dt;
        if (state_ == MouseOrbitState::manual_hold) {
            const float before = idle_;
            idle_ += dt;
            if (idle_ >= settings_.idle_seconds) {
                state_ = MouseOrbitState::returning;
                return_seconds = std::max(idle_ - std::max(before, settings_.idle_seconds), 0.0f);
            } else
                return_seconds = 0;
        } else
            idle_ += dt;
        if (state_ == MouseOrbitState::returning) {
            const float blend = 1 - std::exp(-settings_.return_rate_per_second * return_seconds);
            // Interpolate on a pivot-centered sphere, never along a chord through the car.
            orbit_heading_ += angle_delta(target_heading, orbit_heading_) * blend;
            orbit_pitch_ += (target_pitch - orbit_pitch_) * blend;
            radius_ += (target_radius - radius_) * blend;
            aim_heading_offset_ += angle_delta(target_aim_heading, aim_heading_offset_) * blend;
            aim_pitch_offset_ += (target_aim_pitch - aim_pitch_offset_) * blend;
            if (std::abs(target_radius - radius_) < .001f &&
                std::abs(angle_delta(target_heading, orbit_heading_)) < .001f &&
                std::abs(target_pitch - orbit_pitch_) < .001f &&
                std::abs(angle_delta(target_aim_heading, aim_heading_offset_)) < .001f &&
                std::abs(target_aim_pitch - aim_pitch_offset_) < .001f)
                state_ = MouseOrbitState::original;
        }
    }
    if (state_ == MouseOrbitState::original) {
        yaw_offset_ = pitch_offset_ = 0;
        output = original;
        return Result::ok;
    }
    const float yaw = orbit_heading_ * radians_per_degree;
    const float elevation = orbit_pitch_ * radians_per_degree;
    const Vec3 relative_eye{-radius_ * std::sin(yaw) * std::cos(elevation),
                            -radius_ * std::sin(elevation),
                            -radius_ * std::cos(yaw) * std::cos(elevation)};
    const float view_heading = std::remainder(orbit_heading_ + aim_heading_offset_, 360.0f);
    const float view_pitch = orbit_pitch_ + aim_pitch_offset_;
    const float h = view_heading * radians_per_degree, p = view_pitch * radians_per_degree;
    const Vec3 forward{std::sin(h) * std::cos(p), std::sin(p), std::cos(h) * std::cos(p)};
    const Vec3 right{std::cos(h), 0, -std::sin(h)};
    output = original;
    output.transform.origin = add(original.pivot, relative_eye);
    output.transform.basis = {scale(right, -1), cross(forward, right), scale(forward, -1)};
    output.heading_degrees = view_heading;
    output.pitch_degrees = view_pitch;
    yaw_offset_ = angle_delta(view_heading, target_view_heading);
    pitch_offset_ = view_pitch - target_view_pitch;
    return Result::ok;
}
} // namespace roadwars
