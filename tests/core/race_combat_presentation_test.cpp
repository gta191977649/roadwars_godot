#include "game_src/unresolved/race_presentation.hpp"
#include <map>
#include <stdexcept>
using namespace roadwars;
namespace {
void require(bool value) {
    if (!value)
        throw std::runtime_error("Combat presentation lifecycle contract failed");
}
class Reader final : public ResourceReader {
    Result read(std::string_view, std::vector<std::byte>&) override { return Result::not_found; }
};
class Render final : public Renderer {
  public:
    struct Instance {
        Transform pose;
        bool visible{true};
    };
    std::map<std::uint64_t, Instance> instances;
    std::size_t meshes{}, materials{}, textures{};
    std::uint64_t next{1};
    MeshHandle upload_mesh(std::span<const Vec3>, std::span<const std::uint32_t>) override {
        ++meshes;
        return {next++};
    }
    MeshHandle upload_surface(std::span<const Vec3> vertices, std::span<const std::array<float, 2>>,
                              std::span<const std::uint32_t> indices, MaterialHandle,
                              std::span<const Vec3>,
                              std::span<const std::array<float, 4>>) override {
        return upload_mesh(vertices, indices);
    }
    InstanceHandle create_instance(MeshHandle) override {
        const auto id = next++;
        instances[id] = {};
        return {id};
    }
    Result update_instance(InstanceHandle id, const Transform& pose) override {
        instances.at(id.value).pose = pose;
        return Result::ok;
    }
    void set_instance_visible(InstanceHandle id, bool visible) override {
        instances.at(id.value).visible = visible;
    }
    void destroy_instance(InstanceHandle id) override { require(instances.erase(id.value) == 1); }
    void destroy_mesh(MeshHandle) override {
        require(instances.empty());
        --meshes;
    }
    void update_camera(const Transform&, float) override {}
    TextureHandle upload_texture(std::uint32_t, std::uint32_t,
                                 std::span<const std::byte>) override {
        ++textures;
        return {next++};
    }
    MaterialHandle create_material(TextureHandle, const MaterialSettings&) override {
        ++materials;
        return {next++};
    }
    void destroy_material(MaterialHandle) override {
        require(meshes == 0);
        --materials;
    }
    void destroy_texture(TextureHandle) override {
        require(materials == 0);
        --textures;
    }
};
} // namespace
int main() {
    Reader reader;
    ResourceStore resources(reader);
    Render renderer;
    {
        RacePresentation presentation(renderer);
        MeshObject mesh;
        mesh.position = {2, 0, 0};
        MeshObject child;
        child.position = {0, 3, 0};
        MeshPart triangle;
        triangle.positions = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
        triangle.indices = {0, 1, 2};
        child.parts.push_back(triangle);
        mesh.children.push_back(child);
        std::size_t id{};
        std::string error;
        require(presentation.add_combat_visual(resources, mesh, "Fixture", id, error) ==
                Result::ok);
        require(renderer.instances.size() == 1 && !renderer.instances.begin()->second.visible);
        Transform pose;
        pose.origin = {10, 20, 30};
        pose.basis = {{{0, 0, -1}, {0, 1, 0}, {1, 0, 0}}};
        require(presentation.update_combat_visual(id, pose, true) == Result::ok);
        const auto& instance = renderer.instances.begin()->second;
        require(instance.visible && instance.pose.origin.x == 10 && instance.pose.origin.y == 23 &&
                instance.pose.origin.z == 28);
        require(presentation.update_combat_visual(id, pose, false) == Result::ok);
        require(!instance.visible);
        require(presentation.update_combat_visual(id + 1, pose, true) == Result::invalid_argument);
        presentation.stop();
        presentation.stop();
    }
    require(renderer.instances.empty() && renderer.meshes == 0 && renderer.materials == 0 &&
            renderer.textures == 0);
}
