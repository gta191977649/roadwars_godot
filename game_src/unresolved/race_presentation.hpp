#pragma once
#include "game_src/unresolved/mesh_scene.hpp"
#include "game_src/unresolved/original_camera.hpp"
#include "game_src/unresolved/particle_effects.hpp"
#include "game_src/unresolved/race_frontend.hpp"
#include "game_src/unresolved/race_turrets.hpp"
#include "game_src/unresolved/resource_store.hpp"
#include "game_src/unresolved/skid_marks.hpp"
#include "game_src/unresolved/track_animation.hpp"
#include "game_src/unresolved/track_collision.hpp"
#include "game_src/unresolved/track_render_settings.hpp"
#include "game_src/unresolved/vehicle_dynamics.hpp"
#include <map>
#include <memory>
#include <optional>

namespace roadwars {
class RaceSession;
class SkeletalMeshRuntime;
class VehicleDamageVisuals;
// Host-independent scene presentation; this is not an asserted original class name.
class RacePresentation {
  public:
    explicit RacePresentation(Renderer& renderer) : renderer_(renderer), frontend_(renderer) {}
    ~RacePresentation();
    RacePresentation(const RacePresentation&) = delete;
    RacePresentation& operator=(const RacePresentation&) = delete;
    Result load(ResourceStore& resources, std::string_view track, std::string_view vehicle,
                std::string& error, const TireSelection& tires = {});
    void update_vehicle(Vec3 position, float heading_degrees);
    void update_vehicle(const Transform& pose);
    void set_camera_vehicle_hidden(int target, std::size_t player_index);
    void snap_racer(std::size_t index, std::size_t player_index);
    void update_wheels(const VehicleDynamics&, float delta, float brake, bool reset);
    void update_lights(const VehicleState&);
    void update_track_animation(float delta);
    Result update_turrets(std::span<const RaceTurretPose>);
    Result add_combat_visual(ResourceStore&, const MeshObject&, std::string_view resource_base,
                             std::size_t& id, std::string& error);
    Result update_combat_visual(std::size_t id, const Transform&, bool visible);
    Result update_combat(ResourceStore&, const RaceSession&, std::string& error);
    Result prepare_combat(ResourceStore&, const RaceSession&, std::string& error);
    const std::string& combat_render_status() const { return combat_render_status_; }
    void apply_camera(const CameraFrame& frame);
    Result configure_frontend(ResourceStore&, std::string_view track, std::string_view character,
                              std::string& error);
    Result update_frontend(const VehicleState&);
    Result configure_racers(ResourceStore&, std::string_view track,
                            std::span<const std::string> characters, std::string& error);
    Result configure_equipment_hud(ResourceStore& resources, std::string_view car,
                                   const std::array<std::string, 3>& weapons, std::string& error) {
        return frontend_.configure_equipment(resources, car, weapons, error);
    }
    Result update_frontend(const FrontendFrame& frame) { return frontend_.update(frame); }
    Result add_opponent(ResourceStore&, std::string_view vehicle, std::string& error,
                        const TireSelection& tires = {}, int paint_job = 0);
    Result replace_player_visual(ResourceStore&, std::string_view vehicle, const RaceLoadout&,
                                 std::string& error);
    void update_opponent(std::size_t, const VehicleDynamics&, float delta, bool reset = false);
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
        MeshHandle mesh;
        std::vector<std::size_t> path;
        std::size_t part_index{};
        std::string node_name;
    };
    void upload(ResourceStore&, const MeshObject&, const std::string& base, Vec3 parent,
                bool vehicle, int wheel_index = -1, int parent_node = -1,
                std::vector<std::size_t> node_path = {});
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
    std::vector<InstanceHandle> brake_lights_;
    bool brake_lights_visible_{};
    struct WorldNode {
        Vec3 local;
        int parent{-1};
        TrackAnimation animation;
        std::vector<std::size_t> path;
        Vec3 turret_angles;
        bool turret{};
        bool animated{};
        Vec3 world_angles;
        Transform pose;
    };
    struct VehicleVisual {
        std::vector<std::size_t> instances;
        std::vector<InstanceHandle> lights;
        bool lights_visible{};
        std::array<float, 4> spins{};
        std::array<Transform, 4> wheels{};
        float wheel_scale{1};
        std::shared_ptr<const MeshObject> hierarchy;
        std::optional<Vec3> vehicle_angles;
        Vec3 body_angles{};
    };
    void swap_visual(VehicleVisual&);
    Result load_vehicle_visual(ResourceStore&, std::string_view vehicle, std::string& error,
                               const TireSelection& tires, int paint_job = 0);
    std::shared_ptr<const MeshObject> vehicle_hierarchy_;
    std::optional<Vec3> vehicle_angles_;
    Vec3 body_angles_{};
    std::vector<VehicleVisual> opponents_;
    std::vector<VehicleVisual> combat_visuals_;
    std::map<std::array<std::size_t, 4>, std::size_t> combat_visual_ids_;
    struct CombatParticleVisual {
        ParticleSystemHandle handle;
        std::size_t capacity{};
    };
    std::map<std::array<std::size_t, 5>, CombatParticleVisual> combat_particle_ids_;
    std::string combat_render_status_;
    struct CombatMeshVisual {
        MeshHandle mesh;
        InstanceHandle instance;
        std::size_t triangle_count{};
        std::size_t frame{};
    };
    Result update_skeletal_visual(ResourceStore&, CombatMeshVisual&, const SkeletalMeshRuntime*,
                                  const std::optional<Transform>&);
    std::vector<CombatMeshVisual> oil_visuals_;
    std::vector<CombatMeshVisual> electron_visuals_;
    std::vector<CombatMeshVisual> trap_visuals_;
    struct DamageVisual {
        std::optional<std::uint64_t> revision;
        TextureHandle paint;
        std::map<std::string, MaterialHandle> materials;
    };
    std::vector<DamageVisual> damage_visuals_;
    struct DamagePaintTexture {
        TextureHandle handle;
        TextureTransparency transparency{};
    };
    std::map<std::string, DamagePaintTexture> damage_paint_textures_;
    Result update_damage_visual(ResourceStore&, const VehicleDamageVisuals&,
                                std::span<const std::size_t>, DamageVisual&);
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
