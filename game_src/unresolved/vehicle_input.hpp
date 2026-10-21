#pragma once

namespace roadwars {
struct DrivingCommand {
    float engine_throttle{}, brake_amount{};
    bool force_first_gear{};
};
// Raw keyboard requests are resolved against actual motion, not a speed threshold.
DrivingCommand keyboard_driving_command(bool accelerate, bool brake, bool moving,
                                        float forward_angle_degrees, float previous_brake,
                                        float delta_seconds);
// Keyboard branch only; original LEFT target is -1 and RIGHT target is +1.
// Keep this convention in original simulation coordinates; the renderer adapts handedness.
float advance_keyboard_steering(float current_degrees, float maximum_degrees, int target,
                                float delta_seconds);
} // namespace roadwars
