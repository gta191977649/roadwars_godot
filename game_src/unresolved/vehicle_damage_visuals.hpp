#pragma once
#include "game_src/unresolved/bitmap.hpp"
#include "game_src/unresolved/mesh_scene.hpp"
#include "game_src/unresolved/particle_effects.hpp"
#include "game_src/unresolved/resource_store.hpp"
#include <memory>
#include <functional>
#include <optional>
namespace roadwars {
// Body thresholds established by 0040f06f; selection by 004d3c2c.
Result select_vehicle_body_lod(Vec3 object, Vec3 camera, int detail, std::size_t levels,
                               std::size_t& selected);
Result select_vehicle_tire_lod(Vec3 object, Vec3 camera, int detail, std::size_t levels,
                               std::size_t& selected);
struct VehicleDamageRect {
    std::int32_t left{}, top{}, right{}, bottom{};
};
struct VehicleTextureMark {
    std::optional<std::uint32_t> side;
    std::array<VehicleDamageRect, 4> rectangles;
};
struct VehicleDeformation {
    Vec3 local_point{}, displacement{};
};
struct VehiclePixelPairMark {
    std::uint32_t side{};
    std::int32_t type{}, x{}, y{};
    // Portable copies of the two prior native surface pixels, decoded as RGBA.
    std::array<std::array<std::byte, 4>, 2> previous;
};
enum class VehicleRepairRegion { all, internal, front, right, rear, left };
// Transfer data for the same vehicle and paint resources, without renderer ownership.
struct VehicleDamageVisualState {
    std::string texture_path;
    Bitmap paint;
    std::vector<VehicleTextureMark> marks;
    std::vector<VehicleDeformation> deformations;
    std::vector<VehiclePixelPairMark> pixel_pairs;
};
struct VehicleDamageVisualContact {
    Vec3 force{}, world_point{}, vehicle_origin{}, front_direction{};
    float heading_degrees{}, mass_kg{}, width{}, length{};
    bool invulnerable{}, disabled{}, receiver_front_guard{};
};
// Portable ownership of body meshes/paint mutation; original class name unresolved.
class VehicleDamageVisuals {
  public:
    Result load(ResourceStore&, std::string_view vehicle, int paint_job, std::string& error);
    // Bitmaps must already use the original DirectDraw surface orientation/format.
    Result configure(std::array<MeshObject, 4> body_lods, Bitmap paint, Bitmap damage,
                     std::string texture_path,
                     TextureTransparency transparency = TextureTransparency::opaque);
    Result collision(const VehicleDamageVisualContact&, ParticleRandom&);
    Result share_paint_with(VehicleDamageVisuals&);
    Result repair(VehicleRepairRegion, float fraction, float width, float length);
    VehicleDamageVisualState capture_state() const;
    Result restore_state(const VehicleDamageVisualState&);
    Result replay_texture_mark(const VehicleTextureMark&, bool damaged);
    void observe_replay_mutations(std::function<void(const VehicleTextureMark&)> texture,
                                  std::function<void(const VehicleDeformation&)> deformation) {
        replay_texture_ = std::move(texture);
        replay_deformation_ = std::move(deformation);
    }
    // Transfer only mesh deformation records, independently of paint resources.
    Result reapply_deformations(std::span<const VehicleDeformation>);
    // Replay existing modeled damage over newly loaded paint resources.
    Result repaint_damage_from(const VehicleDamageVisualState&);
    // 00421f1d rectangle path: offsets are centered relative coordinates.
    Result mark_pixel_pair(std::uint32_t side, std::int32_t type, VehicleDamageRect atlas,
                           Vec2 offset);
    // Native side order: FRONT, REAR, MIDDLELEFT, MIDDLERIGHT.
    Result configure_projectile_regions(std::array<VehicleDamageRect, 4>);
    // Contact sides are footprint order FRONT, RIGHT, REAR, LEFT. The original
    // guards are race completion and dying timer, evaluated before health damage.
    Result projectile_damage(std::size_t contact_side, std::int32_t type, ParticleRandom&,
                             bool finished = false, bool dying = false);
    std::span<const VehiclePixelPairMark> pixel_pairs() const { return pixel_pairs_; }
    std::span<const MeshObject, 4> body_lods() const { return body_; }
    std::size_t body_lod_count() const { return terminal_body_ ? 5 : 4; }
    const MeshObject& body_lod(std::size_t index) const {
        return index < 4 ? body_.at(index) : terminal_body_.value();
    }
    const Bitmap& paint() const { return paint_->bitmap; }
    TextureTransparency texture_transparency() const { return paint_->transparency; }
    std::string_view texture_path() const { return texture_path_; }
    std::span<const VehicleTextureMark> marks() const { return marks_; }
    std::span<const VehicleDeformation> deformations() const { return deformations_; }
    std::uint64_t revision() const { return revision_ + (paint_ ? paint_->revision : 0); }

  private:
    void mark(std::optional<std::uint32_t>, Vec2 uv, ParticleRandom&);
    void deform(Vec3 point, Vec3 displacement);
    void rebuild_deformations();
    void apply_pixel_pair(std::uint32_t side, std::int32_t type, std::int32_t x, std::int32_t y);
    std::array<MeshObject, 4> body_{};
    std::array<std::vector<std::vector<Vec3>>, 4> original_positions_;
    // Original terminal LOD is not among the four meshes deformed by collision damage.
    std::optional<MeshObject> terminal_body_;
    struct PaintState {
        Bitmap bitmap;
        std::uint64_t revision{};
        TextureTransparency transparency{TextureTransparency::opaque};
    };
    std::shared_ptr<PaintState> paint_;
    Bitmap damage_;
    Bitmap original_paint_;
    std::string texture_path_;
    std::vector<VehicleTextureMark> marks_;
    std::vector<VehicleDeformation> deformations_;
    std::vector<VehiclePixelPairMark> pixel_pairs_;
    std::function<void(const VehicleTextureMark&)> replay_texture_;
    std::function<void(const VehicleDeformation&)> replay_deformation_;
    std::array<VehicleDamageRect, 4> projectile_regions_{};
    bool projectile_regions_configured_{};
    std::uint64_t revision_{};
    bool configured_{};
};
} // namespace roadwars
