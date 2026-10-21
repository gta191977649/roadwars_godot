#pragma once
#include "game_src/interfaces/host.hpp"

namespace roadwars {
// Direct3D's left-handed world is retained in the simulation. Reflect Z once at the host.
inline Vec3 original_to_godot(Vec3 value) { return {value.x, value.y, -value.z}; }
inline Transform original_object_to_godot(const Transform& value) {
    Transform result = value;
    result.origin = original_to_godot(value.origin);
    result.basis[0] = original_to_godot(value.basis[0]);
    result.basis[1] = original_to_godot(value.basis[1]);
    const auto z = original_to_godot(value.basis[2]);
    result.basis[2] = {-z.x, -z.y, -z.z};
    return result;
}
inline Transform original_camera_to_godot(const Transform& value) {
    Transform result = value;
    result.origin = original_to_godot(value.origin);
    const auto right = original_to_godot(value.basis[0]);
    result.basis[0] = {-right.x, -right.y, -right.z};
    result.basis[1] = original_to_godot(value.basis[1]);
    result.basis[2] = original_to_godot(value.basis[2]);
    return result;
}
} // namespace roadwars
