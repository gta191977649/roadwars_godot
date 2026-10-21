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
};
Result decode_msh(std::span<const std::byte> bytes, MeshObject& output, std::string& error);
} // namespace roadwars
