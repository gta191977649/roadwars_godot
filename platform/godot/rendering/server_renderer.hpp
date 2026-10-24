#pragma once
#include "game_src/interfaces/host.hpp"
#include "game_src/interfaces/graphics_settings.hpp"
#include "game_src/interfaces/race_debug.hpp"
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/rid.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <map>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

namespace roadwars {
class ServerRenderer final : public Renderer {
  public:
    explicit ServerRenderer(godot::RID viewport, bool original_coordinates = false);
    ~ServerRenderer() override;
    Result apply_graphics_quality(const GraphicsSettings& settings);
    int car_detail() const override { return graphics_settings_.car_detail; }
    ChromeMappingOptions chrome_mapping_options() const override { return chrome_options_; }
    Result set_chrome_mapping_enabled(bool enabled) override {
        chrome_options_.enabled = enabled;
        return Result::ok;
    }
    ServerRenderer(const ServerRenderer&) = delete;
    ServerRenderer& operator=(const ServerRenderer&) = delete;
    MeshHandle upload_mesh(std::span<const Vec3>, std::span<const std::uint32_t>) override;
    InstanceHandle create_instance(MeshHandle) override;
    Result update_instance(InstanceHandle, const Transform&) override;
    void destroy_instance(InstanceHandle) override;
    void set_instance_visible(InstanceHandle, bool) override;
    void set_instance_camera_hidden(InstanceHandle, bool) override;
    void set_instance_static(InstanceHandle, bool) override;
    void reset_instance_interpolation(InstanceHandle) override;
    Result set_instance_mesh(InstanceHandle, MeshHandle) override;
    Result update_texture(TextureHandle, std::uint32_t, std::uint32_t,
                          std::span<const std::byte>) override;
    void destroy_mesh(MeshHandle) override;
    void update_camera(const Transform&, float vertical_fov_degrees) override;
    std::size_t live_rids() const;
    Result draw_hud(std::span<const HudQuad>) override;
    void resize_hud(float width, float height);
    void set_first_person(bool enabled) { first_person_ = enabled; }
    void set_hud_visible(bool visible);
    TextureHandle upload_texture(std::uint32_t width, std::uint32_t height,
                                 std::span<const std::byte> rgba) override;
    MaterialHandle create_material(TextureHandle texture, std::array<float, 4> color,
                                   bool transparent) override;
    MaterialHandle create_material(TextureHandle, const MaterialSettings&) override;
    void configure_scene(const SceneSettings&) override;
    void create_fog_fixture(float aspect);
    void create_billboard_fixture();
    godot::Dictionary create_chrome_fixture(float aspect);
    void clear_sky();
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
    void prewarm_particles(std::uint32_t, MaterialHandle, bool, bool) override;
    ParticleSystemHandle create_particles(std::uint32_t, MaterialHandle, bool = false,
                                          bool = false) override;
    Result set_instance_vertex_color(InstanceHandle, std::uint32_t) override;
    void set_particles_visible(bool visible);
    Result update_particles(ParticleSystemHandle, std::span<const ParticleDrawState>) override;
    void destroy_particles(ParticleSystemHandle) override;

    godot::Dictionary presentation_snapshot() const;
    const godot::Transform3D& presented_camera_transform() const { return camera_pose_.presented; }
    static bool test_interpolation_history();
    static bool test_graphics_quality();
    void begin_simulation_tick();
    void reset_interpolation();
    void reset_camera_interpolation();
    void set_interpolation_enabled(bool enabled);
    void set_paused(bool paused);
    void present_interpolated(double alpha);
    Vec3 interpolated_camera_position(double alpha) const;
    RaceDebugCamera race_debug_camera(double alpha) const;

  private:
    godot::RID world_texture(TextureHandle texture);
    void refresh_world_texture(std::uint64_t texture);
    std::string filtered_surface_code(const std::string& code) const;
    GraphicsSettings graphics_settings_{};
    std::unordered_map<std::uint64_t, godot::RID> world_textures_;
    std::unordered_map<std::uint64_t, godot::Ref<godot::Image>> texture_sources_;
    std::unordered_map<std::uint64_t, std::string> material_shader_sources_;
    struct PoseHistory {
        godot::Transform3D previous, current, presented;
    };
    std::unordered_map<std::uint64_t, PoseHistory> instance_poses_;
    PoseHistory camera_pose_;
    float camera_fov_{50}, previous_camera_fov_{50}, presented_camera_fov_{50};
    bool interpolation_enabled_{};
    bool first_person_{};
    float hud_width_{640}, hud_height_{480};
    std::vector<std::array<godot::Vector2, 4>> aspect_hud_pixels_;
    std::unordered_set<std::uint64_t> camera_hidden_, invisible_instances_;
    godot::RenderingServer* server_{};
    godot::RID viewport_, scenario_, camera_, shader_, material_;
    godot::RID environment_, light_, light_instance_;
    godot::RID hud_canvas_, hud_item_;
    godot::RID sky_, sky_material_, sky_shader_;
    bool original_coordinates_{};
    // User-authorized installed-resource compatibility, not an original filename fallback.
    ChromeMappingOptions chrome_options_{true, "Textures/16cr1.bmp"};
    SceneSettings scene_settings_{};
    std::uint64_t next_handle_{1};
    std::unordered_map<std::uint64_t, godot::RID> meshes_, instances_;
    std::unordered_map<std::uint64_t, std::uint64_t> mesh_materials_, instance_meshes_;
    struct ColorMaterial {
        godot::RID material;
        std::uint64_t source{};
    };
    std::unordered_map<std::uint64_t, ColorMaterial> instance_color_materials_;
    std::unordered_map<std::uint64_t, godot::RID> textures_, materials_, material_shaders_;
    std::unordered_map<std::uint64_t, godot::RID> material_textures_;
    std::unordered_set<std::uint64_t> billboard_materials_, static_instances_;
    std::unordered_set<std::uint64_t> chrome_materials_;
    struct ParticleResources {
        godot::RID particles, instance, mesh, process_shader, process_material, draw_shader,
            draw_material;
        std::uint32_t capacity{};
    };
    std::unordered_map<std::uint64_t, ParticleResources> particle_systems_;
    std::unordered_map<std::uint32_t, godot::RID> particle_process_shaders_;
    std::map<std::pair<std::uint32_t, bool>, godot::RID> particle_draw_shaders_;
    std::map<std::tuple<std::uint32_t, std::uint64_t, bool, bool>, ParticleSystemHandle>
        particle_warmups_;
};
} // namespace roadwars
