#include "game_src/unresolved/frontend_scene.hpp"
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>

using namespace roadwars;
namespace {
class Files final : public ResourceReader {
  public:
    explicit Files(std::filesystem::path root) : root_(std::move(root)) {}
    Result read(std::string_view name, std::vector<std::byte>& output) override {
        std::ifstream file(root_ / std::filesystem::path(name), std::ios::binary | std::ios::ate);
        if (!file)
            return Result::not_found;
        const auto size = file.tellg();
        if (size < 0)
            return Result::invalid_argument;
        output.resize(static_cast<std::size_t>(size));
        file.seekg(0);
        file.read(reinterpret_cast<char*>(output.data()), size);
        return file ? Result::ok : Result::invalid_argument;
    }

  private:
    std::filesystem::path root_;
};
class SceneRenderer final : public Renderer {
  public:
    std::set<std::uint64_t> textures, materials, meshes, instances;
    std::set<std::uint64_t> chrome_materials;
    ChromeMappingOptions chrome_mapping_options() const override {
        return {true, "Textures/16cr1.bmp"};
    }
    std::map<std::uint64_t, std::uint64_t> instance_mesh;
    std::uint64_t next{1};
    bool fail_upload{};
    Transform camera;
    MeshHandle upload_mesh(std::span<const Vec3>, std::span<const std::uint32_t>) override {
        return {};
    }
    MeshHandle upload_surface(std::span<const Vec3> positions,
                              std::span<const std::array<float, 2>> uv,
                              std::span<const std::uint32_t> indices, MaterialHandle material,
                              std::span<const Vec3> normals,
                              std::span<const std::array<float, 4>> colors) override {
        assert(materials.contains(material.value));
        assert(uv.size() == positions.size());
        assert(normals.empty() || normals.size() == positions.size());
        assert(colors.empty() || colors.size() == positions.size());
        for (const auto index : indices)
            assert(index < positions.size());
        if (fail_upload)
            return {};
        meshes.insert(next);
        return {next++};
    }
    InstanceHandle create_instance(MeshHandle mesh) override {
        assert(meshes.contains(mesh.value));
        instances.insert(next);
        instance_mesh[next] = mesh.value;
        return {next++};
    }
    Result update_instance(InstanceHandle instance, const Transform& transform) override {
        assert(instances.contains(instance.value));
        assert(std::isfinite(transform.origin.x) && std::isfinite(transform.origin.y) &&
               std::isfinite(transform.origin.z));
        return Result::ok;
    }
    void destroy_instance(InstanceHandle handle) override {
        assert(instances.erase(handle.value));
        assert(instance_mesh.erase(handle.value));
    }
    void destroy_mesh(MeshHandle handle) override {
        for (const auto& [instance, mesh] : instance_mesh) {
            (void)instance;
            assert(mesh != handle.value);
        }
        assert(meshes.erase(handle.value));
    }
    void update_camera(const Transform& pose, float fov) override {
        assert(fov == 50);
        camera = pose;
    }
    TextureHandle upload_texture(std::uint32_t width, std::uint32_t height,
                                 std::span<const std::byte> bytes) override {
        assert(bytes.size() == static_cast<std::size_t>(width) * height * 4);
        textures.insert(next);
        return {next++};
    }
    MaterialHandle create_material(TextureHandle texture,
                                   const MaterialSettings& settings) override {
        assert(textures.contains(texture.value));
        if (settings.chrome_map.value) {
            assert(textures.contains(settings.chrome_map.value));
            chrome_materials.insert(next);
        }
        materials.insert(next);
        return {next++};
    }
    void destroy_material(MaterialHandle handle) override {
        assert(meshes.empty());
        assert(materials.erase(handle.value));
        chrome_materials.erase(handle.value);
    }
    void destroy_texture(TextureHandle handle) override {
        assert(materials.empty());
        assert(textures.erase(handle.value));
    }
    bool empty() const {
        return textures.empty() && materials.empty() && meshes.empty() && instances.empty() &&
               chrome_materials.empty();
    }
};
void u32(std::vector<std::byte>& bytes, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8)
        bytes.push_back(static_cast<std::byte>((value >> shift) & 255));
}
} // namespace
int main(int argc, char** argv) {
    std::vector<std::byte> bytes;
    u32(bytes, 1);
    bytes.resize(36); // Root name is empty, type zero and no children.
    u32(bytes, 0);
    u32(bytes, 0);
    u32(bytes, 1);
    MeshObject object;
    std::string error;
    assert(decode_frontend_d3d(bytes, object, error) == Result::ok);
    bytes.push_back(std::byte{});
    assert(decode_frontend_d3d(bytes, object, error) == Result::invalid_argument);
    assert(decode_frontend_d3d({}, object, error) == Result::invalid_argument);
    if (argc > 1) {
        Files files(argv[1]);
        ResourceStore resources(files);
        assert(resources.open() == Result::ok);
        assert(resources.read("Characters/Shared/Garage/SpatialObject.D3D", bytes) == Result::ok);
        assert(decode_frontend_d3d(bytes, object, error) == Result::ok);
        assert(object.name == "Garage" && object.children.size() == 31);
        assert(object.parts.size() == 3);
        bytes.pop_back();
        assert(decode_frontend_d3d(bytes, object, error) == Result::invalid_argument);
        SceneRenderer renderer;
        FrontendScene scene(resources, renderer);
        for (const auto* vehicle :
             {"Animal", "Avenging Angel", "Bud The Crud", "Daisy", "Electra", "Heartbreaker",
              "Kitiara", "Niki Roadkill", "Radcliff", "Romulus", "Sarge", "Wainright"}) {
            if (scene.load("modify_vehicle", vehicle, 0, error) != Result::ok) {
                std::cerr << vehicle << ": " << error << '\n';
                return 1;
            }
            if (renderer.chrome_materials.empty()) {
                std::cerr << vehicle << ": no chrome material bound in vehicle preview\n";
                return 1;
            }
            RaceLoadout loadout;
            loadout.paint_job = 2;
            loadout.tires = {"Street", "Best"};
            loadout.weapons[0] = "Machine Gun";
            loadout.weapons[1] = "Gatling Gun";
            loadout.weapons[2] = "Missile Jammer";
            if (scene.set_vehicle_loadout(loadout, error) != Result::ok) {
                std::cerr << vehicle << ": selected loadout: " << error << '\n';
                return 1;
            }
            assert(scene.advance(.1) == Result::ok);
            scene.stop();
            if (!renderer.empty()) {
                std::cerr << vehicle << ": frontend resources retained after stop\n";
                return 1;
            }
        }
        for (int variant = 0; variant <= 6; ++variant) {
            if (scene.load("modify_vehicle", "ANIMAL", variant, error) != Result::ok) {
                std::cerr << error << '\n';
                return 1;
            }
            assert(!renderer.instances.empty());
            const auto first_product_count = renderer.instances.size();
            if (variant != 0) {
                assert(scene.select_store_product({}, {}, error) == Result::ok);
                assert(renderer.instances.size() == first_product_count);
                assert(scene.select_store_product("../invalid", {}, error) ==
                       Result::invalid_argument);
                const auto open = scene.store_door_positions();
                const auto original_instances = renderer.instances;
                assert(scene.transition_store_product({}, {}, error) == Result::ok);
                assert(scene.store_transition_active());
                assert(scene.drain_store_door_events() == 0);
                assert(renderer.instances == original_instances);
                assert(scene.advance(1.0 / 60) == Result::ok);
                const auto moving = scene.store_door_positions();
                assert(moving[0] < open[0] && moving[1] > open[1]);
                for (int tick = 0; scene.store_transition_active() && tick < 600; ++tick)
                    assert(scene.advance(1.0 / 60) == Result::ok);
                assert(!scene.store_transition_active());
                assert(scene.drain_store_door_events() == 1);
                assert(scene.drain_store_door_events() == 0);
                assert(renderer.instances != original_instances);
                assert(scene.advance(4) == Result::ok);
                assert(scene.store_door_positions() == open);
                assert(scene.transition_store_product({}, {}, error) == Result::ok);
                scene.stop();
                assert(scene.drain_store_door_events() == 0);
                assert(!scene.store_transition_active());
                assert(renderer.empty());
                assert(scene.load("modify_vehicle", "ANIMAL", variant, error) == Result::ok);
            }
            assert(scene.advance(1) == Result::ok);
            assert(scene.advance(-1) == Result::invalid_argument);
            scene.stop();
            assert(renderer.empty());
        }
        if (scene.load("victory_defeat", "ANIMAL", 0, error) != Result::ok) {
            std::cerr << error << '\n';
            return 1;
        }
        // Mode 9 initially looks toward Cam1, rather than away from the whole room.
        assert(renderer.instances.size() > 60); // Room, three cars, portraits and prize glyphs.
        assert(scene.camera_index() == 0);
        assert(scene.advance(2) == Result::ok);
        assert(scene.camera_index() == 0);
        assert(scene.advance(1.0 / 60) == Result::ok);
        assert(scene.camera_index() == 1);
        scene.stop();
        assert(renderer.empty());
        assert(scene.load("modify_vehicle", "ANIMAL", 0, error) == Result::ok);
        // The first garage arrival eye and Stand share height; +Z is camera backward.
        assert(std::abs(renderer.camera.basis[2].y) < .001f);
        assert(renderer.camera.basis[2].x < -.5f && renderer.camera.basis[2].z > .5f);
        for (int frame = 0; frame < 600; ++frame)
            assert(scene.advance(1.0 / 30) == Result::ok);
        assert(scene.camera_index() == 1);
        const auto first = scene.camera_position();
        scene.stop();
        assert(scene.load("modify_vehicle", "ANIMAL", 0, error) == Result::ok);
        for (int frame = 0; frame < 2880; ++frame)
            assert(scene.advance(1.0 / 144) == Result::ok);
        const auto second = scene.camera_position();
        assert(first.x == second.x && first.y == second.y && first.z == second.z);
        scene.stop();
        assert(renderer.empty());
        renderer.fail_upload = true;
        assert(scene.load("modify_vehicle", "ANIMAL", 0, error) == Result::unavailable);
        assert(renderer.empty());
        std::cout << "Original frontend scene assets, frame-rate and teardown checks passed\n";
    }
    std::cout << "Frontend D3D boundaries passed\n";
}
