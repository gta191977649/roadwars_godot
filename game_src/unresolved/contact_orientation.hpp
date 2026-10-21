#pragma once
#include "game_src/unresolved/track_collision.hpp"

namespace roadwars {
struct VehicleState;
struct VehicleParameters;
class CollisionAudio;
struct ContactOrientationState {
    float body_pitch{}, pitch_target{}, pitch_history{};
    float body_roll{}, roll_target{}, roll_history{};
    float missing_ground_time{};
};
void update_body_spring(float& angle, float& target, float& history, float dt);
// Local wheel heights persist between frames; initialize from wheel_ground_anchors.
void update_wheel_suspension(const VehicleParameters& parameters, VehicleState& vehicle,
                             ContactOrientationState& body, std::array<float, 4>& local_heights,
                             const std::array<Vec3, 4>& wheel_points,
                             const std::array<GroundContact, 4>& contacts,
                             CollisionAudio* collision_audio = nullptr);
Transform original_vehicle_transform(Vec3 position, float heading, float pitch, float roll);
void update_chassis_ground_strikes(const VehicleParameters& parameters, VehicleState& vehicle,
                                   const std::array<Vec3, 4>& wheel_points,
                                   const TrackCollision& track, Vec3 body_world_center);
// Wheel points must be recomputed from the pre-contact vehicle transform each frame.
// Returns unavailable when the original missing-ground respawn path is required.
Result update_contact_orientation(VehicleState& vehicle, ContactOrientationState& body,
                                  const std::array<Vec3, 4>& wheel_points,
                                  const std::array<GroundContact, 4>& contacts, float dt);
} // namespace roadwars
