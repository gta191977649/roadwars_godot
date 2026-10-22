#pragma once
#include "game_src/unresolved/vehicle_dynamics.hpp"

namespace roadwars {
struct CameraFrame {
    Transform transform{};
    Vec3 pivot{};
    float heading_degrees{}, pitch_degrees{}, vertical_fov_degrees{50.0f};
};
struct OriginalCameraSettings {
    Vec3 body_local_position{};
    float hood_height_offset{};
    Vec3 in_car_local_position{}, hood_local_position{}, hood_point_local_position{};
    Vec3 body_local_angles{};
};
// Four-view cycling is a modern presentation extension; values are stable diagnostic IDs.
enum class CameraView { near_chase, far_chase, hood, cockpit };
CameraView next_camera_view(CameraView view);
Result camera_view_frame(CameraView view, const VehicleState& vehicle,
                         const VehicleParameters& parameters, const OriginalCameraSettings& settings,
                         const TrackCollision* track, const CameraFrame& near_frame,
                         CameraFrame& output);
struct OriginalCameraView {
    bool look_back{}, follow_vehicle_heading{}, nitrous_active{}, badlands{};
};
Result load_original_camera_settings(ResourceStore& resources, std::string_view vehicle,
                                     OriginalCameraSettings& settings, std::string& error);
float interpolate_original_camera_angle(float current, float target, float amount);
// This preserves the existing Renderer camera-basis contract in original world coordinates.
CameraFrame original_camera_frame(Vec3 eye, Vec3 pivot, float heading, float pitch);
float original_camera_wall_distance(Vec3 eye, float heading, Vec3 body_world_center,
                                    float vehicle_radius, std::span<const WallRecord> walls);

// Active stock chase modes 4 (velocity-following) and 3 (heading-following).
// Menu, intro, cockpit and replay camera dispatch are explicitly outside this class.
class OriginalChaseCamera {
  public:
    Result load(ResourceStore& resources, std::string_view vehicle, std::string& error);
    void reset();
    Result update(const VehicleState& vehicle, const VehicleParameters& parameters,
                  const TrackCollision* track, float delta_seconds, CameraFrame& output,
                  const OriginalCameraView& view = {});
    const OriginalCameraSettings& settings() const { return settings_; }

  private:
    void compute_pose(const VehicleState&, const VehicleParameters&, float,
                      const OriginalCameraView&);
    OriginalCameraSettings settings_{};
    CameraFrame frame_{};
    bool loaded_{}, initialized_{}, look_back_{};
    float follow_heading_{}, follow_pitch_{}, distance_{6.5f}, distance_offset_{}, height_{};
    float obstruction_distance_{};
};

// Native race-controller mode5 selects nearest track CameraN and camera mode7.
// Original class/source name remains unresolved.
class OriginalTrackCamera {
  public:
    Result load(const MeshObject& world, std::string& error);
    Result update(Vec3 target, CameraFrame& output) const;
    std::span<const Vec3> anchors() const { return anchors_; }

  private:
    std::vector<Vec3> anchors_;
};

// Explicitly new host feature. These values are not reverse-engineered original constants.
struct MouseOrbitSettings {
    float degrees_per_pixel{0.15f};
    float idle_seconds{3.0f};
    float return_rate_per_second{3.0f};
    float minimum_pitch_degrees{-70.0f}, maximum_pitch_degrees{70.0f};
};
struct MouseOrbitInput {
    float relative_x{}, relative_y{};
};
enum class MouseOrbitState { original, manual_hold, returning };
class MouseOrbitCamera {
  public:
    explicit MouseOrbitCamera(MouseOrbitSettings settings = {}) : settings_(settings) {}
    void reset();
    Result update(const CameraFrame& original, float delta_seconds, MouseOrbitInput mouse,
                  CameraFrame& output);
    float yaw_offset_degrees() const { return yaw_offset_; }
    float pitch_offset_degrees() const { return pitch_offset_; }
    float idle_seconds() const { return idle_; }
    MouseOrbitState state() const { return state_; }

  private:
    MouseOrbitSettings settings_;
    float yaw_offset_{}, pitch_offset_{}, idle_{};
    MouseOrbitState state_{MouseOrbitState::original};
    float orbit_heading_{}, orbit_pitch_{}, radius_{};
    float aim_heading_offset_{}, aim_pitch_offset_{};
};
} // namespace roadwars
