#include "game_src/unresolved/vehicle_input.hpp"
#include <algorithm>
#include <cmath>

namespace roadwars {
// Keyboard branch of 00424d3e, including the original W-then-S processing order.
DrivingCommand keyboard_driving_command(bool accelerate, bool brake, bool moving, float angle,
                                        float previous_brake, float dt) {
    DrivingCommand command;
    float ramp = previous_brake;
    const auto apply_brake = [&] {
        ramp = std::min(ramp + dt * (ramp < 0.6000000238418579f ? 4.0f : 0.550000011920929f), 1.0f);
        command.brake_amount = ramp;
    };
    if (accelerate) {
        if (!moving || angle < 90.0f)
            command.engine_throttle = 1.0f;
        else
            apply_brake();
    }
    if (brake) {
        if (!moving || angle > 90.0f) {
            command.engine_throttle = -1.0f;
            command.force_first_gear = true;
        } else
            apply_brake();
    }
    return command;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00424d3e
// Keyboard steering subset of the original controller dispatch only.
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00424c8b
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00424ae8
float advance_keyboard_steering(float current, float maximum, int target, float dt) {
    target = std::clamp(target, -1, 1);
    const auto destination = maximum * static_cast<float>(target);
    if (current == destination)
        return current;
    float step = dt * maximum * 4.0f; // float at 005f5128
    if (target && ((target > 0 && current > 0) || (target < 0 && current < 0)) &&
        std::abs(current) > maximum * 0.5f)
        step = dt * maximum;
    if (current > destination)
        step = -step;
    auto next = current + step;
    if ((current < destination && next > destination) ||
        (current > destination && next < destination))
        next = destination;
    return std::clamp(next, -maximum, maximum);
}
} // namespace roadwars
