#include "game_src/core/runtime.hpp"
#include <cmath>

namespace roadwars {
Runtime::~Runtime() { stop(); }
Result Runtime::start_smoke_scene() {
    if (instance_.value != 0) return Result::ok;
    const std::array<Vec3, 3> vertices{{{-1, -1, 0}, {1, -1, 0}, {0, 1, 0}}};
    const std::array<std::uint32_t, 3> indices{{0, 1, 2}};
    mesh_ = renderer_.upload_mesh(vertices, indices);
    if (mesh_.value == 0) return Result::unavailable;
    instance_ = renderer_.create_instance(mesh_);
    if (instance_.value == 0) { stop(); return Result::unavailable; }
    Transform camera;
    camera.origin.z = 3;
    renderer_.update_camera(camera, 70);
    return Result::ok;
}
Result Runtime::update(double delta_seconds, const InputState& input) {
    if (!std::isfinite(delta_seconds) || delta_seconds < 0) return Result::invalid_argument;
    if (input.quit_requested) { stop(); return Result::ok; }
    if (instance_.value == 0) return Result::unavailable;
    return renderer_.update_instance(instance_, Transform{});
}
void Runtime::stop() {
    if (instance_.value != 0) renderer_.destroy_instance(instance_);
    if (mesh_.value != 0) renderer_.destroy_mesh(mesh_);
    instance_ = {};
    mesh_ = {};
}
} // namespace roadwars
