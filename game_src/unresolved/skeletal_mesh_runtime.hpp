#pragma once
#include "game_src/unresolved/mesh_scene.hpp"
#include "game_src/unresolved/resource_store.hpp"

namespace roadwars {
Transform original_skeletal_transform(Vec3 origin, float yaw, float pitch, float roll, Vec3 scale);
// Portable reconstruction of004ee465/004ee4ac ownership; original C++ name unresolved.
class SkeletalMeshRuntime {
  public:
    Result load(ResourceStore&, std::string_view mesh_path, std::string_view animation_path,
                std::string& error);
    Result decode(std::span<const std::byte> mesh, std::span<const std::byte> animation,
                  std::string& error);
    Result advance(float delta);
    void reset();
    const MeshPart& geometry() const { return transformed_; }
    std::size_t frame() const { return frame_; }
    std::size_t frame_count() const { return frames_; }
    std::size_t bone_count() const { return bones_.size(); }
    float frame_period() const { return period_; }

  private:
    struct Bone {
        std::vector<std::uint16_t> vertices;
    };
    void transform();
    MeshPart original_, transformed_;
    std::vector<Bone> bones_;
    std::vector<std::array<float, 16>> matrices_;
    std::size_t frames_{}, frame_{};
    float period_{}, elapsed_{};
};
} // namespace roadwars
