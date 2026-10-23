#pragma once
#include "game_src/unresolved/bitmap.hpp"
#include "game_src/unresolved/mesh_scene.hpp"
#include "game_src/unresolved/particle_effects.hpp"
#include "game_src/unresolved/resource_store.hpp"
#include <memory>
#include <optional>
namespace roadwars {
// Body thresholds established by 0040f06f; selection by 004d3c2c.
Result select_vehicle_body_lod(Vec3 object, Vec3 camera, int detail, std::size_t levels,
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
    std::array<MeshObject, 4> body_{};
    // Original terminal LOD is not among the four meshes deformed by collision damage.
    std::optional<MeshObject> terminal_body_;
    struct PaintState {
        Bitmap bitmap;
        std::uint64_t revision{};
        TextureTransparency transparency{TextureTransparency::opaque};
    };
    std::shared_ptr<PaintState> paint_;
    Bitmap damage_;
    std::string texture_path_;
    std::vector<VehicleTextureMark> marks_;
    std::vector<VehicleDeformation> deformations_;
    std::uint64_t revision_{};
    bool configured_{};
};
} // namespace roadwars
