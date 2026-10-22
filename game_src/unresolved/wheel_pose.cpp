#include "game_src/unresolved/wheel_pose.hpp"
#include "game_src/unresolved/contact_orientation.hpp"
#include <cmath>

namespace roadwars {
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00412834
// Stock Street/Poor rendering subset; purchase, LOD and upgrade transitions remain outside it.
Result load_stock_wheel_scale(ResourceStore& resources, std::string_view vehicle, float& scale_x,
                              std::string& error) {
    Configuration config;
    const auto result =
        config.load(resources, "Cars/" + std::string(vehicle) + "/Tires/Street/Poor/Data.ini");
    if (result != Result::ok) {
        error = "Stock tire scale configuration unavailable";
        return result;
    }
    float scale{};
    if (config.number("SCALE", "SCALEX", scale) != Result::ok || scale <= 0) {
        error = "Invalid original tire X scale";
        return Result::invalid_argument;
    }
    scale_x = scale;
    error.clear();
    return Result::ok;
}

// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00428cb6
// The caller owns body-LOD resolution and velocity-angle cache selection.
Result advance_wheel_spin(std::array<float, 4>& pitch_degrees, float cached_speed,
                          float velocity_angle_degrees, float circumference, float brake,
                          const std::array<float, 4>& air_time, float delta_seconds,
                          bool body_spin_enabled) {
    if (!std::isfinite(cached_speed) || cached_speed < 0 ||
        !std::isfinite(velocity_angle_degrees) || velocity_angle_degrees < 0 ||
        velocity_angle_degrees > 180 || !std::isfinite(circumference) || circumference <= 0 ||
        // 0041aa67 can emit .99-target/speed in [-.01,0) near the target speed.
        !std::isfinite(brake) || brake < -.01f || brake > 1 || !std::isfinite(delta_seconds) ||
        delta_seconds < 0)
        return Result::invalid_argument;
    if (!body_spin_enabled || cached_speed == 0 ||
        (velocity_angle_degrees > 45 && velocity_angle_degrees < 135))
        return Result::ok;
    constexpr float kDegreesToRadians = 0.017455555498600006f;
    const float projected =
        static_cast<float>(
            std::abs(std::cos(static_cast<double>(velocity_angle_degrees * kDegreesToRadians)))) *
        cached_speed;
    const float degrees_per_second = (projected / circumference) * 360.0f;
    const float braking_factor = 1.0f - brake;
    for (std::size_t i = 0; i < 4; ++i) {
        if (air_time[i] != 0)
            continue;
        // Non-root pitch setters preserve the unbounded wheel angle; no chassis +/-60 clamp.
        if (velocity_angle_degrees >= 90)
            pitch_degrees[i] = static_cast<float>(static_cast<double>(degrees_per_second) *
                                                      delta_seconds * braking_factor +
                                                  pitch_degrees[i]);
        else
            pitch_degrees[i] =
                static_cast<float>(pitch_degrees[i] - static_cast<double>(degrees_per_second) *
                                                          delta_seconds * braking_factor);
    }
    return Result::ok;
}

// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00412834
// Matrix form of the evidenced attachment, scale, right-side X/Z mirroring and wheel angles.
Transform original_wheel_transform(std::size_t wheel_index, Vec3 anchor, float steering_degrees,
                                   float spin_degrees, float scale_x) {
    Transform pose =
        original_vehicle_transform(anchor, wheel_index < 2 ? steering_degrees : 0, spin_degrees, 0);
    const bool right_side = wheel_index == 1 || wheel_index == 3;
    const float x_scale = right_side ? -scale_x : scale_x;
    pose.basis[0] = {pose.basis[0].x * x_scale, pose.basis[0].y * x_scale,
                     pose.basis[0].z * x_scale};
    if (right_side)
        pose.basis[2] = {-pose.basis[2].x, -pose.basis[2].y, -pose.basis[2].z};
    return pose;
}
} // namespace roadwars
