#include "platform/godot/rendering/server_renderer.hpp"
#include "game_src/core/simulation_clock.hpp"
#include "game_src/shader/shader_variant.hpp"
#include "game_src/unresolved/track_render_settings.hpp"
#include "platform/godot/extension/frame_trace.hpp"
#include "platform/godot/rendering/coordinate_conversion.hpp"
#include "shader_sources.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/variant/aabb.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/packed_vector4_array.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <iomanip>
#include <limits>
#include <sstream>
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
std::string ServerRenderer::filtered_surface_code(const std::string& source) const {
    auto code = source;
    const std::string filter = graphics_settings_.texture_filtering == TextureFiltering::linear
        ? "filter_linear" : "filter_linear_mipmap_anisotropic";
    // Keep the original chrome lookup linear; filtering applies to world paint/base textures.
    for (const auto* sampler : {"diffuse_map : source_color, ", "chrome_base_map : "}) {
        const std::string needle = std::string(sampler) + "filter_linear";
        const auto offset = code.find(needle);
        if (offset != std::string::npos)
            code.replace(offset, needle.size(), std::string(sampler) + filter);
    }
    return code;
}
bool ServerRenderer::test_graphics_quality() {
    auto* server = godot::RenderingServer::get_singleton();
    const auto viewport = server->viewport_create();
    bool valid = true;
    {
        ServerRenderer fixture(viewport);
        std::vector<std::byte> pixels(16 * 8 * 4, std::byte{127});
        const auto texture = fixture.upload_texture(16, 8, pixels);
        MaterialSettings material;
        material.chrome_map = texture;
        const auto handle = fixture.create_material(texture, material);
        const auto world = fixture.world_textures_.at(texture.value);
        for (const auto quality :
             {TextureQuality::low, TextureQuality::medium, TextureQuality::high}) {
            GraphicsSettings settings;
            settings.texture_quality = quality;
            settings.texture_filtering = TextureFiltering::anisotropic_8x;
            settings.aa = AntiAliasing::msaa_4x;
            settings.render_scale = .75f;
            valid &= fixture.apply_graphics_quality(settings) == Result::ok;
            valid &= fixture.world_textures_.at(texture.value) == world;
            const auto original = fixture.texture_sources_.at(texture.value);
            valid &= original->get_width() == 16 && original->get_height() == 8;
            const auto code = fixture.filtered_surface_code(
                fixture.material_shader_sources_.at(handle.value));
            valid &= code.find("chrome_base_map : filter_linear_mipmap_anisotropic") !=
                     std::string::npos;
            valid &= code.find("chrome_map : filter_linear, repeat_enable") != std::string::npos;
            if (godot::DisplayServer::get_singleton()->get_name() != "headless") {
                const auto gpu = server->texture_2d_get(world);
                const int divisor = quality == TextureQuality::low ? 4
                    : quality == TextureQuality::medium ? 2 : 1;
                valid &= gpu.is_valid() && gpu->get_width() == 16 / divisor &&
                         gpu->get_height() == 8 / divisor && gpu->has_mipmaps();
            }
        }
        valid &= fixture.update_texture(texture, 16, 8, pixels) == Result::ok;
        const std::array<Vec3, 3> positions{{{-1, 0, 0}, {1, 0, 0}, {0, 1, 0}}};
        const std::array<Vec3, 3> normals{{{0, 0, 1}, {0, 0, 1}, {0, 0, 1}}};
        const std::array<std::array<float, 2>, 3> uv{{{0, 0}, {1, 0}, {0, 1}}};
        const std::array<std::uint32_t, 3> indices{0, 1, 2};
        const auto mesh = fixture.upload_surface(positions, uv, indices, handle, normals, {});
        const auto first = fixture.create_instance(mesh), second = fixture.create_instance(mesh);
        valid &= fixture.set_instance_vertex_color(first, 0xffff0000) == Result::ok;
        valid &= fixture.set_instance_vertex_color(second, 0xff0000ff) == Result::ok;
        if (godot::DisplayServer::get_singleton()->get_name() != "headless") {
            const auto red = fixture.instance_color_materials_.at(first.value).material;
            const auto blue = fixture.instance_color_materials_.at(second.value).material;
            valid &= red != blue;
            const godot::Vector4 a = server->material_get_param(red, "original_vertex_override");
            const godot::Vector4 b = server->material_get_param(blue, "original_vertex_override");
            valid &= a.x == 1 && a.z == 0 && b.x == 0 && b.z == 1;
        }
        fixture.destroy_instance(first);
        valid &= fixture.instance_color_materials_.size() == 1;
        valid &= fixture.set_instance_mesh(second, mesh) == Result::ok;
        valid &= fixture.instance_color_materials_.empty();
        fixture.destroy_instance(second);
        fixture.destroy_mesh(mesh);
        fixture.destroy_material(handle);
        fixture.destroy_texture(texture);
        valid &= fixture.world_textures_.empty() && fixture.texture_sources_.empty();
    }
    server->free_rid(viewport);
    return valid;
}
void ServerRenderer::refresh_world_texture(std::uint64_t texture) {
    const auto source = textures_.find(texture);
    const auto target = world_textures_.find(texture);
    if (source == textures_.end() || target == world_textures_.end())
        return;
    // Retain full-resolution canvas textures and regenerate only their world-only counterpart.
    const auto original = texture_sources_.at(texture);
    auto image = godot::Image::create_from_data(original->get_width(), original->get_height(),
        false, godot::Image::FORMAT_RGBA8, original->get_data());
    if (image.is_null())
        throw std::runtime_error("Cannot read source texture for graphics quality");
    const int divisor = graphics_settings_.texture_quality == TextureQuality::low ? 4
        : graphics_settings_.texture_quality == TextureQuality::medium ? 2 : 1;
    if (divisor != 1)
        image->resize(std::max(1, image->get_width() / divisor),
                      std::max(1, image->get_height() / divisor), godot::Image::INTERPOLATE_LANCZOS);
    if (image->generate_mipmaps() != godot::OK)
        throw std::runtime_error("Cannot generate world texture mipmaps");
    const auto replacement = server_->texture_2d_create(image);
    server_->texture_replace(target->second, replacement);
}
godot::RID ServerRenderer::world_texture(TextureHandle texture) {
    auto found = world_textures_.find(texture.value);
    if (found != world_textures_.end())
        return found->second;
    const auto image = texture_sources_.at(texture.value);
    if (image.is_null())
        throw std::runtime_error("Cannot read world texture source");
    const auto rid = server_->texture_2d_create(image);
    world_textures_.emplace(texture.value, rid);
    refresh_world_texture(texture.value);
    return rid;
}
Result ServerRenderer::apply_graphics_quality(const GraphicsSettings& settings) {
    if (!std::isfinite(settings.render_scale) || settings.render_scale < .5f ||
        settings.render_scale > 1.f ||
        static_cast<unsigned>(settings.aa) > static_cast<unsigned>(AntiAliasing::msaa_8x) ||
        static_cast<unsigned>(settings.texture_quality) > static_cast<unsigned>(TextureQuality::low) ||
        static_cast<unsigned>(settings.texture_filtering) >
            static_cast<unsigned>(TextureFiltering::anisotropic_16x))
        return Result::invalid_argument;
    using RS = godot::RenderingServer;
    auto msaa = RS::VIEWPORT_MSAA_DISABLED;
    switch (settings.aa) {
    case AntiAliasing::msaa_2x: msaa = RS::VIEWPORT_MSAA_2X; break;
    case AntiAliasing::msaa_4x: msaa = RS::VIEWPORT_MSAA_4X; break;
    case AntiAliasing::msaa_8x: msaa = RS::VIEWPORT_MSAA_8X; break;
    default: break;
    }
    server_->viewport_set_msaa_3d(viewport_, msaa);
    if (server_->get_current_rendering_method() == "forward_plus")
        server_->viewport_set_use_taa(viewport_, false);
    server_->viewport_set_screen_space_aa(viewport_, settings.aa == AntiAliasing::fxaa
        ? RS::VIEWPORT_SCREEN_SPACE_AA_FXAA : RS::VIEWPORT_SCREEN_SPACE_AA_DISABLED);
    server_->viewport_set_scaling_3d_mode(viewport_, RS::VIEWPORT_SCALING_3D_MODE_BILINEAR);
    server_->viewport_set_scaling_3d_scale(viewport_, settings.render_scale);
    server_->viewport_set_anisotropic_filtering_level(viewport_,
        static_cast<RS::ViewportAnisotropicFiltering>(settings.texture_filtering));
    const bool textures_changed = settings.texture_quality != graphics_settings_.texture_quality;
    const bool filters_changed = settings.texture_filtering != graphics_settings_.texture_filtering;
    graphics_settings_ = settings;
    if (textures_changed)
        for (const auto& [id, rid] : world_textures_)
            refresh_world_texture(id);
    if (filters_changed)
        for (const auto& [id, code] : material_shader_sources_)
            server_->shader_set_code(material_shaders_.at(id),
                                     godot::String(filtered_surface_code(code).c_str()));
    return Result::ok;
}
ServerRenderer::ServerRenderer(godot::RID viewport, bool original_coordinates)
    : server_(godot::RenderingServer::get_singleton()), viewport_(viewport),
      original_coordinates_(original_coordinates) {
    if (!server_ || !viewport.is_valid())
        throw std::runtime_error("RenderingServer unavailable");
    scenario_ = server_->scenario_create();
    camera_ = server_->camera_create();
    shader_ = server_->shader_create();
    material_ = server_->material_create();
    server_->shader_set_code(shader_, "shader_type spatial; render_mode unshaded, cull_disabled; "
                                      "void fragment() { ALBEDO = vec3(0.15, 0.8, 0.55); }");
    server_->material_set_shader(material_, shader_);
    server_->viewport_set_scenario(viewport_, scenario_);
    server_->viewport_attach_camera(viewport_, camera_);
}
void ServerRenderer::set_instance_visible(InstanceHandle handle, bool visible) {
    if (visible)
        invisible_instances_.erase(handle.value);
    else
        invisible_instances_.insert(handle.value);
    const auto found = instances_.find(handle.value);
    if (found != instances_.end())
        server_->instance_set_visible(found->second, visible && !camera_hidden_.contains(handle.value));
}
void ServerRenderer::set_instance_camera_hidden(InstanceHandle handle, bool hidden) {
    if (hidden)
        camera_hidden_.insert(handle.value);
    else
        camera_hidden_.erase(handle.value);
    const auto found = instances_.find(handle.value);
    if (found != instances_.end())
        server_->instance_set_visible(found->second, !hidden && !invisible_instances_.contains(handle.value));
}
ServerRenderer::~ServerRenderer() {
    if (hud_canvas_.is_valid()) {
        server_->canvas_item_clear(hud_item_);
        server_->free_rid(hud_item_);
        server_->viewport_remove_canvas(viewport_, hud_canvas_);
        server_->free_rid(hud_canvas_);
    }
    server_->viewport_attach_camera(viewport_, {});
    server_->viewport_set_scenario(viewport_, {});
    while (!particle_systems_.empty())
        destroy_particles({particle_systems_.begin()->first});
    for (auto [key, rid] : particle_process_shaders_)
        server_->free_rid(rid);
    for (auto [key, rid] : particle_draw_shaders_)
        server_->free_rid(rid);
    for (auto [key, rid] : instances_)
        server_->free_rid(rid);
    for (const auto& [key, color] : instance_color_materials_)
        server_->free_rid(color.material);
    for (auto [key, rid] : meshes_)
        server_->free_rid(rid);
    for (auto [key, rid] : materials_)
        server_->free_rid(rid);
    for (auto [key, rid] : material_shaders_)
        server_->free_rid(rid);
    for (auto [key, rid] : textures_)
        server_->free_rid(rid);
    for (auto [key, rid] : world_textures_)
        server_->free_rid(rid);
    if (light_instance_.is_valid())
        server_->free_rid(light_instance_);
    if (light_.is_valid())
        server_->free_rid(light_);
    if (sky_.is_valid()) {
        server_->environment_set_sky(environment_, {});
        server_->free_rid(sky_);
    }
    // The dummy renderer can return no sky RID while allocating its material
    // and shader. Those resources still belong to this owner.
    if (sky_material_.is_valid())
        server_->free_rid(sky_material_);
    if (sky_shader_.is_valid())
        server_->free_rid(sky_shader_);
    if (environment_.is_valid()) {
        server_->scenario_set_environment(scenario_, {});
        server_->free_rid(environment_);
    }
    server_->free_rid(material_);
    server_->free_rid(shader_);
    server_->free_rid(camera_);
    server_->free_rid(scenario_);
}
Result ServerRenderer::draw_hud(std::span<const HudQuad> quads) {
    if (!hud_canvas_.is_valid()) {
        if (quads.empty())
            return Result::ok;
        hud_canvas_ = server_->canvas_create();
        hud_item_ = server_->canvas_item_create();
        server_->viewport_attach_canvas(viewport_, hud_canvas_);
        server_->viewport_set_canvas_stacking(viewport_, hud_canvas_, 10, 0);
        server_->canvas_item_set_parent(hud_item_, hud_canvas_);
        server_->canvas_item_set_default_texture_filter(
            hud_item_, godot::RenderingServer::CANVAS_ITEM_TEXTURE_FILTER_NEAREST);
    }
    server_->canvas_item_clear(hud_item_);
    aspect_hud_pixels_.clear();
    for (const auto& quad : quads) {
        const auto texture = textures_.find(quad.texture.value);
        if (texture == textures_.end())
            return Result::invalid_argument;
        godot::PackedVector2Array points, uv;
        godot::PackedColorArray colors;
        godot::PackedInt32Array indices;
        for (int i : {0, 1, 2, 2, 3, 0})
            indices.push_back(i);
        std::array<godot::Vector2, 4> pixels;
        for (int i = 0; i < 4; ++i) {
            float x = quad.positions[i][0], y = quad.positions[i][1];
            if (quad.preserve_aspect) {
                const float scale = std::min(hud_width_ / 640, hud_height_ / 480);
                x = (x * scale + (hud_width_ - 640 * scale) / 2) * 640 / hud_width_;
                y = (y * scale + hud_height_ - 480 * scale) * 480 / hud_height_;
            }
            pixels[i] = {x * hud_width_ / 640, y * hud_height_ / 480};
            points.push_back({x, y});
            uv.push_back({quad.uv[i][0], quad.uv[i][1]});
            const auto& c = quad.colors[i];
            colors.push_back({c[0], c[1], c[2], c[3]});
        }
        if (quad.preserve_aspect)
            aspect_hud_pixels_.push_back(pixels);
        server_->canvas_item_add_triangle_array(hud_item_, indices, points, colors, uv, {}, {},
                                                texture->second);
    }
    return Result::ok;
}
void ServerRenderer::resize_hud(float width, float height) {
    if (width > 0 && height > 0) {
        hud_width_ = width;
        hud_height_ = height;
    }
    if (!hud_canvas_.is_valid() || width <= 0 || height <= 0)
        return;
    godot::Transform2D transform;
    transform[0] = {width / 640, 0};
    transform[1] = {0, height / 480};
    server_->viewport_set_canvas_transform(viewport_, hud_canvas_, transform);
}
void ServerRenderer::set_hud_visible(bool visible) {
    if (hud_item_.is_valid())
        server_->canvas_item_set_visible(hud_item_, visible);
}
MeshHandle ServerRenderer::upload_mesh(std::span<const Vec3> vertices,
                                       std::span<const std::uint32_t> indices) {
    if (vertices.empty() || indices.empty() || indices.size() % 3 != 0)
        return {};
    for (auto index : indices)
        if (index >= vertices.size())
            return {};
    godot::PackedVector3Array positions;
    for (const auto& v : vertices)
        positions.push_back({v.x, v.y, v.z});
    godot::PackedInt32Array triangles;
    for (auto index : indices)
        triangles.push_back(static_cast<std::int32_t>(index));
    godot::Array arrays;
    arrays.resize(godot::RenderingServer::ARRAY_MAX);
    arrays[godot::RenderingServer::ARRAY_VERTEX] = positions;
    arrays[godot::RenderingServer::ARRAY_INDEX] = triangles;
    trace_resource(FrameResource::mesh);
    const auto rid = server_->mesh_create();
    server_->mesh_add_surface_from_arrays(rid, godot::RenderingServer::PRIMITIVE_TRIANGLES, arrays);
    server_->mesh_surface_set_material(rid, 0, material_);
    const auto id = next_handle_++;
    meshes_.emplace(id, rid);
    return {id};
}
InstanceHandle ServerRenderer::create_instance(MeshHandle mesh) {
    auto found = meshes_.find(mesh.value);
    if (found == meshes_.end())
        return {};
    trace_resource(FrameResource::instance);
    auto rid = server_->instance_create2(found->second, scenario_);
    const auto id = next_handle_++;
    instances_.emplace(id, rid);
    instance_meshes_.emplace(id, mesh.value);
    return {id};
}
Result ServerRenderer::update_instance(InstanceHandle instance, const Transform& transform) {
    auto found = instances_.find(instance.value);
    if (found == instances_.end())
        return Result::not_found;
    const auto pose =
        convert(original_coordinates_ ? original_object_to_godot(transform) : transform);
    if (!static_instances_.contains(instance.value)) {
        const auto cached =
            instance_poses_.try_emplace(instance.value, PoseHistory{pose, pose, {}}).first;
        cached->second.current = pose;
    }
    // Live simulation only records snapshots. Submit one coherent interpolated
    // world/camera in present_interpolated(), never raw catch-up poses.
    if (!interpolation_enabled_ || static_instances_.contains(instance.value)) {
        if (auto cached = instance_poses_.find(instance.value); cached != instance_poses_.end())
            cached->second.presented = pose;
        server_->instance_set_transform(found->second, pose);
    }
    return Result::ok;
}
Result ServerRenderer::set_instance_mesh(InstanceHandle instance, MeshHandle mesh) {
    const auto target = instances_.find(instance.value);
    const auto source = meshes_.find(mesh.value);
    if (target == instances_.end() || source == meshes_.end())
        return Result::not_found;
    server_->instance_set_base(target->second, source->second);
    instance_meshes_[instance.value] = mesh.value;
    if (const auto color = instance_color_materials_.find(instance.value);
        color != instance_color_materials_.end()) {
        server_->instance_geometry_set_material_override(target->second, {});
        server_->free_rid(color->second.material);
        instance_color_materials_.erase(color);
    }
    return Result::ok;
}
Result ServerRenderer::update_texture(TextureHandle texture, std::uint32_t width,
                                      std::uint32_t height, std::span<const std::byte> rgba) {
    const auto target = textures_.find(texture.value);
    if (target == textures_.end())
        return Result::not_found;
    if (!width || !height || rgba.size() != static_cast<std::size_t>(width) * height * 4)
        return Result::invalid_argument;
    godot::PackedByteArray pixels;
    pixels.resize(static_cast<std::int64_t>(rgba.size()));
    std::memcpy(pixels.ptrw(), rgba.data(), rgba.size());
    const auto image =
        godot::Image::create_from_data(width, height, false, godot::Image::FORMAT_RGBA8, pixels);
    server_->texture_2d_update(target->second, image, 0);
    texture_sources_[texture.value] = image;
    if (world_textures_.contains(texture.value))
        refresh_world_texture(texture.value);
    return Result::ok;
}
Result ServerRenderer::set_instance_vertex_color(InstanceHandle instance, std::uint32_t color) {
    const auto found = instances_.find(instance.value);
    if (found == instances_.end())
        return Result::not_found;
    auto override = instance_color_materials_.find(instance.value);
    if (override == instance_color_materials_.end()) {
        const auto mesh = instance_meshes_.find(instance.value);
        if (mesh == instance_meshes_.end())
            return Result::not_found;
        const auto source = mesh_materials_.find(mesh->second);
        if (source == mesh_materials_.end() || !materials_.contains(source->second))
            return Result::not_found;
        // Static world instances must not consume the Compatibility renderer's
        // limited global instance-uniform buffer. Only recolored effects own a copy.
        const auto material = server_->material_create();
        trace_resource(FrameResource::material);
        const auto shader = material_shaders_.at(source->second);
        server_->material_set_shader(material, shader);
        const auto parameters = server_->get_shader_parameter_list(shader);
        for (int i = 0; i < parameters.size(); ++i) {
            const godot::Dictionary parameter = parameters[i];
            const godot::StringName name = parameter["name"];
            const auto value = server_->material_get_param(materials_.at(source->second), name);
            if (value.get_type() != godot::Variant::NIL)
                server_->material_set_param(material, name, value);
        }
        override = instance_color_materials_.emplace(
            instance.value, ColorMaterial{material, source->second}).first;
        server_->instance_geometry_set_material_override(found->second, material);
    }
    server_->material_set_param(
        override->second.material, "original_vertex_override",
        godot::Vector4(((color >> 16) & 255) / 255.f, ((color >> 8) & 255) / 255.f,
                       (color & 255) / 255.f, ((color >> 24) & 255) / 255.f));
    return Result::ok;
}
void ServerRenderer::set_instance_static(InstanceHandle instance, bool is_static) {
    if (is_static) {
        static_instances_.insert(instance.value);
        instance_poses_.erase(instance.value);
    } else
        static_instances_.erase(instance.value);
}
void ServerRenderer::reset_instance_interpolation(InstanceHandle instance) {
    const auto found = instance_poses_.find(instance.value);
    if (found != instance_poses_.end())
        found->second.previous = found->second.current;
}
void ServerRenderer::destroy_instance(InstanceHandle instance) {
    camera_hidden_.erase(instance.value);
    invisible_instances_.erase(instance.value);
    static_instances_.erase(instance.value);
    instance_poses_.erase(instance.value);
    instance_meshes_.erase(instance.value);
    auto found = instances_.find(instance.value);
    if (found != instances_.end()) {
        server_->free_rid(found->second);
        instances_.erase(found);
    }
    if (const auto color = instance_color_materials_.find(instance.value);
        color != instance_color_materials_.end()) {
        server_->free_rid(color->second.material);
        instance_color_materials_.erase(color);
    }
}
void ServerRenderer::destroy_mesh(MeshHandle mesh) {
    mesh_materials_.erase(mesh.value);
    auto found = meshes_.find(mesh.value);
    if (found != meshes_.end()) {
        server_->free_rid(found->second);
        meshes_.erase(found);
    }
}
void ServerRenderer::update_camera(const Transform& transform, float fov) {
    camera_fov_ = fov;
    camera_pose_.current =
        convert(original_coordinates_ ? original_camera_to_godot(transform) : transform);
    if (!interpolation_enabled_) {
        camera_pose_.presented = camera_pose_.current;
        presented_camera_fov_ = fov;
        server_->camera_set_transform(camera_, camera_pose_.presented);
        server_->camera_set_perspective(
            camera_, fov, original_coordinates_ ? (first_person_ ? .1f : scene_settings_.near_distance) : 0.05f,
            original_coordinates_ ? scene_settings_.far_distance : 2000.0f);
    }
}
// Native test fixture: exercise the actual submission path at a known high speed,
// independently of track contacts, the recovered camera, or a GPU readback.
bool ServerRenderer::test_interpolation_history() {
    auto* server = godot::RenderingServer::get_singleton();
    const auto viewport = server->viewport_create();
    bool valid = true;
    try {
        ServerRenderer fixture(viewport);
        const std::array<Vec3, 3> vertices{{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}}};
        const std::array<std::uint32_t, 3> indices{0, 1, 2};
        const auto mesh = fixture.upload_mesh(vertices, indices);
        const auto body = fixture.create_instance(mesh);
        const auto wheel = fixture.create_instance(mesh);
        for (const int fps : {10, 15, 30, 59, 60, 75, 119, 120, 144, 165, 240}) {
            fixture.set_interpolation_enabled(false);
            Transform pose, mirrored;
            mirrored.basis[0].x = -1;
            fixture.update_instance(body, pose);
            fixture.update_instance(wheel, mirrored);
            auto camera = pose;
            camera.origin.z = -6;
            fixture.update_camera(camera, 50);
            fixture.set_interpolation_enabled(true);
            SimulationClock clock;
            for (int frame = 0; frame < fps * 2; ++frame) {
                const auto last_body = fixture.instance_poses_.at(body.value).presented;
                const auto last_camera = fixture.camera_pose_.presented;
                valid &= clock.advance(1.0 / fps, {}, [&](float, const InputState&) {
                    fixture.begin_simulation_tick();
                    pose.origin.z = static_cast<float>((clock.ticks() + 1) * 2);
                    mirrored.origin.z = pose.origin.z;
                    camera.origin.z = pose.origin.z - 6;
                    fixture.update_instance(body, pose);
                    fixture.update_instance(wheel, mirrored);
                    fixture.update_camera(camera, 50);
                    return Result::ok;
                }) == Result::ok;
                valid &= fixture.instance_poses_.at(body.value).presented == last_body;
                valid &= fixture.camera_pose_.presented == last_camera;
                fixture.present_interpolated(clock.interpolation_alpha());
                const auto position = static_cast<float>(
                    clock.ticks() == 0 ? 0 : (clock.ticks() - 1 + clock.interpolation_alpha()) * 2);
                valid &= std::abs(fixture.instance_poses_.at(body.value).presented.origin.z -
                                  position) < .0001f;
                valid &=
                    std::abs(fixture.camera_pose_.presented.origin.z - (position - 6)) < .0001f;
                valid &= fixture.instance_poses_.at(wheel.value).presented.basis.determinant() < 0;
            }
            // Stop updating after an in-between presentation: the final pose must settle.
            fixture.present_interpolated(.25);
            fixture.begin_simulation_tick();
            fixture.present_interpolated(0);
            valid &= fixture.instance_poses_.at(body.value).presented ==
                     fixture.instance_poses_.at(body.value).current;
            // Reset/teleport must snap at every render fraction.
            pose.origin.z = -100;
            fixture.update_instance(body, pose);
            fixture.reset_instance_interpolation(body);
            fixture.present_interpolated(.25);
            valid &= fixture.instance_poses_.at(body.value).presented.origin.z == -100;
            valid &= fixture.instance_poses_.at(wheel.value).presented.origin.z == 240;
            // Camera cuts and projection interpolation must not reset world histories.
            fixture.begin_simulation_tick();
            pose.origin.z = -98;
            fixture.update_instance(body, pose);
            camera.origin.z = 1000;
            fixture.update_camera(camera, 70);
            fixture.present_interpolated(.25);
            valid &= std::abs(fixture.presented_camera_fov_ - 55) < .0001f;
            fixture.reset_camera_interpolation();
            fixture.present_interpolated(.25);
            valid &= fixture.camera_pose_.presented.origin.z == 1000;
            valid &= std::abs(fixture.instance_poses_.at(body.value).presented.origin.z + 99.5f) <
                     .0001f;
        }
        fixture.destroy_instance(wheel);
        fixture.destroy_instance(body);
        fixture.destroy_mesh(mesh);
        const std::array<std::byte, 4> white{std::byte{255}, std::byte{255}, std::byte{255},
                                             std::byte{255}};
        const auto texture = fixture.upload_texture(1, 1, white);
        const auto material = fixture.create_material(texture, MaterialSettings{});
        fixture.prewarm_particles(15, material, false, false);
        const auto first = fixture.create_particles(15, material);
        const auto second = fixture.create_particles(15, material);
        const auto& a = fixture.particle_systems_.at(first.value);
        const auto& b = fixture.particle_systems_.at(second.value);
        const bool native = godot::DisplayServer::get_singleton()->get_name() != "headless";
        if (native) {
            valid &= a.process_shader == b.process_shader && a.draw_shader == b.draw_shader;
            valid &= a.process_material != b.process_material && a.draw_material != b.draw_material;
        }
        std::array<ParticleDrawState, 15> red{}, blue{};
        red[0] = {{1, 0, 0}, 1, 0xffff0000, 0, true};
        blue[0] = {{2, 0, 0}, 1, 0xff0000ff, 0, true};
        valid &= fixture.update_particles(first, red) == Result::ok;
        valid &= fixture.update_particles(second, blue) == Result::ok;
        const godot::PackedVector4Array first_positions =
            server->material_get_param(a.process_material, "slot_position");
        const godot::PackedVector4Array second_positions =
            server->material_get_param(b.process_material, "slot_position");
        // The headless dummy renderer does not retain material parameter arrays.
        // Exercise uniform isolation on the native renderer, without indexing
        // unavailable readbacks in the dependency-free timing smoke.
        if (native)
            valid &= first_positions.size() == 15 && second_positions.size() == 15 &&
                     first_positions[0].x == 1 && second_positions[0].x == 2;
        fixture.destroy_particles(first);
        valid &= fixture.update_particles(second, blue) == Result::ok;
        fixture.destroy_particles(second);

    } catch (...) {
        valid = false;
    }
    server->free_rid(viewport);
    return valid;
}
godot::Dictionary ServerRenderer::presentation_snapshot() const {
    const auto history = [](const PoseHistory& pose) {
        godot::Dictionary result;
        result["previous"] = pose.previous;
        result["current"] = pose.current;
        result["presented"] = pose.presented;
        return result;
    };
    godot::Dictionary result, instances;
    godot::Array quads;
    for (const auto& points : aspect_hud_pixels_) {
        godot::Array quad;
        for (const auto& p : points) quad.push_back(p);
        quads.push_back(quad);
    }
    result["aspect_hud_pixels"] = quads;
    result["camera_hidden_instances"] = static_cast<int>(camera_hidden_.size());
    result["camera_near"] = first_person_ ? .1f : scene_settings_.near_distance;
    result["chrome_materials"] = static_cast<int>(chrome_materials_.size());
    result["chrome_mapping_enabled"] = chrome_options_.enabled.value_or(false);
    for (const auto& [id, pose] : instance_poses_)
        instances[static_cast<int64_t>(id)] = history(pose);
    result["instances"] = instances;
    result["camera"] = history(camera_pose_);
    result["previous_fov"] = previous_camera_fov_;
    result["current_fov"] = camera_fov_;
    result["presented_fov"] = presented_camera_fov_;
    return result;
}
void ServerRenderer::begin_simulation_tick() {
    for (auto& [id, pose] : instance_poses_)
        pose.previous = pose.current;
    camera_pose_.previous = camera_pose_.current;
    previous_camera_fov_ = camera_fov_;
}
void ServerRenderer::set_paused(bool paused) {
    for (const auto& [id, system] : particle_systems_)
        server_->particles_set_speed_scale(system.particles, paused ? 0 : 1);
}
void ServerRenderer::reset_interpolation() { begin_simulation_tick(); }
void ServerRenderer::reset_camera_interpolation() {
    camera_pose_.previous = camera_pose_.current;
    previous_camera_fov_ = camera_fov_;
}
void ServerRenderer::set_interpolation_enabled(bool enabled) {
    if (interpolation_enabled_ == enabled)
        return;
    reset_interpolation();
    if (!enabled)
        present_interpolated(1);
    interpolation_enabled_ = enabled;
}
Vec3 ServerRenderer::interpolated_camera_position(double alpha) const {
    const auto pose =
        camera_pose_.previous.interpolate_with(camera_pose_.current, std::clamp(alpha, 0.0, 1.0));
    Vec3 position{pose.origin.x, pose.origin.y, pose.origin.z};
    return original_coordinates_ ? original_to_godot(position) : position;
}
void ServerRenderer::present_interpolated(double alpha) {
    const auto weight = static_cast<float>(std::clamp(alpha, 0.0, 1.0));
    for (auto& [id, pose] : instance_poses_) {
        // Also submit settled histories: their last displayed pose may still be
        // between the preceding snapshots when an owner stops updating.
        pose.presented = pose.previous.interpolate_with(pose.current, weight);
        server_->instance_set_transform(instances_.at(id), pose.presented);
    }
    const auto camera = camera_pose_.previous.interpolate_with(camera_pose_.current, weight);
    camera_pose_.presented = camera;
    server_->camera_set_transform(camera_, camera);
    presented_camera_fov_ = previous_camera_fov_ + (camera_fov_ - previous_camera_fov_) * weight;
    server_->camera_set_perspective(camera_, presented_camera_fov_, first_person_ ? .1f : scene_settings_.near_distance,
                                    scene_settings_.far_distance);
}
RaceDebugCamera ServerRenderer::race_debug_camera(double alpha) const {
    const auto pose =
        camera_pose_.previous.interpolate_with(camera_pose_.current, std::clamp(alpha, 0.0, 1.0));
    const auto vector = [&](godot::Vector3 v) {
        Vec3 value{v.x, v.y, v.z};
        return original_coordinates_ ? original_to_godot(value) : value;
    };
    return {vector(pose.origin),
            vector(pose.basis.get_column(0)),
            vector(pose.basis.get_column(1)),
            vector(-pose.basis.get_column(2)),
            previous_camera_fov_ + (camera_fov_ - previous_camera_fov_) *
                                       static_cast<float>(std::clamp(alpha, 0.0, 1.0)),
            first_person_ ? .1f : scene_settings_.near_distance};
}
std::size_t ServerRenderer::live_rids() const {
    return meshes_.size() + instances_.size() + textures_.size() + world_textures_.size() +
           materials_.size() + instance_color_materials_.size() +
           material_shaders_.size() + 4 + (environment_.is_valid() ? 1 : 0) +
           (light_.is_valid() ? 1 : 0) + (light_instance_.is_valid() ? 1 : 0) +
           (sky_.is_valid() ? 1 : 0) + (sky_material_.is_valid() ? 1 : 0) +
           (sky_shader_.is_valid() ? 1 : 0) + particle_systems_.size() * 5 +
           particle_process_shaders_.size() + particle_draw_shaders_.size() +
           (hud_canvas_.is_valid() ? 2 : 0);
}
TextureHandle ServerRenderer::upload_texture(std::uint32_t width, std::uint32_t height,
                                             std::span<const std::byte> rgba) {
    if (!width || !height || rgba.size() != static_cast<std::size_t>(width) * height * 4)
        return {};
    godot::PackedByteArray pixels;
    pixels.resize(static_cast<std::int64_t>(rgba.size()));
    std::memcpy(pixels.ptrw(), rgba.data(), rgba.size());
    const auto image =
        godot::Image::create_from_data(width, height, false, godot::Image::FORMAT_RGBA8, pixels);
    trace_resource(FrameResource::texture);
    const auto rid = server_->texture_2d_create(image);
    const auto id = next_handle_++;
    textures_.emplace(id, rid);
    texture_sources_.emplace(id, image);
    return {id};
}
MaterialHandle ServerRenderer::create_material(TextureHandle texture, std::array<float, 4> color,
                                               bool transparent) {
    MaterialSettings settings;
    settings.color = color;
    settings.unshaded = true;
    settings.alpha_mode = transparent ? AlphaMode::cutout : AlphaMode::opaque;
    return create_material(texture, settings);
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004dce9b
MaterialHandle ServerRenderer::create_material(TextureHandle texture,
                                               const MaterialSettings& settings) {
    const auto found = textures_.find(texture.value);
    if (found == textures_.end())
        return {};
    const auto chrome = textures_.find(settings.chrome_map.value);
    if (settings.chrome_map.value && chrome == textures_.end())
        return {};
    trace_resource(FrameResource::material);
    const auto rid = server_->material_create();
    const auto shader = server_->shader_create();
    std::string code;
    const auto define = [&](bool enabled, const char* name) {
        if (enabled)
            code += std::string("#define ") + name + "\n";
    };
    define(settings.unshaded, "RW_UNSHADED");
    define(settings.chrome_map.value != 0, "RW_CHROME");
    define(settings.depth_write && settings.depth_test, "RW_DEPTH_WRITE");
    define(!settings.depth_test, "RW_NO_DEPTH_TEST");
    define(settings.fog, "RW_FOG");
    define(settings.alpha_mode == AlphaMode::additive, "RW_ADDITIVE");
    define(settings.opaque_alpha_only, "RW_OPAQUE_ALPHA_ONLY");
    define(settings.billboard_y, "RW_BILLBOARD_Y");
    define(settings.alpha_mode == AlphaMode::blend || settings.alpha_mode == AlphaMode::additive ||
               (settings.alpha_mode == AlphaMode::cutout && !settings.opaque_alpha_only),
           "RW_ALPHA");
    code = shaders::shader_variant(shaders::original_surface, code);
    const auto source_code = code;
    server_->shader_set_code(shader, godot::String(filtered_surface_code(code).c_str()));
    server_->material_set_shader(rid, shader);
    const auto world = world_texture(texture);
    server_->material_set_param(rid, "diffuse_map", world);
    if (settings.chrome_map.value) {
        server_->material_set_param(rid, "chrome_map", chrome->second);
        server_->material_set_param(rid, "chrome_base_map", world);
        server_->material_set_param(rid, "chrome_material_color",
            godot::Color(settings.color[0], settings.color[1], settings.color[2], settings.color[3]));
        const auto direction = original_coordinates_
            ? original_to_godot(scene_settings_.directional_light) : scene_settings_.directional_light;
        server_->material_set_param(rid, "chrome_light_direction",
                                    godot::Vector3(direction.x, direction.y, direction.z));
        server_->material_set_param(rid, "chrome_ambient", scene_settings_.ambient_light);
        server_->material_set_param(rid, "chrome_fog_color",
            godot::Vector3(scene_settings_.fog_color.x, scene_settings_.fog_color.y,
                           scene_settings_.fog_color.z));
    }
    server_->material_set_param(
        rid, "tint",
        godot::Color(settings.color[0], settings.color[1], settings.color[2], settings.color[3]));
    if (settings.fog) {
        server_->material_set_param(rid, "original_fog_color",
                                    godot::Color(scene_settings_.fog_color.x,
                                                 scene_settings_.fog_color.y,
                                                 scene_settings_.fog_color.z));
        server_->material_set_param(rid, "original_fog_start", scene_settings_.far_distance * .7f);
        server_->material_set_param(rid, "original_fog_end", scene_settings_.far_distance);
    }
    const auto id = next_handle_++;
    materials_.emplace(id, rid);
    if (settings.chrome_map.value)
        chrome_materials_.insert(id);
    if (settings.billboard_y)
        billboard_materials_.insert(id);
    material_textures_.emplace(id, world);
    material_shaders_.emplace(id, shader);
    material_shader_sources_.emplace(id, source_code);
    return {id};
}
void ServerRenderer::create_fog_fixture(float aspect) {
    SceneSettings scene;
    scene.far_distance = 200;
    configure_scene(scene);
    const std::array<std::byte, 4> white{std::byte{255}, std::byte{255}, std::byte{255},
                                         std::byte{255}};
    const auto texture = upload_texture(1, 1, white);
    for (int row = 0; row < 2; ++row)
        for (int i = 0; i < 4; ++i) {
            const float depth = std::array<float, 4>{100, 155, 185, 199}[i];
            const float half_height = depth * std::tan(25.0f * .0174532925199433f);
            const float x = (i * .4f - .6f) * half_height * aspect;
            const float y = (row == 0 ? .4f : -.4f) * half_height;
            const float dx = .12f * half_height * aspect, dy = .2f * half_height;
            const std::array<Vec3, 4> vertices{{{x - dx, y - dy, -depth},
                                                {x + dx, y - dy, -depth},
                                                {x + dx, y + dy, -depth},
                                                {x - dx, y + dy, -depth}}};
            const std::array<std::array<float, 2>, 4> uv{};
            // Godot uses clockwise front faces; fixture coordinates are right handed.
            const std::array<std::uint32_t, 6> indices{0, 2, 1, 0, 3, 2};
            MaterialSettings material;
            material.unshaded = true;
            material.fog = row == 0;
            const auto mesh =
                upload_surface(vertices, uv, indices, create_material(texture, material));
            create_instance(mesh);
        }
    update_camera({}, 50);
}
void ServerRenderer::create_billboard_fixture() {
    SceneSettings settings;
    settings.far_distance = 100;
    configure_scene(settings);
    const std::array<std::byte, 8> colors{std::byte{255}, std::byte{0},  std::byte{0},
                                          std::byte{255}, std::byte{0},  std::byte{0},
                                          std::byte{255}, std::byte{255}};
    const auto texture = upload_texture(2, 1, colors);
    MaterialSettings material;
    material.unshaded = true;
    material.fog = false;
    material.billboard_y = true;
    const auto handle = create_material(texture, material);
    // Same +Z winding and decreasing-X U direction as original tree sprites.
    const std::array<Vec3, 4> positions{
        {{1.5f, -1, .35f}, {-1.5f, -1, .35f}, {-1.5f, 1, .35f}, {1.5f, 1, .35f}}};
    const std::array<std::array<float, 2>, 4> uv{
        {{.25f, .5f}, {.75f, .5f}, {.75f, .5f}, {.25f, .5f}}};
    const std::array<std::uint32_t, 6> indices{2, 1, 0, 0, 3, 2};
    const auto mesh = upload_surface(positions, uv, indices, handle);
    const auto instance = create_instance(mesh);
    set_instance_static(instance, true);
    update_instance(instance, {});
}
godot::Dictionary ServerRenderer::create_chrome_fixture(float aspect) {
    SceneSettings scene;
    scene.near_distance = .1f;
    scene.far_distance = 5;
    scene.ambient_light = .25f;
    scene.directional_light = {0, 0, 1};
    scene.fog_color = {16.f / 255, 48.f / 255, 80.f / 255};
    configure_scene(scene);
    const std::array<std::byte, 4> base_pixels{
        std::byte{64}, std::byte{32}, std::byte{16}, std::byte{255}};
    const std::array<std::byte, 16> chrome_pixels{
        std::byte{64}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{96}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{128}, std::byte{0},
        std::byte{32}, std::byte{48}, std::byte{64}, std::byte{0}};
    const auto base = upload_texture(1, 1, base_pixels);
    const auto chrome = upload_texture(2, 2, chrome_pixels);
    const std::array<std::byte, 4> bright_pixels{
        std::byte{224}, std::byte{208}, std::byte{192}, std::byte{255}};
    const auto bright = upload_texture(1, 1, bright_pixels);
    const float half_y = 4.f * std::tan(25.f * .0174532925199433f);
    const float half_x = half_y * aspect;
    godot::Array samples;
    const std::array<Vec3, 4> corners{
        {{-.5f, .5f, 1}, {.5f, .5f, 1}, {-.5f, -.5f, 1}, {.5f, -.5f, 1}}};
    const std::array<Vec3, 4> reflection{
        {{64, 0, 0}, {0, 96, 0}, {0, 0, 128}, {32, 48, 64}}};
    for (int i = 0; i < 12; ++i) {
        const float x = (i % 4 + .5f) / 4, y = (i / 4 + .5f) / 3;
        const float dx = half_x * .17f, dy = half_y * .23f;
        const std::array<Vec3, 4> positions{
            {{-dx, -dy, 0}, {dx, -dy, 0}, {dx, dy, 0}, {-dx, dy, 0}}};
        std::array<Vec3, 4> normals;
        normals.fill(corners[i < 4 ? i : 0]);
        Vec3 added = reflection[i < 4 ? i : 0];
        if (i == 4) { normals.fill({2.5f, .5f, 1}); added = reflection[1]; }
        if (i == 5) { normals.fill({-.5f, -2.5f, 1}); added = reflection[2]; }
        if (i == 6) {
            normals = {{{-.5f, -.5f, 1}, {.5f, -.5f, 1},
                         {.5f, .5f, 1}, {-.5f, .5f, 1}}};
            added = {24, 36, 48};
        }
        if (i == 7) added = reflection[2];
        if (i == 8) { normals.fill({0, 0, 1}); added = {24, 36, 48}; }
        MaterialSettings settings;
        settings.unshaded = i != 8;
        settings.fog = i == 9;
        settings.chrome_map = i == 10 ? TextureHandle{} : chrome;
        const std::array<std::array<float, 2>, 4> uv{};
        const std::array<std::uint32_t, 6> indices{0, 2, 1, 0, 3, 2};
        std::array<std::array<float, 4>, 4> colors;
        colors.fill({1, 1, 1, 1});
        const auto material = create_material(i == 11 ? bright : base, settings);
        const auto mesh = upload_surface(positions, uv, indices, material, normals, colors);
        const auto instance = create_instance(mesh);
        Transform pose;
        pose.origin = {(2 * x - 1) * half_x, (1 - 2 * y) * half_y, -4};
        if (i == 7) pose.basis = {{{0, 1, 0}, {-1, 0, 0}, {0, 0, 1}}};
        update_instance(instance, pose);
        Vec3 expected{64 + added.x, 32 + added.y, 16 + added.z};
        if (i == 8) expected = {16 + added.x, 8 + added.y, 4 + added.z};
        if (i == 9) expected = {(expected.x * 2 + 16) / 3,
                                (expected.y * 2 + 48) / 3, (expected.z * 2 + 80) / 3};
        if (i == 10) expected = {64, 32, 16};
        if (i == 11) expected = {255, 208, 192};
        godot::Dictionary sample;
        sample["position"] = godot::Vector2(x, y);
        sample["color"] = godot::Color(expected.x / 255, expected.y / 255, expected.z / 255);
        sample["label"] = godot::String::num_int64(i);
        samples.push_back(sample);
    }
    update_camera({}, 50);
    godot::Dictionary result;
    result["samples"] = samples;
    return result;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004dbeb5
// Original modern-profile vertex fog only; legacy capability fallback remains outside this adapter.
void ServerRenderer::configure_scene(const SceneSettings& settings) {
    scene_settings_ = settings;
    if (!environment_.is_valid()) {
        environment_ = server_->environment_create();
        light_ = server_->directional_light_create();
        light_instance_ = server_->instance_create2(light_, scenario_);
        server_->scenario_set_environment(scenario_, environment_);
    }
    const godot::Color fog(settings.fog_color.x, settings.fog_color.y, settings.fog_color.z);
    server_->environment_set_background(environment_, sky_.is_valid()
                                                          ? godot::RenderingServer::ENV_BG_SKY
                                                          : godot::RenderingServer::ENV_BG_COLOR);
    server_->environment_set_bg_color(environment_, fog);
    server_->environment_set_ambient_light(
        environment_,
        godot::Color(settings.ambient_light, settings.ambient_light, settings.ambient_light),
        godot::RenderingServer::ENV_AMBIENT_SOURCE_COLOR);
    server_->environment_set_fog(environment_, true, fog, 1.0f, 0.0f, 0.0f, 0, 0, 0, 0,
                                 godot::RenderingServer::ENV_FOG_MODE_DEPTH);
    server_->environment_set_fog_depth(environment_, 1.0f, settings.far_distance * 0.7f,
                                       settings.far_distance);
    // Native depth fog is radial smoothstep; the original vertex-linear path is
    // supplied through Godot's FOG output instead. Keep sky/particle overrides.
    const auto update_material = [&](std::uint64_t id, godot::RID material) {
        server_->material_set_param(material, "original_fog_color", fog);
        server_->material_set_param(material, "original_fog_start", settings.far_distance * .7f);
        server_->material_set_param(material, "original_fog_end", settings.far_distance);
        if (chrome_materials_.contains(id)) {
            const auto direction = original_coordinates_
                ? original_to_godot(settings.directional_light) : settings.directional_light;
            server_->material_set_param(material, "chrome_light_direction",
                                       godot::Vector3(direction.x, direction.y, direction.z));
            server_->material_set_param(material, "chrome_ambient", settings.ambient_light);
            server_->material_set_param(material, "chrome_fog_color",
                godot::Vector3(settings.fog_color.x, settings.fog_color.y, settings.fog_color.z));
        }
    };
    for (const auto& [id, material] : materials_)
        update_material(id, material);
    for (const auto& [id, color] : instance_color_materials_)
        update_material(color.source, color.material);
    auto direction = original_coordinates_ ? original_to_godot(settings.directional_light)
                                           : settings.directional_light;
    godot::Transform3D light_transform;
    const godot::Vector3 backward(-direction.x, -direction.y, -direction.z);
    const auto right = godot::Vector3(0, 1, 0).cross(backward).normalized();
    light_transform.basis.set_column(0, right);
    light_transform.basis.set_column(1, backward.cross(right).normalized());
    light_transform.basis.set_column(2, backward.normalized());
    server_->instance_set_transform(light_instance_, light_transform);
    server_->light_set_color(light_, godot::Color(1, 1, 1));
    server_->light_set_param(light_, godot::RenderingServer::LIGHT_PARAM_ENERGY, 1.0f);
    if (sky_material_.is_valid()) {
        const auto scale = original_sky_transform({}, settings.far_distance);
        server_->material_set_param(
            sky_material_, "original_scale",
            godot::Vector3(scale.basis[0].x, scale.basis[1].y, scale.basis[2].z));
        server_->material_set_param(sky_material_, "camera_height", settings.camera_height);
    }
}
MeshHandle ServerRenderer::upload_surface(std::span<const Vec3> positions,
                                          std::span<const std::array<float, 2>> uv,
                                          std::span<const std::uint32_t> indices,
                                          MaterialHandle material) {
    return upload_surface(positions, uv, indices, material, {}, {});
}
MeshHandle ServerRenderer::upload_surface(std::span<const Vec3> positions,
                                          std::span<const std::array<float, 2>> uv,
                                          std::span<const std::uint32_t> indices,
                                          MaterialHandle material, std::span<const Vec3> normals,
                                          std::span<const std::array<float, 4>> colors) {
    const auto found = materials_.find(material.value);
    if (found == materials_.end() || positions.size() != uv.size() || positions.empty() ||
        indices.empty() || indices.size() % 3)
        return {};
    for (auto index : indices)
        if (index >= positions.size())
            return {};
    godot::PackedVector3Array vertices;
    godot::PackedVector2Array coordinates;
    godot::PackedVector3Array vertex_normals;
    godot::PackedColorArray vertex_colors;
    godot::PackedInt32Array triangles;
    godot::PackedFloat32Array chrome_normals;
    const bool chrome = chrome_materials_.contains(material.value);
    if (chrome && normals.size() != positions.size())
        return {};
    if (chrome)
        chrome_normals.resize(static_cast<std::int64_t>(normals.size() * 3));
    for (std::size_t i = 0; i < positions.size(); ++i) {
        const auto p = original_coordinates_ ? original_to_godot(positions[i]) : positions[i];
        vertices.push_back({p.x, p.y, p.z});
        if (i < normals.size()) {
            const auto n = original_coordinates_ ? original_to_godot(normals[i]) : normals[i];
            vertex_normals.push_back({n.x, n.y, n.z});
            if (chrome) {
                chrome_normals.set(i * 3, n.x);
                chrome_normals.set(i * 3 + 1, n.y);
                chrome_normals.set(i * 3 + 2, n.z);
            }
        }
        if (i < colors.size()) {
            const auto c = colors[i];
            vertex_colors.push_back({c[0], c[1], c[2], c[3]});
        }
        coordinates.push_back({uv[i][0], uv[i][1]});
    }
    for (auto index : indices)
        triangles.push_back(static_cast<std::int32_t>(index));
    godot::Array arrays;
    arrays.resize(godot::RenderingServer::ARRAY_MAX);
    arrays[godot::RenderingServer::ARRAY_VERTEX] = vertices;
    arrays[godot::RenderingServer::ARRAY_TEX_UV] = coordinates;
    if (!vertex_normals.is_empty())
        arrays[godot::RenderingServer::ARRAY_NORMAL] = vertex_normals;
    if (!vertex_colors.is_empty())
        arrays[godot::RenderingServer::ARRAY_COLOR] = vertex_colors;
    arrays[godot::RenderingServer::ARRAY_INDEX] = triangles;
    std::uint64_t format{};
    if (chrome) {
        arrays[godot::RenderingServer::ARRAY_CUSTOM0] = chrome_normals;
        format = static_cast<std::uint64_t>(godot::RenderingServer::ARRAY_CUSTOM_RGB_FLOAT)
                 << godot::RenderingServer::ARRAY_FORMAT_CUSTOM0_SHIFT;
    }
    trace_resource(FrameResource::mesh);
    const auto rid = server_->mesh_create();
    server_->mesh_add_surface_from_arrays(rid, godot::RenderingServer::PRIMITIVE_TRIANGLES, arrays,
                                          {}, {}, format);
    server_->mesh_surface_set_material(rid, 0, found->second);
    if (billboard_materials_.contains(material.value)) {
        float radius = 0, minimum_y = positions.front().y, maximum_y = minimum_y;
        for (const auto p : positions) {
            radius = std::max(radius, std::sqrt(p.x * p.x + p.z * p.z));
            minimum_y = std::min(minimum_y, p.y);
            maximum_y = std::max(maximum_y, p.y);
        }
        // The GPU rotates the mesh outside its serialized flat-plane bounds.
        server_->mesh_set_custom_aabb(
            rid, godot::AABB({-radius, minimum_y, -radius},
                             {radius * 2, std::max(maximum_y - minimum_y, .001f), radius * 2}));
    }
    const auto id = next_handle_++;
    meshes_.emplace(id, rid);
    mesh_materials_.emplace(id, material.value);
    return {id};
}
void ServerRenderer::clear_sky() {
    if (environment_.is_valid()) {
        server_->environment_set_sky(environment_, {});
        server_->environment_set_background(environment_, godot::RenderingServer::ENV_BG_COLOR);
    }
    for (auto* rid : {&sky_, &sky_material_, &sky_shader_}) {
        if (rid->is_valid())
            server_->free_rid(*rid);
        *rid = {};
    }
}
Result ServerRenderer::configure_sky(std::span<const Vec3> positions,
                                     std::span<const std::array<float, 2>> uv,
                                     std::span<const std::uint32_t> indices,
                                     MaterialHandle material) {
    const auto texture = material_textures_.find(material.value);
    if (!environment_.is_valid() || texture == material_textures_.end() ||
        positions.size() != uv.size() || indices.empty() || indices.size() % 3)
        return Result::invalid_argument;
    clear_sky();
    std::ostringstream code;
    code << std::setprecision(std::numeric_limits<float>::max_digits10);
    code << "shader_type sky; uniform sampler2D "
            "original_texture:source_color,filter_linear,repeat_enable; uniform vec3 "
            "original_scale; uniform float camera_height;";
    code << "vec3 hit_triangle(vec3 d,vec3 a,vec3 b,vec3 c){vec3 "
            "sc=original_scale;a=a*sc-vec3(0,camera_height,0);b=b*sc-vec3(0,camera_height,0);c=c*"
            "sc-vec3(0,camera_height,0);vec3 e=b-a;vec3 f=c-a;vec3 p=cross(d,f);float "
            "k=dot(e,p);if(abs(k)<.000001)return vec3(-1);vec3 t=-a;float u=dot(t,p)/k;vec3 "
            "q=cross(t,e);float v=dot(d,q)/k;float "
            "distance=dot(f,q)/k;if(u<0.0||v<0.0||u+v>1.0||distance<=0.0)return vec3(-1);return "
            "vec3(distance,u,v);}";
    code << "void sky(){vec3 d=vec3(EYEDIR.x,EYEDIR.y,-EYEDIR.z);float nearest=1e20;vec2 "
            "tc=vec2(0);vec3 h;";
    const auto vertex = [&](Vec3 p) { code << "vec3(" << p.x << "," << p.y << "," << p.z << ")"; };
    const auto coordinate = [&](std::array<float, 2> p) {
        code << "vec2(" << p[0] << "," << p[1] << ")";
    };
    for (std::size_t i = 0; i < indices.size(); i += 3) {
        const auto a = indices[i], b = indices[i + 1], c = indices[i + 2];
        if (a >= positions.size() || b >= positions.size() || c >= positions.size())
            return Result::invalid_argument;
        code << "h=hit_triangle(d,";
        vertex(positions[a]);
        code << ",";
        vertex(positions[b]);
        code << ",";
        vertex(positions[c]);
        code << ");if(h.x>0.0&&h.x<nearest){nearest=h.x;tc=";
        coordinate(uv[a]);
        code << "*(1.0-h.y-h.z)+";
        coordinate(uv[b]);
        code << "*h.y+";
        coordinate(uv[c]);
        code << "*h.z;}";
    }
    code << "vec2 "
            "texel=1.0/"
            "vec2(textureSize(original_texture,0));COLOR=texture(original_texture,vec2(tc.x,clamp("
            "tc.y,.5*texel.y,1.0-.5*texel.y))).rgb;}";
    sky_shader_ = server_->shader_create();
    sky_material_ = server_->material_create();
    sky_ = server_->sky_create();
    server_->shader_set_code(sky_shader_, godot::String(code.str().c_str()));
    server_->material_set_shader(sky_material_, sky_shader_);
    server_->material_set_param(sky_material_, "original_texture", texture->second);
    const auto scale = original_sky_transform({}, scene_settings_.far_distance);
    server_->material_set_param(
        sky_material_, "original_scale",
        godot::Vector3(scale.basis[0].x, scale.basis[1].y, scale.basis[2].z));
    server_->material_set_param(sky_material_, "camera_height", scene_settings_.camera_height);
    server_->sky_set_material(sky_, sky_material_);
    server_->sky_set_mode(sky_, godot::RenderingServer::SKY_MODE_REALTIME);
    server_->environment_set_sky(environment_, sky_);
    server_->environment_set_background(environment_, godot::RenderingServer::ENV_BG_SKY);
    return Result::ok;
}
void ServerRenderer::prewarm_particles(std::uint32_t capacity, MaterialHandle material, bool tire,
                                       bool lower_atlas) {
    if (!capacity)
        return;
    const auto key = std::tuple{capacity, material.value, tire, lower_atlas};
    if (particle_warmups_.contains(key))
        return;
    const auto handle = create_particles(capacity, material, tire, lower_atlas);
    if (!handle.value)
        throw std::runtime_error("Particle preparation failed");
    auto& system = particle_systems_.at(handle.value);
    server_->instance_set_visible(system.instance, false);
    server_->particles_set_emitting(system.particles, false);
    particle_warmups_.emplace(key, handle);
}
ParticleSystemHandle ServerRenderer::create_particles(std::uint32_t capacity,
                                                      MaterialHandle material, bool tire,
                                                      bool lower_atlas) {
    const auto texture = material_textures_.find(material.value);
    if (!capacity || capacity > 1024 || texture == material_textures_.end())
        return {};
    ParticleResources system;
    trace_resource(FrameResource::particles,
                   "particles:" + std::to_string(capacity) + (tire ? ":mix" : ":add"));
    system.capacity = capacity;
    system.particles = server_->particles_create();
    system.instance = server_->instance_create2(system.particles, scenario_);
    system.mesh = server_->mesh_create();
    auto [process_entry, new_process] = particle_process_shaders_.try_emplace(capacity);
    if (new_process)
        process_entry->second = server_->shader_create();
    system.process_shader = process_entry->second;
    system.process_material = server_->material_create();
    auto [draw_entry, new_draw] = particle_draw_shaders_.try_emplace(std::pair{capacity, tire});
    if (new_draw)
        draw_entry->second = server_->shader_create();
    system.draw_shader = draw_entry->second;
    system.draw_material = server_->material_create();
    const auto count = std::to_string(capacity);
    const std::string process =
        "shader_type particles; render_mode disable_velocity; uniform vec4 slot_position[" + count +
        "]; uniform vec4 slot_color[" + count + "]; uniform vec4 slot_data[" + count +
        "]; void start(){VELOCITY=vec3(0.0);} void process(){int "
        "i=int(INDEX);TRANSFORM=mat4(1.0);TRANSFORM[3]=vec4(slot_position[i].xyz,1.0);COLOR=slot_"
        "color[i];CUSTOM=slot_data[i];ACTIVE=true;}";
    if (new_process)
        server_->shader_set_code(system.process_shader, godot::String(process.c_str()));
    server_->material_set_shader(system.process_material, system.process_shader);
    auto draw = shaders::shader_variant(shaders::original_particle,
                                        std::string("#define RW_CUSTOM_GEOMETRY\n") +
                                            (tire ? "#define RW_TIRE\n" : ""));
    const auto vertex_entry = draw.find("void vertex()");
    draw.insert(vertex_entry, "uniform vec4 slot_triangle_ab[" + count +
                                  "]; uniform vec4 slot_triangle_c[" + count + "];\n");
    if (new_draw)
        server_->shader_set_code(system.draw_shader, godot::String(draw.c_str()));
    server_->material_set_shader(system.draw_material, system.draw_shader);
    server_->material_set_param(system.draw_material, "original_texture", texture->second);
    if (tire || lower_atlas)
        server_->material_set_param(system.draw_material, "atlas_v", godot::Vector2(.609f, 1.0f));
    godot::PackedVector3Array vertices;
    vertices.push_back({-.25f, -.75f, 0});
    vertices.push_back({-.25f, .25f, 0});
    vertices.push_back({.75f, .25f, 0});
    godot::PackedVector2Array uv;
    uv.push_back({0, 0});
    uv.push_back({1, 0});
    uv.push_back({0, 1});
    godot::Array arrays;
    arrays.resize(godot::RenderingServer::ARRAY_MAX);
    arrays[godot::RenderingServer::ARRAY_VERTEX] = vertices;
    arrays[godot::RenderingServer::ARRAY_TEX_UV] = uv;
    server_->mesh_add_surface_from_arrays(system.mesh, godot::RenderingServer::PRIMITIVE_TRIANGLES,
                                          arrays);
    server_->mesh_surface_set_material(system.mesh, 0, system.draw_material);
    server_->particles_set_amount(system.particles, static_cast<int>(capacity));
    server_->particles_set_lifetime(system.particles, 86400.0);
    server_->particles_set_explosiveness_ratio(system.particles, 1.0f);
    server_->particles_set_use_local_coordinates(system.particles, false);
    server_->particles_set_fixed_fps(system.particles, 0);
    server_->particles_set_draw_order(system.particles,
                                      godot::RenderingServer::PARTICLES_DRAW_ORDER_VIEW_DEPTH);
    server_->particles_set_process_material(system.particles, system.process_material);
    server_->particles_set_draw_passes(system.particles, 1);
    server_->particles_set_draw_pass_mesh(system.particles, 0, system.mesh);
    server_->particles_set_custom_aabb(
        system.particles, godot::AABB({-10000, -10000, -10000}, {20000, 20000, 20000}));
    server_->particles_set_emitting(system.particles, true);
    const auto id = next_handle_++;
    particle_systems_.emplace(id, system);
    std::vector<ParticleDrawState> empty(capacity);
    update_particles({id}, empty);
    return {id};
}
Result ServerRenderer::update_particles(ParticleSystemHandle handle,
                                        std::span<const ParticleDrawState> particles) {
    const auto found = particle_systems_.find(handle.value);
    if (found == particle_systems_.end() || particles.size() != found->second.capacity)
        return Result::invalid_argument;
    auto& system = found->second;
    godot::PackedVector4Array positions, colors, data, triangles_ab, triangles_c;
    godot::AABB bounds;
    bool has_bounds = false;
    for (const auto& p : particles) {
        const auto position = original_coordinates_ ? original_to_godot(p.position) : p.position;
        positions.push_back({position.x, position.y, position.z, 1});
        if (p.active) {
            float radius = p.size;
            if (p.custom_triangle)
                for (const auto vertex : p.triangle)
                    radius = std::max(radius, std::max(std::abs(vertex.x), std::abs(vertex.y)));
            const godot::Vector3 center(position.x, position.y, position.z),
                extent(radius, radius, radius);
            const godot::AABB particle_bounds(center - extent, extent * 2.0f);
            bounds = has_bounds ? bounds.merge(particle_bounds) : particle_bounds;
            has_bounds = true;
        }
        const auto color = p.color_argb;
        colors.push_back({((color >> 16) & 255) / 255.0f, ((color >> 8) & 255) / 255.0f,
                          (color & 255) / 255.0f,
                          p.active ? ((color >> 24) & 255) / 255.0f : 0.0f});
        data.push_back({p.active ? p.size : 0.0f, static_cast<float>(p.atlas_frame),
                        static_cast<float>(data.size()), p.custom_triangle ? 1.0f : 0.0f});
        triangles_ab.push_back(
            {p.triangle[0].x, p.triangle[0].y, p.triangle[1].x, p.triangle[1].y});
        triangles_c.push_back({p.triangle[2].x, p.triangle[2].y, 0, 0});
    }
    server_->particles_set_custom_aabb(
        system.particles, has_bounds ? bounds : godot::AABB({0, 0, 0}, {.001f, .001f, .001f}));
    server_->material_set_param(system.process_material, "slot_position", positions);
    server_->material_set_param(system.process_material, "slot_color", colors);
    server_->material_set_param(system.process_material, "slot_data", data);
    server_->material_set_param(system.draw_material, "slot_triangle_ab", triangles_ab);
    server_->material_set_param(system.draw_material, "slot_triangle_c", triangles_c);
    server_->particles_request_process(system.particles);
    return Result::ok;
}
void ServerRenderer::set_particles_visible(bool visible) {
    for (const auto& [id, system] : particle_systems_)
        server_->instance_set_visible(system.instance, visible);
}
void ServerRenderer::destroy_particles(ParticleSystemHandle handle) {
    const auto found = particle_systems_.find(handle.value);
    if (found == particle_systems_.end())
        return;
    const auto& p = found->second;
    for (auto rid : {p.instance, p.particles, p.mesh, p.process_material, p.draw_material})
        server_->free_rid(rid);
    particle_systems_.erase(found);
}
void ServerRenderer::destroy_material(MaterialHandle material) {
    material_shader_sources_.erase(material.value);
    const auto found = materials_.find(material.value);
    if (found != materials_.end()) {
        server_->free_rid(found->second);
        materials_.erase(found);
        material_textures_.erase(material.value);
        billboard_materials_.erase(material.value);
        chrome_materials_.erase(material.value);
    }
    const auto shader = material_shaders_.find(material.value);
    if (shader != material_shaders_.end()) {
        server_->free_rid(shader->second);
        material_shaders_.erase(shader);
    }
}
void ServerRenderer::destroy_texture(TextureHandle texture) {
    texture_sources_.erase(texture.value);
    if (const auto world = world_textures_.find(texture.value); world != world_textures_.end()) {
        server_->free_rid(world->second);
        world_textures_.erase(world);
    }
    const auto found = textures_.find(texture.value);
    if (found != textures_.end()) {
        server_->free_rid(found->second);
        textures_.erase(found);
    }
}
} // namespace roadwars
