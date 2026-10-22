#include "game_src/unresolved/skeletal_mesh_runtime.hpp"
#include <bit>
#include <cassert>
#include <cmath>
using namespace roadwars;
namespace {
void u32(std::vector<std::byte>& b, std::uint32_t n) {
    for (unsigned i = 0; i < 4; ++i)
        b.push_back(static_cast<std::byte>((n >> (8 * i)) & 255));
}
void u16(std::vector<std::byte>& b, std::uint16_t n) {
    b.push_back(static_cast<std::byte>(n & 255));
    b.push_back(static_cast<std::byte>(n >> 8));
}
void f32(std::vector<std::byte>& b, float n) { u32(b, std::bit_cast<std::uint32_t>(n)); }
void matrix(std::vector<std::byte>& b, float x, float y) {
    const std::array<float, 16> m{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, x, y, 0, 1};
    for (const auto n : m)
        f32(b, n);
}
} // namespace
int main() {
    const auto world = original_skeletal_transform({4, 5, 6}, 90, 90, 0, {2, 3, 4});
    assert(std::abs(world.basis[0].z + 2) < .001f);
    assert(std::abs(world.basis[1].x - 3) < .001f);
    assert(world.origin.x == 4 && world.origin.y == 5);
    std::vector<std::byte> mesh, animation;
    u32(mesh, 3);
    for (unsigned i = 0; i < 3; ++i) {
        f32(mesh, static_cast<float>(i));
        f32(mesh, 2);
        f32(mesh, 3);
        f32(mesh, 0);
        f32(mesh, 1);
        f32(mesh, 0);
        f32(mesh, .25f);
        f32(mesh, .5f);
    }
    u32(mesh, 1);
    u16(mesh, 0);
    u16(mesh, 1);
    u16(mesh, 2);
    const std::string texture = "beam.bmp";
    for (unsigned i = 0; i < 64; ++i)
        mesh.push_back(i < texture.size() ? static_cast<std::byte>(texture[i]) : std::byte{});
    u32(mesh, 3);
    u16(mesh, 0);
    u16(mesh, 1);
    u16(mesh, 2);
    u32(mesh, 1);
    u32(mesh, 1);
    u16(mesh, 2);
    u32(mesh, 0);
    u32(animation, 2);
    u32(animation, 10);
    matrix(animation, 10, 0);
    matrix(animation, 20, 0); // Root, both frames.
    matrix(animation, 0, 30);
    matrix(animation, 0, 40); // Child, both frames.
    SkeletalMeshRuntime runtime;
    std::string error;
    assert(runtime.advance(0) == Result::unavailable);
    assert(runtime.decode(mesh, animation, error) == Result::ok);
    assert(runtime.bone_count() == 2 && runtime.frame_count() == 2 && runtime.frame() == 0);
    assert(runtime.geometry().texture == texture && runtime.geometry().indices.size() == 3);
    assert(runtime.geometry().positions[0].x == 10);
    assert(runtime.geometry().positions[2].x == 2 && runtime.geometry().positions[2].y == 32);
    assert(runtime.geometry().normals[2].y == 1 && runtime.geometry().uv[2].x == .25f);
    assert(runtime.advance(.05f) == Result::ok && runtime.frame() == 0);
    assert(runtime.advance(.05f) == Result::ok && runtime.frame() == 1);
    assert(runtime.geometry().positions[0].x == 20 && runtime.geometry().positions[2].y == 42);
    assert(runtime.advance(10) == Result::ok && runtime.frame() == 0);   // At most one frame.
    assert(runtime.advance(.01f) == Result::ok && runtime.frame() == 0); // Dropped remainder.
    runtime.reset();
    assert(runtime.frame() == 0 && runtime.geometry().positions[0].x == 10);
    assert(runtime.advance(-1) == Result::invalid_argument);
    auto truncated = animation;
    truncated.pop_back();
    assert(runtime.decode(mesh, truncated, error) == Result::invalid_argument);
    assert(runtime.frame_count() == 2); // Transactional decode.
    animation.push_back(std::byte{});
    assert(runtime.decode(mesh, animation, error) == Result::invalid_argument);
    assert(runtime.decode({}, {}, error) == Result::invalid_argument);
}
