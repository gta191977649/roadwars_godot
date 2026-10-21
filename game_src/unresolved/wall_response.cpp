#include "game_src/unresolved/wall_response.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>

namespace roadwars {
namespace {
float length(Vec3 value) {
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}
Vec3 scale(Vec3 value, float scalar) {
    return {value.x * scalar, value.y * scalar, value.z * scalar};
}
} // namespace
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0042e8f0
// Physics response subset only; full original dispatch and side effects remain unresolved.
void apply_wall_response(const VehicleParameters& p, VehicleState& state,
                         const WallContact& contact, float delta_seconds) {
    if (contact.restore_previous) {
        state.position = contact.position;
        state.heading_degrees = contact.heading_degrees;
        state.velocity = {};
        return;
    }
    if (!contact.hit)
        return;
    // The original geometric solver has already committed this clear pose before response.
    state.position = contact.position;
    state.heading_degrees = contact.heading_degrees;
    Vec3 normal{contact.normal.x, 0, contact.normal.z};
    assert(length(normal) > 0 && "Resolved wall contact must provide a nonzero XZ normal");
    const float speed = state.cached_speed;
    const Vec3 horizontal_velocity{state.velocity.x, 0, state.velocity.z};
    const bool moving = state.velocity.x != 0 || state.velocity.z != 0;
    float angle_degrees{};
    float incidence{};
    Vec3 force = normal;
    if (moving) {
        const float dot = normal.x * horizontal_velocity.x + normal.z * horizontal_velocity.z;
        const float cosine =
            std::clamp(dot / (length(normal) * length(horizontal_velocity)), -1.0f, 1.0f);
        const double radians = std::acos(static_cast<double>(cosine));
        constexpr float kRadToDeg = 57.288352966308594f;
        constexpr float kDegToRad = 0.017455555498600006f;
        angle_degrees = static_cast<float>(radians * kRadToDeg);
        // 00517924 is FCOS, confirmed by disassembly; this is not a sine impulse.
        incidence = static_cast<float>(std::abs(std::cos(radians * kRadToDeg * kDegToRad)));
        force = scale(force, 2.0f * speed * p.mass_kg * incidence);
    }
    if (!moving || angle_degrees < 90.0f) {
        const float magnitude = length(normal);
        if (magnitude != 0)
            normal = scale(normal, 1.0f / magnitude);
        force = scale(normal, p.mass_kg * 3.0f);
        apply_vehicle_impulse(state.velocity, force, 1.0f, p.inverse_mass, false);
        apply_vehicle_yaw_impulse(p, state, force, contact.contact_point, 1.0f, false, -1.0f);
    } else {
        apply_vehicle_impulse(state.velocity, force, 1.0f, p.inverse_mass, false);
        apply_vehicle_yaw_impulse(p, state, force, contact.contact_point, 1.0f, false, -1.0f);
        state.velocity = scale(state.velocity, (1.0f - incidence) * 1.0f);
        state.yaw_rate *= 0.30000001192092896f;
    }
    const float remaining_seconds = contact.removed_fraction * delta_seconds;
    const Vec3 displacement = scale(state.velocity, remaining_seconds);
    state.position.x += displacement.x;
    state.position.y += displacement.y;
    state.position.z += displacement.z;
    state.heading_degrees += remaining_seconds * state.yaw_rate;
    if (state.heading_degrees < 0)
        state.heading_degrees += 360;
    if (state.heading_degrees >= 360)
        state.heading_degrees -= 360;
}
} // namespace roadwars
