#include "game_src/unresolved/race_presentation.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
using namespace roadwars;
class RenderProbeReader final : public ResourceReader {
  public:
    explicit RenderProbeReader(std::filesystem::path root) : root_(std::move(root)) {}
    Result read(std::string_view name, std::vector<std::byte>& bytes) override {
        std::ifstream input(root_ / std::filesystem::path(name), std::ios::binary | std::ios::ate);
        if (!input)
            return Result::not_found;
        const auto size = input.tellg();
        if (size < 0)
            return Result::invalid_argument;
        bytes.resize(static_cast<std::size_t>(size));
        input.seekg(0);
        input.read(reinterpret_cast<char*>(bytes.data()), size);
        return input ? Result::ok : Result::invalid_argument;
    }

  private:
    std::filesystem::path root_;
};

class AnimationRenderer final : public Renderer {
  public:
    std::uint64_t next{1};
    std::map<std::uint64_t, Transform> poses;
    std::map<std::uint64_t, bool> billboard_materials, billboard_meshes, billboards;
    std::map<std::uint64_t, unsigned> updates;
    std::map<std::uint64_t, bool> static_instances;
    MeshHandle upload_mesh(std::span<const Vec3>, std::span<const std::uint32_t>) override {
        return {next++};
    }
    MeshHandle upload_surface(std::span<const Vec3>, std::span<const std::array<float, 2>>,
                              std::span<const std::uint32_t>, MaterialHandle material,
                              std::span<const Vec3>,
                              std::span<const std::array<float, 4>>) override {
        const auto id = next++;
        billboard_meshes[id] = billboard_materials[material.value];
        return {id};
    }
    InstanceHandle create_instance(MeshHandle mesh) override {
        auto id = next++;
        poses[id] = {};
        billboards[id] = billboard_meshes[mesh.value];
        return {id};
    }
    Result update_instance(InstanceHandle h, const Transform& p) override {
        poses[h.value] = p;
        ++updates[h.value];
        return Result::ok;
    }
    void destroy_instance(InstanceHandle h) override {
        poses.erase(h.value);
        billboards.erase(h.value);
        updates.erase(h.value);
        static_instances.erase(h.value);
    }
    void set_instance_static(InstanceHandle h, bool value) override {
        static_instances[h.value] = value;
    }
    void destroy_mesh(MeshHandle) override {}
    void update_camera(const Transform&, float) override {}
    TextureHandle upload_texture(std::uint32_t, std::uint32_t,
                                 std::span<const std::byte>) override {
        return {next++};
    }
    MaterialHandle create_material(TextureHandle, const MaterialSettings& settings) override {
        const auto id = next++;
        billboard_materials[id] = settings.billboard_y;
        return {id};
    }
    Result configure_sky(std::span<const Vec3>, std::span<const std::array<float, 2>>,
                         std::span<const std::uint32_t>, MaterialHandle) override {
        return Result::ok;
    }
};
int main(int argc, char** argv) {
    if (argc != 2)
        return 2;
    RenderProbeReader reader(argv[1]);
    ResourceStore resources(reader);
    if (resources.open() != Result::ok)
        return 1;
    AnimationRenderer renderer;
    RacePresentation race(renderer);
    std::string error;
    for (const auto track : {"Dark Forest", "Underground"}) {
        if (race.load(resources, track, "Animal", error) != Result::ok) {
            std::cerr << error;
            return 1;
        }
        const auto initial_updates = renderer.updates;
        race.update_vehicle({1, 2, 3}, 45);
        race.apply_camera(original_camera_frame({3, 4, 5}, {}, 123, 25));
        unsigned sprites = 0;
        for (const auto& [id, billboard] : renderer.billboards)
            if (billboard) {
                ++sprites;
                if (!renderer.static_instances[id] ||
                    renderer.updates[id] != initial_updates.at(id))
                    return 1;
            }
        if (std::string_view(track) == "Dark Forest" && sprites < 200)
            return 1;
        std::cout << track << ": GPU billboard surfaces=" << sprites << '\n';
        race.update_track_animation(0);
        const auto before = renderer.poses;
        race.update_track_animation(1);
        unsigned changed = 0;
        for (const auto& [id, p] : renderer.poses) {
            const auto& old = before.at(id);
            if (p.basis[0].x != old.basis[0].x || p.basis[1].y != old.basis[1].y ||
                p.basis[2].z != old.basis[2].z) {
                ++changed;
                if (p.origin.x != old.origin.x || p.origin.y != old.origin.y ||
                    p.origin.z != old.origin.z)
                    return 1;
            }
        }
        std::cout << track << ": animated surfaces=" << changed << '\n';
        if (changed == 0)
            return 1;
        race.stop();
        if (!renderer.poses.empty())
            return 1;
    }
}
