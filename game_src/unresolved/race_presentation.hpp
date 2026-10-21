#pragma once
#include "game_src/unresolved/mesh_scene.hpp"
#include "game_src/unresolved/original_camera.hpp"
#include "game_src/unresolved/particle_effects.hpp"
#include "game_src/unresolved/race_frontend.hpp"
#include "game_src/unresolved/resource_store.hpp"
#include "game_src/unresolved/skid_marks.hpp"
#include "game_src/unresolved/track_animation.hpp"
#include "game_src/unresolved/track_collision.hpp"
#include "game_src/unresolved/track_render_settings.hpp"
#include "game_src/unresolved/vehicle_dynamics.hpp"
#include <map>

namespace roadwars {
// Host-independent scene presentation; this is not an asserted original class name.
class RacePresentation {
  public:
    explicit RacePresentation(Renderer& renderer) : renderer_(renderer), frontend_(renderer) {}
    ~RacePresentation();
    RacePresentation(const RacePresentation&) = delete;
    RacePresentation& operator=(const RacePresentation&) = delete;
    Result load(ResourceStore& resources, std::string_view track, std::string_view vehicle,
                std::string& error);
    void update_vehicle(Vec3 position, float heading_degrees);
    void update_vehicle(const Transform& pose);
    void update_wheels(const VehicleDynamics&, float delta, float brake, bool reset);
    void update_track_animation(float delta);
    void apply_camera(const CameraFrame& frame);
    Result configure_frontend(ResourceStore&, std::string_view track, std::string_view character,
                              std::string& error);
    Result update_frontend(const VehicleState&);
    const RaceFrontend& frontend() const { return frontend_; }
    void set_developer_build(bool enabled) { developer_build_ = enabled; }
    void configure_view_at(Vec3 eye);
    float camera_far_distance() const { return camera_far_distance_; }
    float view_distance_at(Vec3 eye) {
        return view_distances_.sample(eye) * render_settings_.view_distance_scalar;
    }
    Vec3 camera_position() const { return camera_.transform.origin; }
    const CameraFrame& camera_frame() const { return camera_; }
    void stop();
    Result configure_particles(ResourceStore&, std::uint32_t capacity);
    Result update_particles(std::span<const ParticleRenderState>);
    Result configure_tire_particles(ResourceStore&, std::uint32_t capacity);
    Result update_tire_particles(std::span<const ParticleRenderState>);
    Result configure_skid_marks(ResourceStore&);
    Result update_skid_marks(const std::array<SkidMarkEmitter, 4>&);
    void set_skid_marks_visible(bool);
    TrackCollision& collision() { return collision_; }
    const MeshObject& vehicle_mesh() const { return vehicle_mesh_; }

  private:
    struct ObjectInstance {
        InstanceHandle handle;
        Vec3 local;
        bool vehicle;
        std::uint32_t flags;
        int wheel_index{-1};
        int world_node{-1};
    };
    void upload(ResourceStore&, const MeshObject&, const std::string& base, Vec3 parent,
                bool vehicle, int wheel_index = -1, int parent_node = -1);
    MaterialHandle material(ResourceStore&, const std::string& base, const MeshPart& part,
                            std::uint32_t flags);
    CameraFrame camera_;
    float camera_far_distance_{};
    Renderer& renderer_;
    RaceFrontend frontend_;
    bool developer_build_{};
    TrackCollision collision_;
    TrackRenderSettings render_settings_;
    ViewDistanceMap view_distances_;
    MeshObject world_, vehicle_mesh_;
    std::vector<ObjectInstance> instances_;
    std::vector<std::size_t> vehicle_instances_, animated_instances_, animated_world_nodes_;
    struct WorldNode {
        Vec3 local;
        int parent{-1};
        TrackAnimation animation;
        bool animated{};
        Vec3 world_angles;
        Transform pose;
    };
    std::vector<WorldNode> world_nodes_;
    std::vector<MeshHandle> meshes_;
    std::map<std::string, MaterialHandle> materials_;
    std::vector<TextureHandle> textures_;
    ParticleSystemHandle particles_;
    ParticleSystemHandle tire_particles_;
    std::array<float, 4> wheel_spins_{};
    std::array<Transform, 4> wheel_poses_{};
    float wheel_scale_x_{1};
    MaterialHandle skid_material_;
    struct SkidMesh {
        MeshHandle mesh;
        InstanceHandle instance;
        std::uint64_t revision{};
    };
    std::array<SkidMesh, 20> skid_meshes_{};
};
} // namespace roadwars
