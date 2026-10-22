#pragma once
#include "game_src/interfaces/host.hpp"
#include "game_src/interfaces/race_debug.hpp"
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/variant/rid.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <unordered_map>
#include <unordered_set>

namespace roadwars {
class ServerRenderer final : public Renderer {
  public:
    explicit ServerRenderer(godot::RID viewport, bool original_coordinates = false);
    ~ServerRenderer() override;
    ServerRenderer(const ServerRenderer&) = delete;
    ServerRenderer& operator=(const ServerRenderer&) = delete;
    MeshHandle upload_mesh(std::span<const Vec3>, std::span<const std::uint32_t>) override;
    InstanceHandle create_instance(MeshHandle) override;
    Result update_instance(InstanceHandle, const Transform&) override;
    void destroy_instance(InstanceHandle) override;
    void set_instance_visible(InstanceHandle, bool) override;
    void set_instance_static(InstanceHandle, bool) override;
    void destroy_mesh(MeshHandle) override;
    void update_camera(const Transform&, float vertical_fov_degrees) override;
    std::size_t live_rids() const;
    Result draw_hud(std::span<const HudQuad>) override;
    void resize_hud(float width, float height);
    void set_hud_visible(bool visible);
    TextureHandle upload_texture(std::uint32_t width, std::uint32_t height,
                                 std::span<const std::byte> rgba) override;
    MaterialHandle create_material(TextureHandle texture, std::array<float, 4> color,
                                   bool transparent) override;
    MaterialHandle create_material(TextureHandle, const MaterialSettings&) override;
    void configure_scene(const SceneSettings&) override;
    void create_fog_fixture(float aspect);
    void create_billboard_fixture();
    Result configure_sky(std::span<const Vec3>, std::span<const std::array<float, 2>>,
                         std::span<const std::uint32_t>, MaterialHandle) override;
    MeshHandle upload_surface(std::span<const Vec3> positions,
                              std::span<const std::array<float, 2>> uv,
                              std::span<const std::uint32_t> indices,
                              MaterialHandle material) override;
    MeshHandle upload_surface(std::span<const Vec3>, std::span<const std::array<float, 2>>,
                              std::span<const std::uint32_t>, MaterialHandle, std::span<const Vec3>,
                              std::span<const std::array<float, 4>>) override;
    void destroy_material(MaterialHandle material) override;
    void destroy_texture(TextureHandle texture) override;
    ParticleSystemHandle create_particles(std::uint32_t, MaterialHandle, bool = false) override;
    void set_particles_visible(bool visible);
    Result update_particles(ParticleSystemHandle, std::span<const ParticleDrawState>) override;
    void destroy_particles(ParticleSystemHandle) override;

    void begin_simulation_tick();
    void reset_interpolation();
    void set_paused(bool paused);
    void present_interpolated(double alpha);
    Vec3 interpolated_camera_position(double alpha) const;
    RaceDebugCamera race_debug_camera(double alpha) const;

  private:
    struct PoseHistory {
        godot::Transform3D previous, current;
    };
    std::unordered_map<std::uint64_t, PoseHistory> instance_poses_;
    PoseHistory camera_pose_;
    float camera_fov_{50};
    godot::RenderingServer* server_{};
    godot::RID viewport_, scenario_, camera_, shader_, material_;
    godot::RID environment_, light_, light_instance_;
    godot::RID hud_canvas_, hud_item_;
    godot::RID sky_, sky_material_, sky_shader_;
    bool original_coordinates_{};
    SceneSettings scene_settings_{};
    std::uint64_t next_handle_{1};
    std::unordered_map<std::uint64_t, godot::RID> meshes_, instances_;
    std::unordered_map<std::uint64_t, godot::RID> textures_, materials_, material_shaders_;
    std::unordered_map<std::uint64_t, godot::RID> material_textures_;
    std::unordered_set<std::uint64_t> billboard_materials_, static_instances_;
    struct ParticleResources {
        godot::RID particles, instance, mesh, process_shader, process_material, draw_shader,
            draw_material;
        std::uint32_t capacity{};
    };
    std::unordered_map<std::uint64_t, ParticleResources> particle_systems_;
};
} // namespace roadwars
