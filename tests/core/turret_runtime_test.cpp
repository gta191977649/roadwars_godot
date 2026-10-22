#include "game_src/unresolved/turret_runtime.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>
using namespace roadwars;
namespace {
void require(bool condition) {
    if (!condition)
        throw std::runtime_error("Original turret contract mismatch");
}
} // namespace
int main() {
    TurretParameters parameters;
    require(parse_turret_parameters("Turret_135_Full_G_NOALPHA02", parameters) == Result::ok);
    require(parameters.weapon == "Gatling Gun" && parameters.initial_yaw == 135 &&
            parameters.unrestricted_yaw && parameters.fire_interval == 1);
    require(parse_turret_parameters("Turret_090_070_070_M_NOALPHA", parameters) == Result::ok);
    require(parameters.weapon == "Machine Gun" && parameters.left_limit == 70 &&
            parameters.right_limit == 70 && !parameters.unrestricted_yaw);
    require(parse_turret_parameters("Turret_180_Full_NOALPHA", parameters) == Result::ok);
    require(parameters.weapon == "Rocket Launcher" && parameters.fire_interval == 5);
    require(parse_turret_parameters("Turret_000_Full", parameters) == Result::ok);
    require(parameters.weapon == "Rocket Launcher");
    require(parse_turret_parameters("Turret_270_120_090", parameters) == Result::ok);
    require(parameters.weapon == "Rocket Launcher" && parameters.right_limit == 90);
    require(parse_turret_parameters("RotObject_000_000_010", parameters) == Result::not_found);
    require(parse_turret_parameters("Turret_x00_Full", parameters) == Result::invalid_argument);
    require(parse_turret_parameters("Turret_090_Full_M", parameters) == Result::ok);
    parameters.ammunition_acceleration = 2;
    parameters.barrel_length = 1;
    require(parameters.maximum_range() == 204);
    TurretRuntime turret;
    require(turret.initialize(parameters, {}) == Result::ok);
    std::array<TurretTarget, 6> targets{};
    for (std::size_t i = 0; i < targets.size(); ++i) {
        targets[i].id = i;
        targets[i].position = {100 + static_cast<float>(i), 0, 0};
    }
    targets[0].not_a_turret_target = true;
    targets[1].invulnerable = true;
    targets[2].inactive_timer = .1f;
    targets[3].position = {203.9f, 0, 0};
    targets[4].position = {204, 0, 0};
    targets[5].position = {205, 0, 0};
    std::optional<TurretFireEvent> event;
    require(turret.advance(3, targets, true, event) == Result::ok);
    require(!turret.target() && !event); // The original test is strict timer > 3.
    require(turret.advance(.001f, targets, true, event) == Result::ok);
    require(turret.target() == 3 && turret.velocity_samples() == 1);
    // Range admits this car, but the 1.5-second predictor cannot yet hit it.
    require(turret.predicted_target().x == 0 && !event);
    targets[3].position = {100, 0, 0};
    for (int step = 0; step < 4; ++step)
        require(turret.advance(.1f, targets, true, event) == Result::ok);
    require(turret.velocity_samples() == 5 && turret.aligned() && event);
    require(event->target == 3 && event->speed == 60 && event->predicted_target.x == 100);
    require(std::abs(event->origin.x - 1) < .001f);
    require(turret.advance(.1f, targets, true, event) == Result::ok && !event);
    require(turret.advance(1, targets, true, event) == Result::ok && event);
    require(turret.advance(0, targets, false, event) == Result::ok && !turret.target() && !event);
    // Selection uses strict full 3D distance, not XZ distance or an invented radius.
    targets[3].position = {0, 204, 0};
    require(turret.advance(3.01f, targets, true, event) == Result::ok && !turret.target());
    targets[3].position = {100, 0, 0};
    require(turret.advance(3.01f, targets, true, event) == Result::ok && turret.target() == 3);
    turret.reset();
    require(!turret.target() && turret.heading() == 90);
    require(turret.advance(std::numeric_limits<float>::quiet_NaN(), targets, true, event) ==
            Result::invalid_argument);
    targets[1].id = targets[0].id;
    require(turret.advance(.01f, targets, true, event) == Result::invalid_argument);
    // Runtime-initialized axis0064a1b0 is -X. Positive pitch raises the muzzle.
    require(turret.initialize(parameters, {}) == Result::ok);
    TurretTarget elevated;
    elevated.id = 7;
    elevated.position = {100, 10, 0};
    require(turret.advance(3.01f, {&elevated, 1}, true, event) == Result::ok);
    for (int i = 0; i < 4; ++i)
        require(turret.advance(.1f, {&elevated, 1}, true, event) == Result::ok);
    require(event && event->pitch > 0 && event->origin.y > 0);
    // 0047c5da..e3 returns 360 for exact +Z, while rotation wraps to [0,360).
    // Preserve this original exact-alignment quirk rather than normalizing the desired yaw.
    parameters.initial_yaw = 0;
    require(turret.initialize(parameters, {}) == Result::ok);
    elevated.position = {0, 0, 40};
    for (int i = 0; i < 300; ++i)
        require(turret.advance(1.0f / 60, {&elevated, 1}, true, event) == Result::ok && !event);
    require(turret.target_heading() == 360 && !turret.aligned() && !event);
}
