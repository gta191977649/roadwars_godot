#pragma once
#include "game_src/interfaces/host.hpp"

namespace roadwars {
struct TrackAnimation {
    Vec3 angles{}, speeds{};
};
Result parse_track_animation(std::string_view name, TrackAnimation& output);
void advance_track_animation(TrackAnimation& animation, float dt);
Transform track_animation_transform(Vec3 origin, const TrackAnimation& animation);
// New infrastructure for transforming offsets. Original hierarchy Euler angles are summed
// separately; this helper must not replace that original angle accumulation.
Transform compose_track_transform(const Transform& parent, const Transform& local);
} // namespace roadwars
