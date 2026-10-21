#pragma once
#include "game_src/unresolved/resource_store.hpp"

namespace roadwars {
Result load_stock_wheel_scale(ResourceStore& resources, std::string_view vehicle, float& scale_x,
                              std::string& error);
// Caller supplies the original current velocity/forward angle and body-LOD visibility guard.
Result advance_wheel_spin(std::array<float, 4>& pitch_degrees, float cached_speed,
                          float velocity_angle_degrees, float circumference, float brake,
                          const std::array<float, 4>& air_time, float delta_seconds,
                          bool body_spin_enabled = true);
// Tire mesh root pose must be reset on attachment; the original anchor supplies this origin.
Transform original_wheel_transform(std::size_t wheel_index, Vec3 anchor, float steering_degrees,
                                   float spin_degrees, float scale_x);
} // namespace roadwars
