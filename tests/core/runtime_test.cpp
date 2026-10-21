#include "game_src/core/runtime.hpp"
#include <limits>
#include <stdexcept>

using namespace roadwars;
void require(bool value) { if (!value) throw std::runtime_error("Core contract failed"); }
class RecordingRenderer final : public Renderer {
  public:
    int meshes{}, instances{}, uploads{}, updates{};
    bool fail_instance{};
    MeshHandle upload_mesh(std::span<const Vec3> vertices, std::span<const std::uint32_t> indices) override {
        require(vertices.size() == 3 && indices.size() == 3);
        ++uploads; ++meshes; return {1};
    }
    InstanceHandle create_instance(MeshHandle) override {
        if (fail_instance) return {};
        ++instances; return {2};
    }
    Result update_instance(InstanceHandle, const Transform&) override { ++updates; return Result::ok; }
    void destroy_instance(InstanceHandle) override { --instances; }
    void destroy_mesh(MeshHandle) override { require(instances == 0); --meshes; }
    void update_camera(const Transform& transform, float) override { require(transform.origin.z == 3); }
};
int main() {
    RecordingRenderer renderer;
    {
        Runtime runtime(renderer);
        require(runtime.update(0, {}) == Result::unavailable);
        for (int cycle = 0; cycle < 3; ++cycle) {
            require(runtime.start_smoke_scene() == Result::ok);
            require(runtime.start_smoke_scene() == Result::ok);
            require(runtime.update(0.016, {}) == Result::ok);
            require(runtime.update(-1, {}) == Result::invalid_argument);
            require(runtime.update(std::numeric_limits<double>::quiet_NaN(), {}) == Result::invalid_argument);
            runtime.stop(); runtime.stop();
            require(renderer.instances == 0 && renderer.meshes == 0);
        }
        renderer.fail_instance = true;
        require(runtime.start_smoke_scene() == Result::unavailable);
        require(renderer.meshes == 0);
    }
    require(renderer.uploads == 4 && renderer.updates == 3);
}
