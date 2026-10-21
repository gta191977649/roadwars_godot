#include "game_src/unresolved/vehicle_dynamics.hpp"
#include "game_src/unresolved/vehicle_input.hpp"
#include <cassert>
#include <cmath>
using namespace roadwars;
namespace {
bool close(float a, float b) { return std::abs(a - b) < 0.0001f; }
} // namespace
int main() {
    VehicleParameters p;
    p.mass_kg = 1000;
    p.torque_nm = 400;
    p.frontal_area_m2 = 2;
    p.drag_coefficient = .3f;
    p.rear_end_ratio = 3;
    p.gear_ratios = {2};
    p.rear_wheel_radius = .5f;
    p.length = 4;
    p.width = 2;
    p.height = 1;
    p.wheelbase = 2.5f;
    assert(finalize_vehicle_parameters(p) == Result::ok);
    assert(close(p.inverse_mass, .001f));
    assert(close(p.yaw_inertia, 5000));
    assert(close(vehicle_drive_force(p, 0, 1, 10000), 4800));
    assert(close(vehicle_drive_force(p, 0, 1, 3000), 3000));
    const auto grip = vehicle_force_limits(p, {1, 2, 3, 4}, {2, 4, 6, 8}, {.1f, .2f, .3f, .4f});
    assert(std::abs(grip.traction - 34300.0f) < .01f);
    assert(std::abs(grip.partial_brake - 14700.0f) < .01f);
    assert(std::abs(grip.full_brake - 29400.0f) < .01f);
    assert(std::abs(grip.lateral - 12250.0f) < .01f);
    assert(std::abs(grip.rolling[2] - 735.0f) < .01f);
    Vec3 velocity{0, 0, 1};
    apply_vehicle_impulse(velocity, {0, 0, -1}, 2000, p.inverse_mass, true);
    assert(velocity.x == 0 && velocity.y == 0 && velocity.z == 0);
    float rate = 1;
    apply_angular_delta(rate, -2, true);
    assert(rate == -2); // Original sequential crossing tests deliberately preserved.
    VehicleState state;
    apply_vehicle_engine(p, state, {0, 0, 1}, 1, 1000, .1f);
    assert(close(state.velocity.z, .1f));
    state = {};
    state.air_time[2] = 1;
    apply_vehicle_engine(p, state, {0, 0, 1}, 1, 1000, .1f);
    assert(close(state.velocity.z, .05f));
    state = {};
    state.air_time[2] = state.air_time[3] = 1;
    apply_vehicle_engine(p, state, {0, 0, 1}, 1, 1000, .1f);
    assert(state.velocity.z == 0);
    state = {};
    state.velocity = {0, 0, 10};
    state.cached_speed = 10;
    state.air_time = {1, 1, 1, 1};
    assert(std::abs(vehicle_rpm(p, state, 0, 1) - 3145.9156f) < .001f);
    state.air_time = {0, 1, 1, 1};
    assert(std::abs(vehicle_rpm(p, state, 0, 1) - 1145.9156f) < .001f);
    // Independently calculated 400Nm *3*2 /.5m =4800N; dv=F/m*dt.
    state = {};
    state.rpm = 6000;
    apply_vehicle_engine(p, state, {0, 0, 1}, 1, 4800, .1f);
    assert(close(state.velocity.z, .336f)); // Original RPM factor:1.7-6000/6000=.7.
    state = {};
    apply_vehicle_engine(p, state, {0, 0, 1}, -1, 4800, .1f);
    assert(close(state.velocity.z, -.48f));
    const auto reverse = keyboard_driving_command(false, true, false, 90, 0, .1f);
    assert(reverse.engine_throttle == -1 && reverse.force_first_gear && reverse.brake_amount == 0);
    const auto braking = keyboard_driving_command(false, true, true, 0, 0, .1f);
    assert(braking.engine_throttle == 0 && close(braking.brake_amount, .4f));
    const auto slower_ramp = keyboard_driving_command(false, true, true, 0, .6f, .1f);
    assert(close(slower_ramp.brake_amount, .655f));
    const auto backward = keyboard_driving_command(false, true, true, 180, 1, .1f);
    assert(backward.engine_throttle == -1 && backward.brake_amount == 0);
    assert(keyboard_driving_command(true, false, true, 180, 0, .1f).brake_amount > 0);
    assert(keyboard_driving_command(true, true, false, 0, 0, .1f).engine_throttle == -1);
    assert(keyboard_driving_command(true, true, true, 90, 0, .1f).brake_amount == .8f);
    p.gear_ratios = {2, 1.5f, 1};
    state = {};
    state.rpm = 6000;
    update_automatic_transmission(p, state);
    assert(state.gear_index == 0 && !state.upshifted);
    state.rpm = 6001;
    update_automatic_transmission(p, state);
    assert(state.gear_index == 1 && state.upshifted);
    state.rpm = 3500;
    state.engine_throttle = -1;
    update_automatic_transmission(p, state);
    assert(state.gear_index == 1 && !state.upshifted);
    state.engine_throttle = 1;
    update_automatic_transmission(p, state);
    assert(state.gear_index == 0);
    state.gear_index = 2;
    state.rpm = 2999;
    state.engine_throttle = 0;
    update_automatic_transmission(p, state);
    assert(state.gear_index == 1);
    state = {};
    state.velocity = {0, 0, 10};
    state.cached_speed = 10;
    apply_vehicle_drag(p, state, {0, -1, 0}, .1f);
    assert(state.velocity.y < 0 && state.velocity.z < 10);
}
