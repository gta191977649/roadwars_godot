#include "game_src/unresolved/contact_orientation.hpp"
#include "platform/godot/rendering/coordinate_conversion.hpp"
#include <cassert>
#include <cmath>
using namespace roadwars;
float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
int main() {
    Transform camera;
    camera.origin = {0, 3, -8};
    camera.basis = {{{-1, 0, 0}, {0, 1, 0}, {0, 0, -1}}};
    const auto native = original_camera_to_godot(camera);
    assert(native.origin.z == 8 && native.basis[0].x == 1 && native.basis[2].z == 1);
    const auto right = original_to_godot(Vec3{1, 0, 0});
    const auto left = original_to_godot(Vec3{-1, 0, 0});
    assert(dot(right, native.basis[0]) > 0 && dot(left, native.basis[0]) < 0);
    const auto turn = original_object_to_godot(original_vehicle_transform({}, 30, 0, 0));
    // Original +Z local front becomes Godot -Z; right turn stays image-right.
    Vec3 forward{-turn.basis[2].x, -turn.basis[2].y, -turn.basis[2].z};
    assert(dot(forward, native.basis[0]) > 0 && forward.z < 0);
    const auto original = original_vehicle_transform({1, 2, 3}, 37, 14, -9);
    const auto converted = original_object_to_godot(original);
    const auto roundtrip = original_object_to_godot(converted);
    assert(roundtrip.origin.x == original.origin.x && roundtrip.origin.z == original.origin.z);
    for (int i = 0; i < 3; ++i)
        assert(roundtrip.basis[i].x == original.basis[i].x &&
               roundtrip.basis[i].z == original.basis[i].z);
}
