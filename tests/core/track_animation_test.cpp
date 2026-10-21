#include "game_src/unresolved/track_animation.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>
using namespace roadwars;
namespace {
void require(bool condition) {
    if (!condition)
        throw std::runtime_error("Track animation contract failed");
}
bool near(float a, float b) { return std::abs(a - b) < 0.001f; }
} // namespace
int main() {
    TrackAnimation animation;
    require(parse_track_animation("RotObject_004_000_000", animation) == Result::ok);
    require(animation.speeds.x == 4 && animation.speeds.y == 0 && animation.speeds.z == 0);
    require(parse_track_animation("RotObject_-010_025_-999C", animation) == Result::ok);
    require(animation.speeds.x == -10 && animation.speeds.y == 25 && animation.speeds.z == -999);
    require(parse_track_animation("RotObject_000_100_000_ALPHA", animation) == Result::ok);
    require(animation.speeds.y == 100);
    for (const auto name : {"RotObject_01_000_000", "RotObject_010_00x_000",
                           "RotObject_010-000_000", "RotObject_+010_000_000",
                           "RotObject_010_000_", "RotObject_010_000_--10"}) {
        require(parse_track_animation(name, animation) == Result::invalid_argument);
        require(animation.speeds.y == 100);
    }
    require(parse_track_animation("512", animation) == Result::not_found);
    animation = {{359, -0.0f, 20}, {4, 0, -25}};
    advance_track_animation(animation, 2);
    require(animation.angles.x == 367 && animation.angles.z == -30);
    require(std::signbit(animation.angles.y));
    animation.angles.y = 5;
    advance_track_animation(animation, std::numeric_limits<float>::infinity());
    require(animation.angles.y == 5);

    // An offset mesh must rotate about its object's origin, then inherit its parent.
    animation = {{90, 0, 0}, {}};
    const auto local = track_animation_transform({10, 2, 3}, animation);
    require(local.origin.x == 10 && local.origin.y == 2 && local.origin.z == 3);
    require(near(local.basis[2].x, 1) && near(local.basis[2].z, 0));
    Transform parent;
    parent.origin = {100, 200, 300};
    const auto world = compose_track_transform(parent, local);
    require(world.origin.x == 110 && world.origin.y == 202 && world.origin.z == 303);
    Transform offset;
    offset.origin = {0, 0, 4};
    const auto child = compose_track_transform(world, offset);
    require(near(child.origin.x, 114) && near(child.origin.y, 202) && near(child.origin.z, 303));
}
