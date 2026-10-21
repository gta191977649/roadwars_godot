#include "game_src/unresolved/contact_orientation.hpp"
#include "game_src/unresolved/collision_audio.hpp"
#include "game_src/unresolved/vehicle_dynamics.hpp"
#include <algorithm>
#include <cmath>

namespace roadwars {
namespace {
constexpr float kDegreesToRadians = 0.017455555498600006f;
constexpr float kRadiansToDegrees = 57.288352966308594f;
float inclination(Vec3 a, Vec3 b) {
    const Vec3 v{a.x - b.x, a.y - b.y, a.z - b.z};
    const float flat_squared = v.x * v.x + v.z * v.z;
    const float dot = flat_squared;
    const float length = std::sqrt(flat_squared + v.y * v.y);
    const float flat_length = std::sqrt(flat_squared);
    float cosine = dot / (length * flat_length);
    // Preserve the original comparison ordering, including its NaN handling.
    if (cosine <= 1.0f) {
        if (cosine < -1.0f)
            cosine = -1.0f;
    } else {
        cosine = 1.0f;
    }
    float angle = static_cast<float>(std::acos(static_cast<double>(cosine)) * kRadiansToDegrees);
    if (a.y < b.y)
        angle = -angle;
    return angle;
}
} // namespace

// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0042a11c
void update_wheel_suspension(const VehicleParameters& parameters, VehicleState& vehicle,
                             ContactOrientationState& body, std::array<float, 4>& local_heights,
                             const std::array<Vec3, 4>& wheel_points,
                             const std::array<GroundContact, 4>& contacts,
                             CollisionAudio* collision_audio) {
    for (std::size_t i = 0; i < 4; ++i) {
        const float desired = local_heights[i] - (wheel_points[i].y - contacts[i].point.y);
        const float lower = parameters.wheel_ground_anchors[i].y - 0.1f;
        const float upper = parameters.wheel_ground_anchors[i].y + 0.1f;
        if (desired < lower) {
            local_heights[i] = lower;
            continue;
        }
        local_heights[i] = std::min(desired, upper);
        if (vehicle.air_time[i] != 0.0f) {
            if (collision_audio)
                collision_audio->wheel_landing(vehicle.air_time[i], vehicle.position);
            const Vec3 v = vehicle.velocity, n = contacts[i].normal;
            const float speed = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
            const float normal_length = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
            float cosine = (v.x * n.x + v.y * n.y + v.z * n.z) / (speed * normal_length);
            if (cosine <= 1.0f) {
                if (cosine < -1.0f)
                    cosine = -1.0f;
            } else
                cosine = 1.0f;
            const float angle =
                static_cast<float>(std::acos(static_cast<double>(cosine)) * kRadiansToDegrees);
            if (angle > 90.0f) {
                const float projected = static_cast<float>(
                    std::abs(std::cos(static_cast<double>(angle * kDegreesToRadians))));
                const float damping = std::min(1.0f, (1.0f - projected) + 0.25f);
                vehicle.velocity = {v.x * damping, v.y * damping, v.z * damping};
                vehicle.pitch_rate *= 0.8f;
                vehicle.roll_rate *= 0.8f;
                const float strength = std::min(angle - 90.0f, 15.0f) / 15.0f;
                body.pitch_target = (i < 2 ? 10.0f : -10.0f) * strength;
                body.roll_target = (i == 0 || i == 2 ? -10.0f : 10.0f) * strength;
            }
            vehicle.air_time[i] = 0.0f;
        }
    }
    // The original next calls 00429bdf for chassis front/rear ground strikes.
    // This helper implements wheel suspension only; its caller owns that boundary.
}

// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004286e0
void update_body_spring(float& angle, float& target, float& history, float dt) {
    if (target == 0.0f) {
        if (history != 0.0f) {
            if (angle >= 0.0f) {
                if (angle > 0.0f) {
                    angle -= 45.0f * dt;
                    if (angle < 0.0f) {
                        if (std::abs(history) < 1.0f) {
                            angle = 0.0f;
                            history = 0.0f;
                        }
                        target = history * 0.5f;
                    }
                }
            } else {
                angle = 45.0f * dt + angle;
                if (angle > 0.0f) {
                    if (std::abs(history) < 1.0f) {
                        angle = 0.0f;
                        history = 0.0f;
                    }
                    target = history * 0.5f;
                }
            }
        }
    } else if (target <= angle) {
        if (target < angle) {
            angle -= 45.0f * dt;
            if (angle < target) {
                history = target;
                target = 0.0f;
            }
        }
    } else {
        angle = 45.0f * dt + angle;
        if (target < angle) {
            history = target;
            target = 0.0f;
        }
    }
}

// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004d3dbd
Transform original_vehicle_transform(Vec3 position, float heading, float pitch, float roll) {
    const float h = heading * kDegreesToRadians, p = -pitch * kDegreesToRadians;
    const float r = roll * kDegreesToRadians;
    const float ch = std::cos(h), sh = std::sin(h), cp = std::cos(p), sp = std::sin(p);
    const float cr = std::cos(r), sr = std::sin(r);
    // Original row-vector matrix product Rx(-pitch) * Rz(roll) * Ry(heading).
    const std::array<std::array<float, 3>, 3> x{{{1, 0, 0}, {0, cp, sp}, {0, -sp, cp}}};
    const std::array<std::array<float, 3>, 3> z{{{cr, sr, 0}, {-sr, cr, 0}, {0, 0, 1}}};
    const std::array<std::array<float, 3>, 3> y{{{ch, 0, -sh}, {0, 1, 0}, {sh, 0, ch}}};
    std::array<std::array<float, 3>, 3> xz{}, result{};
    for (std::size_t i = 0; i < 3; ++i)
        for (std::size_t j = 0; j < 3; ++j)
            xz[i][j] = x[i][1] * z[1][j] + x[i][0] * z[0][j] + x[i][2] * z[2][j];
    for (std::size_t i = 0; i < 3; ++i)
        for (std::size_t j = 0; j < 3; ++j)
            result[i][j] = xz[i][1] * y[1][j] + xz[i][0] * y[0][j] + xz[i][2] * y[2][j];
    Transform output;
    output.origin = position;
    for (std::size_t i = 0; i < 3; ++i)
        output.basis[i] = {result[i][0], result[i][1], result[i][2]};
    return output;
}

// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00429bdf
void update_chassis_ground_strikes(const VehicleParameters& parameters, VehicleState& vehicle,
                                   const std::array<Vec3, 4>& wheel_points,
                                   const TrackCollision& track, Vec3 body_world_center) {
    const float pitch = vehicle.pitch_degrees * kDegreesToRadians;
    const float heading = vehicle.heading_degrees * kDegreesToRadians;
    // 0064a160=(0,0,1), pitch axis0064a1b0=(-1,0,0), yawaxis0064a170=(0,1,0).
    const Vec3 forward{std::sin(heading) * std::cos(pitch), std::sin(pitch),
                       std::cos(heading) * std::cos(pitch)};
    const float half = parameters.length / 2.0f;
    const std::array<Vec3, 2> endpoints{
        {{body_world_center.x + forward.x * half, body_world_center.y + forward.y * half,
          body_world_center.z + forward.z * half},
         {body_world_center.x - forward.x * half, body_world_center.y - forward.y * half,
          body_world_center.z - forward.z * half}}};
    const float cached_speed = vehicle.cached_speed;
    for (std::size_t i = 0; i < 2; ++i) {
        const Vec3 endpoint = endpoints[i];
        const auto contact = track.sample(endpoint);
        if (!contact.hit || contact.point.y < endpoint.y)
            continue;
        const Vec3 v = vehicle.velocity, n = contact.normal;
        const float speed = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        const float normal_length = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
        float cosine = (v.x * n.x + v.y * n.y + v.z * n.z) / (speed * normal_length);
        if (cosine <= 1.0f) {
            if (cosine < -1.0f)
                cosine = -1.0f;
        } else
            cosine = 1.0f;
        const float angle =
            static_cast<float>(std::acos(static_cast<double>(cosine)) * kRadiansToDegrees);
        const float projection =
            static_cast<float>(std::abs(std::cos(static_cast<double>(angle * kDegreesToRadians))));
        const float impulse =
            (cached_speed * projection * (2.0f * parameters.mass_kg)) / (i == 0 ? 8.5f : 4.0f);
        apply_vehicle_impulse(vehicle.velocity, n, impulse, parameters.inverse_mass, false);
        apply_vehicle_tilt(parameters, vehicle, wheel_points, n, endpoint, impulse, true, false,
                           true);
        vehicle.position.y += contact.point.y - endpoint.y;
    }
}

// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00429a7e
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0042988c
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00429714
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004292fe
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00429507
Result update_contact_orientation(VehicleState& vehicle, ContactOrientationState& body,
                                  const std::array<Vec3, 4>& wheel_points,
                                  const std::array<GroundContact, 4>& contacts, float dt) {
    if (!std::isfinite(dt) || dt < 0.0f)
        return Result::invalid_argument;
    for (std::size_t i = 0; i < 4; ++i) {
        const Vec3 a = wheel_points[i], b = contacts[i].point;
        if (b.y < a.y) {
            const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
            if (std::sqrt(dx * dx + dy * dy + dz * dz) > 0.1f)
                vehicle.air_time[i] += dt;
        }
    }
    if (contacts[0].point.y == -99999.0f)
        body.missing_ground_time += dt;
    if (body.missing_ground_time >= 1.0f)
        return Result::unavailable;
    float height = 0.0f;
    for (std::size_t i = 0; i < 4; ++i)
        height += std::max(wheel_points[i].y, contacts[i].point.y);
    vehicle.position.y = height * 0.25f;
    const auto grounded = [&](std::size_t a, std::size_t b) {
        return vehicle.air_time[a] == 0.0f && vehicle.air_time[b] == 0.0f;
    };
    const float left =
        grounded(0, 2) ? inclination(contacts[0].point, contacts[2].point) : vehicle.pitch_degrees;
    const float right =
        grounded(1, 3) ? inclination(contacts[1].point, contacts[3].point) : vehicle.pitch_degrees;
    vehicle.pitch_degrees = (left + right) * 0.5f;
    if (vehicle.pitch_degrees > 60.0f) {
        vehicle.pitch_degrees = 60.0f;
        vehicle.pitch_rate = 0;
    } else if (vehicle.pitch_degrees < -60.0f) {
        vehicle.pitch_degrees = -60.0f;
        vehicle.pitch_rate = 0;
    }
    const float rear =
        grounded(2, 3) ? -inclination(contacts[2].point, contacts[3].point) : vehicle.roll_degrees;
    const float front =
        grounded(0, 1) ? -inclination(contacts[0].point, contacts[1].point) : vehicle.roll_degrees;
    vehicle.roll_degrees = (rear + front) * 0.5f;
    if (vehicle.roll_degrees > 60.0f) {
        vehicle.roll_degrees = 60.0f;
        vehicle.roll_rate = 0;
    } else if (vehicle.roll_degrees < -60.0f) {
        vehicle.roll_degrees = -60.0f;
        vehicle.roll_rate = 0;
    }
    if (std::all_of(vehicle.air_time.begin(), vehicle.air_time.end(),
                    [](float t) { return t == 0.0f; })) {
        vehicle.pitch_rate = 0.0f;
        vehicle.roll_rate = 0.0f;
    }
    update_body_spring(body.body_pitch, body.pitch_target, body.pitch_history, dt);
    update_body_spring(body.body_roll, body.roll_target, body.roll_history, dt);
    return Result::ok;
}
} // namespace roadwars
