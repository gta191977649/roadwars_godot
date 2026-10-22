#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace roadwars {
struct Vec3 {
    float x{}, y{}, z{};
};
struct Transform {
    std::array<Vec3, 3> basis{{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}};
    Vec3 origin{};
};
struct MeshHandle {
    std::uint64_t value{};
};
struct TextureHandle {
    std::uint64_t value{};
};
struct HudQuad {
    TextureHandle texture;
    std::array<std::array<float, 2>, 4> positions{}, uv{};
    std::array<std::array<float, 4>, 4> colors{};
};
struct MaterialHandle {
    std::uint64_t value{};
};
enum class AlphaMode { opaque, cutout, blend, additive };
struct MaterialSettings {
    std::array<float, 4> color{1, 1, 1, 1};
    AlphaMode alpha_mode{AlphaMode::opaque};
    bool unshaded{}, depth_write{true}, depth_test{true}, fog{true}, opaque_alpha_only{};
    bool billboard_y{};
};
struct SceneSettings {
    Vec3 fog_color{}, directional_light{};
    float ambient_light{}, near_distance{1}, far_distance{400};
    float camera_height{};
};
struct InstanceHandle {
    std::uint64_t value{};
};
struct ParticleSystemHandle {
    std::uint64_t value{};
};
struct ParticleDrawState {
    Vec3 position{};
    float size{};
    std::uint32_t color_argb{}, atlas_frame{};
    bool active{};
    // Optional original per-slot geometry, in the camera-yaw plane.
    std::array<Vec3, 3> triangle{};
    bool custom_triangle{};
};
struct InputState {
    float steering{}, throttle{}, brake{};
    bool quit_requested{}, reset_requested{}, handbrake{};
    std::array<bool, 3> weapon_down{}, weapon_requested{};
    bool nitrous_down{}, nitrous_requested{};
    bool current_weapon_down{}, current_weapon_requested{}, cycle_weapon_down{},
        cycle_weapon_requested{};
    bool any_key_down{};
};
enum class Result { ok, invalid_argument, unavailable, not_found };
class ResourceReader {
  public:
    virtual ~ResourceReader() = default;
    virtual Result read(std::string_view path, std::vector<std::byte>& output) = 0;
};
class Logger {
  public:
    virtual ~Logger() = default;
    virtual void log(std::string_view message) = 0;
};
class Renderer {
  public:
    virtual ~Renderer() = default;
    // Logical 640x480 screen coordinates. Empty submission clears the frontend.
    virtual Result draw_hud(std::span<const HudQuad>) { return Result::unavailable; }
    virtual MeshHandle upload_mesh(std::span<const Vec3> vertices,
                                   std::span<const std::uint32_t> indices) = 0;
    virtual InstanceHandle create_instance(MeshHandle mesh) = 0;
    virtual Result update_instance(InstanceHandle instance, const Transform& transform) = 0;
    virtual void destroy_instance(InstanceHandle instance) = 0;
    virtual void set_instance_visible(InstanceHandle, bool) {}
    virtual void set_instance_static(InstanceHandle, bool) {}
    // Visual-only discontinuity: do not interpolate an instance across a teleport.
    virtual void reset_instance_interpolation(InstanceHandle) {}
    virtual Result set_instance_vertex_color(InstanceHandle, std::uint32_t) {
        return Result::unavailable;
    }
    virtual Result set_instance_mesh(InstanceHandle, MeshHandle) { return Result::unavailable; }
    virtual Result update_texture(TextureHandle, std::uint32_t, std::uint32_t,
                                  std::span<const std::byte>) {
        return Result::unavailable;
    }
    virtual void destroy_mesh(MeshHandle mesh) = 0;
    virtual void update_camera(const Transform& transform, float vertical_fov_degrees) = 0;
    virtual TextureHandle upload_texture(std::uint32_t, std::uint32_t, std::span<const std::byte>) {
        return {};
    }
    virtual MaterialHandle create_material(TextureHandle, std::array<float, 4>, bool) { return {}; }
    virtual MaterialHandle create_material(TextureHandle, const MaterialSettings&) { return {}; }
    virtual void configure_scene(const SceneSettings&) {}
    virtual Result configure_sky(std::span<const Vec3>, std::span<const std::array<float, 2>>,
                                 std::span<const std::uint32_t>, MaterialHandle) {
        return Result::unavailable;
    }
    virtual MeshHandle upload_surface(std::span<const Vec3>, std::span<const std::array<float, 2>>,
                                      std::span<const std::uint32_t>, MaterialHandle) {
        return {};
    }
    virtual MeshHandle upload_surface(std::span<const Vec3>, std::span<const std::array<float, 2>>,
                                      std::span<const std::uint32_t>, MaterialHandle,
                                      std::span<const Vec3>,
                                      std::span<const std::array<float, 4>>) {
        return {};
    }
    virtual void destroy_material(MaterialHandle) {}
    virtual void destroy_texture(TextureHandle) {}
    // Alpha-mixed pools use the lower atlas; flame uses it with additive blending.
    virtual ParticleSystemHandle create_particles(std::uint32_t, MaterialHandle, bool = false,
                                                  bool = false) {
        return {};
    }
    virtual Result update_particles(ParticleSystemHandle, std::span<const ParticleDrawState>) {
        return Result::unavailable;
    }
    virtual void destroy_particles(ParticleSystemHandle) {}
};
} // namespace roadwars
