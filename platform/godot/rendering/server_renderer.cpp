#include "platform/godot/rendering/server_renderer.hpp"
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <stdexcept>

namespace roadwars {
namespace {
godot::Transform3D convert(const Transform& input) {
    godot::Transform3D result;
    for (int axis = 0; axis < 3; ++axis) {
        const auto& vector = input.basis[axis];
        result.basis.set_column(axis, {vector.x, vector.y, vector.z});
    }
    result.origin = {input.origin.x, input.origin.y, input.origin.z};
    return result;
}
} // namespace
ServerRenderer::ServerRenderer(godot::RID viewport)
    : server_(godot::RenderingServer::get_singleton()), viewport_(viewport) {
    if (!server_ || !viewport.is_valid()) throw std::runtime_error("RenderingServer unavailable");
    scenario_ = server_->scenario_create();
    camera_ = server_->camera_create();
    shader_ = server_->shader_create();
    material_ = server_->material_create();
    server_->shader_set_code(shader_, "shader_type spatial; render_mode unshaded, cull_disabled; void fragment() { ALBEDO = vec3(0.15, 0.8, 0.55); }");
    server_->material_set_shader(material_, shader_);
    server_->viewport_set_scenario(viewport_, scenario_);
    server_->viewport_attach_camera(viewport_, camera_);
}
ServerRenderer::~ServerRenderer() {
    server_->viewport_attach_camera(viewport_, {});
    server_->viewport_set_scenario(viewport_, {});
    for (auto [key, rid] : instances_) server_->free_rid(rid);
    for (auto [key, rid] : meshes_) server_->free_rid(rid);
    server_->free_rid(material_);
    server_->free_rid(shader_);
    server_->free_rid(camera_);
    server_->free_rid(scenario_);
}
MeshHandle ServerRenderer::upload_mesh(std::span<const Vec3> vertices,
                                       std::span<const std::uint32_t> indices) {
    if (vertices.empty() || indices.empty() || indices.size() % 3 != 0) return {};
    for (auto index : indices) if (index >= vertices.size()) return {};
    godot::PackedVector3Array positions;
    for (const auto& v : vertices) positions.push_back({v.x, v.y, v.z});
    godot::PackedInt32Array triangles;
    for (auto index : indices) triangles.push_back(static_cast<std::int32_t>(index));
    godot::Array arrays;
    arrays.resize(godot::RenderingServer::ARRAY_MAX);
    arrays[godot::RenderingServer::ARRAY_VERTEX] = positions;
    arrays[godot::RenderingServer::ARRAY_INDEX] = triangles;
    const auto rid = server_->mesh_create();
    server_->mesh_add_surface_from_arrays(rid, godot::RenderingServer::PRIMITIVE_TRIANGLES, arrays);
    server_->mesh_surface_set_material(rid, 0, material_);
    const auto id = next_handle_++;
    meshes_.emplace(id, rid);
    return {id};
}
InstanceHandle ServerRenderer::create_instance(MeshHandle mesh) {
    auto found = meshes_.find(mesh.value);
    if (found == meshes_.end()) return {};
    auto rid = server_->instance_create2(found->second, scenario_);
    const auto id = next_handle_++;
    instances_.emplace(id, rid);
    return {id};
}
Result ServerRenderer::update_instance(InstanceHandle instance, const Transform& transform) {
    auto found = instances_.find(instance.value);
    if (found == instances_.end()) return Result::not_found;
    server_->instance_set_transform(found->second, convert(transform));
    return Result::ok;
}
void ServerRenderer::destroy_instance(InstanceHandle instance) {
    auto found = instances_.find(instance.value);
    if (found != instances_.end()) { server_->free_rid(found->second); instances_.erase(found); }
}
void ServerRenderer::destroy_mesh(MeshHandle mesh) {
    auto found = meshes_.find(mesh.value);
    if (found != meshes_.end()) { server_->free_rid(found->second); meshes_.erase(found); }
}
void ServerRenderer::update_camera(const Transform& transform, float fov) {
    server_->camera_set_transform(camera_, convert(transform));
    server_->camera_set_perspective(camera_, fov, 0.05f, 2000.0f);
}
std::size_t ServerRenderer::live_rids() const { return meshes_.size() + instances_.size() + 4; }
} // namespace roadwars
