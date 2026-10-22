#include "game_src/unresolved/contact_orientation.hpp"
#include "game_src/unresolved/mesh_scene.hpp"
#include <bit>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <stdexcept>
using namespace roadwars;
namespace {
bool close(float a, float b) { return std::abs(a - b) < .0001f; }
void check(Vec3 a, Vec3 b) { assert(close(a.x, b.x) && close(a.y, b.y) && close(a.z, b.z)); }
Vec3 rotate(const Transform& t, Vec3 p) {
    return {t.basis[0].x * p.x + t.basis[1].x * p.y + t.basis[2].x * p.z,
            t.basis[0].y * p.x + t.basis[1].y * p.y + t.basis[2].y * p.z,
            t.basis[0].z * p.x + t.basis[1].z * p.y + t.basis[2].z * p.z};
}
Vec3 add(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
void u32(std::vector<std::byte>& b, std::uint32_t v) {
    for (unsigned i = 0; i < 4; ++i)
        b.push_back(static_cast<std::byte>((v >> (i * 8)) & 255));
}
void vec(std::vector<std::byte>& b, Vec3 p) {
    for (auto v : {p.x, p.y, p.z})
        u32(b, std::bit_cast<std::uint32_t>(v));
}
void walk(const MeshObject& n) {
    check(n.angles_degrees, {});
    if (n.type == 4) {
        for (auto b : n.serialized_dummy_basis)
            assert(std::isfinite(b.x) && std::isfinite(b.y) && std::isfinite(b.z));
    }
    for (auto& c : n.children)
        walk(c);
}
bool find(const MeshObject& n, std::string_view name, std::vector<std::size_t>& path) {
    if (n.name == name)
        return true;
    for (std::size_t i = 0; i < n.children.size(); ++i) {
        path.push_back(i);
        if (find(n.children[i], name, path))
            return true;
        path.pop_back();
    }
    return false;
}
} // namespace
int main(int argc, char** argv) {
    std::vector<std::byte> bytes(32);
    bytes[0] = std::byte{'D'};
    u32(bytes, 4);
    u32(bytes, 0);
    vec(bytes, {1, 2, 3});
    vec(bytes, {1, 1, 0});
    vec(bytes, {7, 8, 9});
    vec(bytes, {4, 5, 6});
    MeshObject decoded;
    std::string error;
    assert(decode_msh(bytes, decoded, error) == Result::ok);
    check(decoded.serialized_dummy_basis[0], {1, 2, 3});
    check(decoded.serialized_dummy_basis[1], {1, 1, 0});
    check(decoded.serialized_dummy_basis[2], {7, 8, 9});
    check(decoded.angles_degrees, {});
    assert(decoded.dummy_heading_degrees > 89 && decoded.dummy_heading_degrees < 91);
    const auto unloaded =
        original_mesh_node_pose(decoded, {}, decoded.position, decoded.angles_degrees);
    check(unloaded.origin, {4, 5, 6});
    check(unloaded.basis[0], {1, 0, 0});
    MeshObject root, child, grandchild;
    child.position = {1, 2, 3};
    child.angles_degrees = {10, 15, 25};
    grandchild.position = {4, 5, 6};
    grandchild.angles_degrees = {-7, 9, 3};
    child.children.push_back(grandchild);
    root.children.push_back(child);
    const std::array<std::size_t, 2> path{0, 0};
    const Vec3 origin{7, 8, 9}, angles{20, 30, 40};
    const auto pose = original_mesh_node_pose(root, path, origin, angles);
    const auto expected = original_vehicle_transform({}, 23, 54, 68);
    for (unsigned i = 0; i < 3; ++i)
        check(pose.basis[i], expected.basis[i]);
    check(pose.origin, add(add(rotate(original_vehicle_transform({}, 30, 45, 65), {4, 5, 6}),
                               rotate(original_vehicle_transform({}, 20, 30, 40), {1, 2, 3})),
                           origin));
    bool rejected{};
    try {
        original_mesh_node_pose(root, std::array<std::size_t, 1>{9}, {}, {});
    } catch (const std::out_of_range&) {
        rejected = true;
    }
    assert(rejected);
    if (argc == 2) {
        unsigned cars{}, mounts{};
        for (auto& entry :
             std::filesystem::directory_iterator(std::filesystem::path(argv[1]) / "Cars")) {
            std::ifstream stream(entry.path() / "Body/SpatialObject.msh",
                                 std::ios::binary | std::ios::ate);
            if (!stream)
                continue;
            ++cars;
            std::vector<std::byte> original(static_cast<std::size_t>(stream.tellg()));
            stream.seekg(0);
            stream.read(reinterpret_cast<char*>(original.data()), original.size());
            MeshObject car;
            if (decode_msh(original, car, error) != Result::ok)
                throw std::runtime_error(error);
            walk(car);
            for (auto name : {"FrontWeaponMount", "RearWeaponMount", "ULRORMount1", "ROR1",
                              "ULRORMount1Weapon", "HoodPoint"}) {
                std::vector<std::size_t> actual;
                assert(find(car, name, actual));
                const MeshObject* node = &car;
                Vec3 sum = car.position;
                bool body_ancestor{};
                for (auto i : actual) {
                    node = &node->children[i];
                    sum = add(sum, node->position);
                    body_ancestor = body_ancestor || node->name == "1024Body";
                }
                assert(body_ancestor && (node->flags & 0x200) == 0);
                const auto plain = original_mesh_node_pose(car, actual, car.position, {});
                check(plain.origin, sum);
                check(plain.basis[0], {1, 0, 0});
                check(plain.basis[2], {0, 0, 1});
                const std::array<MeshNodeEulerOverride, 2> changes{
                    {{"1024Body", {0, 11, 17}}, {"ROR1", {0, 0, -45}}}};
                const auto changed =
                    original_mesh_node_pose(car, actual, {}, {30, 15, 20}, changes);
                const auto expected_angles =
                    original_vehicle_transform({}, 30, 26,
                                               name == std::string_view("ULRORMount1Weapon") ||
                                                       name == std::string_view("ROR1")
                                                   ? -8.f
                                                   : 37.f);
                for (unsigned i = 0; i < 3; ++i)
                    check(changed.basis[i], expected_angles.basis[i]);
                ++mounts;
            }
        }
        assert(cars > 0 && mounts == cars * 6);
    }
}
