#pragma once
#include "game_src/interfaces/host.hpp"
#include <string>

namespace roadwars {
struct Vec2 {
    float x{}, y{};
};
struct MeshPart {
    std::vector<Vec3> positions;
    std::vector<Vec3> normals;
    std::vector<std::array<float, 4>> colors;
    std::vector<Vec2> uv;
    // Raw serialized coordinates retained for native damage-texture operations.
    // Render UVs may sanitize unused NaNs in untextured parts.
    std::vector<Vec2> native_uv;
    std::vector<std::uint32_t> indices;
    std::string texture;
    std::array<float, 4> color{1, 1, 1, 1};
};
struct MeshObject {
    std::string name;
    std::uint32_t flags{}, type{};
    bool visible{true};
    Vec3 position{};
    float dummy_heading_degrees{};
    std::vector<MeshPart> parts;
    std::vector<MeshObject> children;
    // Serialized type4 orientation is metadata; native loading does not set Euler angles.
    std::array<Vec3, 3> serialized_dummy_basis{};
    Vec3 angles_degrees{}; // Native +b4/+b8/+bc: heading, pitch, roll; constructor zeroes them.
};
struct MeshNodeEulerOverride {
    std::string_view name;
    Vec3 angles_degrees;
};
Vec3 original_mesh_node_angles(const MeshObject&, std::span<const std::size_t> path,
                               Vec3 root_angles,
                               std::span<const MeshNodeEulerOverride> overrides = {});
Transform original_mesh_node_pose(const MeshObject&, std::span<const std::size_t> path,
                                  Vec3 root_position, Vec3 root_angles,
                                  std::span<const MeshNodeEulerOverride> overrides = {});
Result decode_msh(std::span<const std::byte> bytes, MeshObject& output, std::string& error);
} // namespace roadwars
