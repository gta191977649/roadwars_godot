#include "game_src/unresolved/track_animation.hpp"
#include "game_src/unresolved/contact_orientation.hpp"

namespace roadwars {
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004c7880
// Only the serialized name's angular speeds are recovered here.
Result parse_track_animation(std::string_view name, TrackAnimation& output) {
    constexpr std::string_view prefix = "RotObject_";
    if (!name.starts_with(prefix))
        return Result::not_found;
    name.remove_prefix(prefix.size());
    TrackAnimation parsed;
    float* const axes[]{&parsed.speeds.x, &parsed.speeds.y, &parsed.speeds.z};
    for (int axis = 0; axis < 3; ++axis) {
        const bool negative = name.starts_with('-');
        if (negative)
            name.remove_prefix(1);
        if (name.size() < 3)
            return Result::invalid_argument;
        unsigned value{};
        for (int digit = 0; digit < 3; ++digit) {
            if (name[digit] < '0' || name[digit] > '9')
                return Result::invalid_argument;
            value = value * 10 + static_cast<unsigned>(name[digit] - '0');
        }
        *axes[axis] = negative ? -static_cast<float>(value) : static_cast<float>(value);
        name.remove_prefix(3);
        if (axis < 2) {
            if (!name.starts_with('_'))
                return Result::invalid_argument;
            name.remove_prefix(1);
        }
    }
    output = parsed;
    return Result::ok;
}

// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004c7b1b
void advance_track_animation(TrackAnimation& animation, float dt) {
    if (animation.speeds.x != 0.0f)
        animation.angles.x += animation.speeds.x * dt;
    if (animation.speeds.y != 0.0f)
        animation.angles.y += animation.speeds.y * dt;
    if (animation.speeds.z != 0.0f)
        animation.angles.z += animation.speeds.z * dt;
}

Transform track_animation_transform(Vec3 origin, const TrackAnimation& animation) {
    return original_vehicle_transform(origin, animation.angles.x, animation.angles.y,
                                      animation.angles.z);
}

Transform compose_track_transform(const Transform& parent, const Transform& local) {
    const auto rotate = [&](Vec3 value) -> Vec3 {
        return {parent.basis[0].x * value.x + parent.basis[1].x * value.y +
                    parent.basis[2].x * value.z,
                parent.basis[0].y * value.x + parent.basis[1].y * value.y +
                    parent.basis[2].y * value.z,
                parent.basis[0].z * value.x + parent.basis[1].z * value.y +
                    parent.basis[2].z * value.z};
    };
    Transform result;
    for (std::size_t axis = 0; axis < result.basis.size(); ++axis)
        result.basis[axis] = rotate(local.basis[axis]);
    result.origin = rotate(local.origin);
    result.origin.x += parent.origin.x;
    result.origin.y += parent.origin.y;
    result.origin.z += parent.origin.z;
    return result;
}
} // namespace roadwars
