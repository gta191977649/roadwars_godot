#include "game_src/unresolved/race_presentation.hpp"
#include "game_src/unresolved/bitmap.hpp"
#include "game_src/unresolved/contact_orientation.hpp"
#include "game_src/unresolved/weapon_mounts.hpp"
#include "game_src/unresolved/wheel_pose.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <stdexcept>

namespace roadwars {
namespace {
Vec3 add(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 multiply(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
} // namespace
RacePresentation::~RacePresentation() { stop(); }
Result RacePresentation::configure_frontend(ResourceStore& resources, std::string_view track,
                                            std::string_view character, std::string& error) {
    const std::array<std::string, 1> characters{std::string(character)};
    auto result = frontend_.load(resources, track, collision_.spawn().position, characters, error);
    if (result != Result::ok)
        return result;
    VehicleState initial;
    initial.position = collision_.spawn().position;
    return update_frontend(initial);
}
Result RacePresentation::update_frontend(const VehicleState& state) {
    const std::array<FrontendRacer, 1> racers{{{state.position, 0}}};
    FrontendFrame frame;
    frame.developer_build = developer_build_;
    frame.racers = racers;
    frame.speed = state.cached_speed;
    frame.rpm = state.rpm;
    frame.gear_index = static_cast<int>(state.gear_index);
    frame.reverse = state.reverse_engaged;
    return frontend_.update(frame);
}
Result RacePresentation::load(ResourceStore& resources, std::string_view track,
                              std::string_view vehicle, std::string& error) {
    stop();
    const auto world_path = "Tracks/" + std::string(track) + "/World";
    const auto vehicle_path = "Cars/" + std::string(vehicle) + "/Body";
    std::vector<std::byte> bytes;
    if (resources.read(world_path + "/SpatialObject.msh", bytes) != Result::ok) {
        error = "Missing world MSH";
        return Result::not_found;
    }
    if (decode_msh(bytes, world_, error) != Result::ok)
        return Result::invalid_argument;
    if (resources.read(vehicle_path + "/SpatialObject.msh", bytes) != Result::ok) {
        error = "Missing vehicle MSH";
        return Result::not_found;
    }
    if (decode_msh(bytes, vehicle_mesh_, error) != Result::ok)
        return Result::invalid_argument;
    initialize_stock_weapon_mounts(vehicle_mesh_);
    auto result = collision_.load(resources, track, world_, error);
    if (result != Result::ok)
        return result;
    if (load_track_render_settings(resources, track, render_settings_, error) != Result::ok ||
        view_distances_.load(resources, track, error) != Result::ok)
        return Result::invalid_argument;
    try {
        SceneSettings settings;
        settings.fog_color = render_settings_.fog_color;
        settings.directional_light = render_settings_.directional_light;
        settings.ambient_light = render_settings_.ambient_light;
        settings.near_distance = render_settings_.near_distance;
        settings.far_distance =
            view_distances_.maximum_distance() * render_settings_.view_distance_scalar;
        renderer_.configure_scene(settings);
        if (render_settings_.use_sky) {
            MeshObject sky;
            if (resources.read(render_settings_.sky_path, bytes) != Result::ok ||
                decode_msh(bytes, sky, error) != Result::ok)
                throw std::runtime_error("Original sky MSH unavailable: " + error);
            if (sky.parts.size() != 1 || !sky.children.empty())
                throw std::runtime_error("Original sky hierarchy requires additional recovery");
            const auto& part = sky.parts.front();
            std::vector<std::array<float, 2>> uv;
            for (auto p : part.uv)
                uv.push_back({p.x, p.y});
            const auto slash = render_settings_.sky_path.find_last_of('/');
            const auto handle =
                material(resources, render_settings_.sky_path.substr(0, slash), part, sky.flags);
            if (renderer_.configure_sky(part.positions, uv, part.indices, handle) != Result::ok)
                throw std::runtime_error("Godot native sky initialization failed");
        }
        upload(resources, world_, world_path, {}, false);
        upload(resources, vehicle_mesh_, vehicle_path, {}, true);
        const auto tires_path = "Cars/" + std::string(vehicle) + "/Tires/Street";
        if (resources.read(tires_path + "/SpatialObject.msh", bytes) != Result::ok)
            throw std::runtime_error("Missing Street tire MSH");
        MeshObject tire;
        if (decode_msh(bytes, tire, error) != Result::ok)
            throw std::runtime_error(error);
        if (load_stock_wheel_scale(resources, vehicle, wheel_scale_x_, error) != Result::ok)
            throw std::runtime_error(error);
        wheel_spins_ = {};
        tire.position = {}; // 00412834 -> 004d2ea1 clears the loaded child transform on attach.
        const auto attach = [&](const auto& self, const MeshObject& node, Vec3 parent) -> void {
            const auto position = add(parent, node.position);
            if (node.name == "Tire1" || node.name == "Tire2" || node.name == "Tire3" ||
                node.name == "Tire4") {
                const auto index = static_cast<std::size_t>(node.name.back() - '1');
                wheel_poses_[index] =
                    original_wheel_transform(index, position, 0, 0, wheel_scale_x_);
                upload(resources, tire, tires_path, {}, true, static_cast<int>(index));
            }
            for (const auto& child : node.children)
                self(self, child, position);
        };
        attach(attach, vehicle_mesh_, {});
        update_vehicle(collision_.spawn().position, collision_.spawn().heading_degrees);
    } catch (const std::exception& exception) {
        stop();
        error = exception.what();
        return Result::unavailable;
    }
    error.clear();
    return Result::ok;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004d2fcd
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004ea597
MaterialHandle RacePresentation::material(ResourceStore& resources, const std::string& base,
                                          const MeshPart& part, std::uint32_t flags) {
    auto key = ResourceStore::canonical(base + "/" + part.texture);
    for (const auto component : part.color)
        key += ":" + std::to_string(std::bit_cast<std::uint32_t>(component));
    key += ":flags=" + std::to_string(flags);
    const auto found = materials_.find(key);
    if (found != materials_.end())
        return found->second;
    std::vector<std::byte> bytes;
    Bitmap bitmap;
    TextureTransparency transparency{TextureTransparency::opaque};
    if (part.texture.empty()) {
        bitmap = {1, 1,     {std::byte{255}, std::byte{255}, std::byte{255}, std::byte{255}},
                  0, false, {}};
    } else {
        std::vector<std::string> candidates{
            base + "/" + part.texture, base + "/Textures/" + part.texture,
            "Textures/" + part.texture, "Cars/Shared/Textures/" + part.texture};
        const auto suffix = "/" + ResourceStore::canonical(part.texture);
        const auto context = ResourceStore::canonical(base.substr(0, base.find_last_of('/')) + "/");
        for (const auto& [path, entry] : resources.entries()) {
            if (path.ends_with(suffix) &&
                (path.starts_with(context) || path.starts_with("CARS/SHARED/")))
                candidates.push_back(path);
        }
        bool loaded = false;
        std::string resolved;
        for (const auto& path : candidates) {
            if (resources.read(path, bytes) == Result::ok) {
                loaded = true;
                resolved = path;
                break;
            }
        }
        if (!loaded)
            throw std::runtime_error("Texture missing: " + key);
        if (decode_bitmap(bytes, bitmap) != Result::ok)
            throw std::runtime_error("Unsupported BMP: " + key);
        Bitmap alpha;
        const auto companion = original_alpha_path(resolved);
        const Bitmap* alpha_pointer = nullptr;
        if (!companion.empty()) {
            if (resources.read(companion, bytes) != Result::ok ||
                decode_bitmap(bytes, alpha) != Result::ok)
                throw std::runtime_error("Original alpha companion unavailable: " + companion);
            alpha_pointer = &alpha;
        }
        if (prepare_original_texture(resolved, bitmap, transparency, alpha_pointer) != Result::ok)
            throw std::runtime_error("Original texture conversion unavailable: " + resolved);
    }
    const auto texture = renderer_.upload_texture(bitmap.width, bitmap.height, bitmap.rgba);
    if (!texture.value)
        throw std::runtime_error("Texture upload failed: " + key);
    textures_.push_back(texture);
    MaterialSettings settings;
    settings.color =
        part.texture.empty() ? part.color : std::array<float, 4>{1, 1, 1, part.color[3]};
    settings.unshaded = (flags & 4) != 0;
    settings.billboard_y = (flags & 0x40) != 0;
    settings.depth_write = (flags & 1) == 0;
    settings.depth_test = (flags & 0x10) == 0;
    settings.fog = (flags & 0x100) == 0;
    settings.opaque_alpha_only = (flags & 0x80) != 0;
    if (flags & 2)
        settings.alpha_mode = AlphaMode::additive;
    else if ((flags & 0x20) || transparency == TextureTransparency::alpha_blend ||
             settings.color[3] < 1)
        settings.alpha_mode = AlphaMode::blend;
    else
        settings.alpha_mode =
            transparency == TextureTransparency::color_key ? AlphaMode::cutout : AlphaMode::opaque;
    const auto handle = renderer_.create_material(texture, settings);
    if (!handle.value)
        throw std::runtime_error("Material upload failed: " + key);
    materials_.emplace(key, handle);
    return handle;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004d314d
void RacePresentation::upload(ResourceStore& resources, const MeshObject& object,
                              const std::string& base, Vec3 parent, bool vehicle, int wheel_index,
                              int parent_node) {
    // 004d314d returns before submitting children when visibility +0x5c is zero.
    if (!object.visible)
        return;
    const auto position = add(parent, object.position);
    int world_node = -1;
    if (!vehicle) {
        WorldNode node;
        node.local = object.position;
        node.parent = parent_node;
        const auto parsed = parse_track_animation(object.name, node.animation);
        if (parsed != Result::ok && parsed != Result::not_found)
            throw std::runtime_error("Invalid original RotObject name: " + object.name);
        node.animated = node.animation.speeds.x != 0 || node.animation.speeds.y != 0 ||
                        node.animation.speeds.z != 0 ||
                        (parent_node >= 0 && world_nodes_[parent_node].animated);
        node.pose.origin = position;
        world_node = static_cast<int>(world_nodes_.size());
        world_nodes_.push_back(node);
        if (node.animated)
            animated_world_nodes_.push_back(static_cast<std::size_t>(world_node));
    }
    for (const auto& part : object.parts) {
        if (part.indices.empty())
            continue;
        std::vector<std::array<float, 2>> uv;
        for (const auto& coordinate : part.uv)
            uv.push_back({coordinate.x, coordinate.y});
        const auto mesh = renderer_.upload_surface(part.positions, uv, part.indices,
                                                   material(resources, base, part, object.flags),
                                                   part.normals, part.colors);
        if (!mesh.value)
            throw std::runtime_error("Mesh upload failed: " + object.name);
        meshes_.push_back(mesh);
        const auto instance = renderer_.create_instance(mesh);
        if (!instance.value)
            throw std::runtime_error("Instance upload failed: " + object.name);
        renderer_.set_instance_static(instance, !vehicle && !world_nodes_[world_node].animated);
        if (vehicle)
            vehicle_instances_.push_back(instances_.size());
        else if (world_nodes_[world_node].animated)
            animated_instances_.push_back(instances_.size());
        instances_.push_back({instance, position, vehicle, object.flags, wheel_index, world_node});
        Transform transform;
        transform.origin = position;
        renderer_.update_instance(instance, transform);
    }
    for (const auto& child : object.children)
        upload(resources, child, base, position, vehicle, wheel_index, world_node);
}
void RacePresentation::update_track_animation(float delta) {
    for (const auto index : animated_world_nodes_) {
        auto& node = world_nodes_[index];
        advance_track_animation(node.animation, delta);
        node.world_angles = node.animation.angles;
        auto origin = node.local;
        if (node.parent >= 0) {
            const auto& parent = world_nodes_[node.parent];
            node.world_angles = add(parent.world_angles, node.world_angles);
            const auto& b = parent.pose.basis;
            origin = add(parent.pose.origin,
                         add(add(multiply(b[0], node.local.x), multiply(b[1], node.local.y)),
                             multiply(b[2], node.local.z)));
        }
        // 004d5790/004d57e0/004d5830 sum ancestor Euler angles before 004d3dbd.
        node.pose = original_vehicle_transform(origin, node.world_angles.x, node.world_angles.y,
                                               node.world_angles.z);
    }
    for (const auto index : animated_instances_) {
        const auto& instance = instances_[index];
        renderer_.update_instance(instance.handle, world_nodes_[instance.world_node].pose);
    }
}
void RacePresentation::update_vehicle(Vec3 position, float heading_degrees) {
    update_vehicle(original_vehicle_transform(position, heading_degrees, 0, 0));
}
void RacePresentation::update_vehicle(const Transform& pose) {
    const auto position = pose.origin;
    const auto rotate = [&](Vec3 v) {
        return add(add(multiply(pose.basis[0], v.x), multiply(pose.basis[1], v.y)),
                   multiply(pose.basis[2], v.z));
    };
    for (const auto index : vehicle_instances_) {
        const auto& instance = instances_[index];
        Transform transform = pose;
        transform.origin =
            instance.vehicle ? add(position, rotate(instance.local)) : instance.local;
        if (instance.wheel_index >= 0) {
            const auto& wheel = wheel_poses_[instance.wheel_index];
            const auto wheel_vector = [&](Vec3 p) {
                return add(add(multiply(wheel.basis[0], p.x), multiply(wheel.basis[1], p.y)),
                           multiply(wheel.basis[2], p.z));
            };
            transform.origin =
                add(position, rotate(add(wheel.origin, wheel_vector(instance.local))));
            for (int axis = 0; axis < 3; ++axis)
                transform.basis[axis] = rotate(wheel.basis[axis]);
        }
        renderer_.update_instance(instance.handle, transform);
    }
}
void RacePresentation::apply_camera(const CameraFrame& camera) {
    camera_ = camera;
    const auto eye = camera.transform.origin;
    configure_view_at(eye);
    renderer_.update_camera(camera.transform, camera.vertical_fov_degrees);
}
void RacePresentation::configure_view_at(Vec3 eye) {
    SceneSettings settings;
    settings.fog_color = render_settings_.fog_color;
    settings.directional_light = render_settings_.directional_light;
    settings.ambient_light = render_settings_.ambient_light;
    settings.near_distance = render_settings_.near_distance;
    settings.far_distance = view_distance_at(eye);
    camera_far_distance_ = settings.far_distance;
    settings.camera_height = eye.y;
    renderer_.configure_scene(settings);
}

void RacePresentation::update_wheels(const VehicleDynamics& vehicle, float dt, float brake,
                                     bool reset) {
    const auto& state = vehicle.state();
    const auto& parameters = vehicle.parameters();
    if (reset)
        wheel_spins_ = {};
    const auto forward = vehicle.transform().basis[2];
    const auto velocity = state.velocity;
    const float magnitude =
        std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z);
    float angle = 0;
    if (magnitude > 0) {
        const float dot =
            (forward.x * velocity.x + forward.y * velocity.y + forward.z * velocity.z) / magnitude;
        angle = std::acos(std::clamp(dot, -1.0f, 1.0f)) * 57.288352966308594f;
    }
    if (advance_wheel_spin(wheel_spins_, state.cached_speed, angle,
                           parameters.rear_wheel_radius * 6.2831854820251465f, brake,
                           state.air_time, dt) != Result::ok)
        throw std::runtime_error("Original wheel spin input invalid");
    for (std::size_t i = 0; i < 4; ++i) {
        auto anchor = parameters.wheel_axle_anchors[i];
        anchor.y += vehicle.local_wheel_heights()[i] - parameters.wheel_ground_anchors[i].y;
        wheel_poses_[i] = original_wheel_transform(i, anchor, state.steering_degrees,
                                                   wheel_spins_[i], wheel_scale_x_);
    }
}
Result RacePresentation::configure_particles(ResourceStore& resources, std::uint32_t capacity) {
    if (!capacity)
        return Result::ok;
    MeshPart particle;
    particle.texture = "16ParticleFX.bmp";
    particles_ =
        renderer_.create_particles(capacity, material(resources, "Textures", particle, 0x127));
    return particles_.value ? Result::ok : Result::unavailable;
}
Result RacePresentation::update_particles(std::span<const ParticleRenderState> particles) {
    if (!particles_.value)
        return particles.empty() ? Result::ok : Result::unavailable;
    std::vector<ParticleDrawState> states;
    for (const auto& p : particles)
        states.push_back({p.position, p.size, p.color_argb, p.atlas_frame, p.active});
    return renderer_.update_particles(particles_, states);
}
Result RacePresentation::configure_tire_particles(ResourceStore& resources,
                                                  std::uint32_t capacity) {
    if (!capacity)
        return Result::ok;
    MeshPart particle;
    particle.texture = "xxParticleFX.bmp";
    tire_particles_ = renderer_.create_particles(
        capacity, material(resources, "Textures", particle, 0x125), true);
    return tire_particles_.value ? Result::ok : Result::unavailable;
}
Result RacePresentation::update_tire_particles(std::span<const ParticleRenderState> particles) {
    if (!tire_particles_.value)
        return particles.empty() ? Result::ok : Result::unavailable;
    std::vector<ParticleDrawState> states;
    for (const auto& p : particles)
        states.push_back({p.position, p.size, p.color_argb, p.atlas_frame, p.active});
    return renderer_.update_particles(tire_particles_, states);
}
void RacePresentation::set_skid_marks_visible(bool visible) {
    for (const auto& cached : skid_meshes_)
        if (cached.instance.value)
            renderer_.set_instance_visible(cached.instance, visible);
}
Result RacePresentation::configure_skid_marks(ResourceStore& resources) {
    MeshPart strip;
    strip.texture = "xxSkidMark.bmp";
    skid_material_ = material(resources, "Textures", strip, 0x125);
    return skid_material_.value ? Result::ok : Result::unavailable;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004e8bda
// Original triangle-strip geometry is submitted through the host renderer.
Result RacePresentation::update_skid_marks(const std::array<SkidMarkEmitter, 4>& emitters) {
    if (!skid_material_.value)
        return Result::unavailable;
    for (std::size_t tire = 0; tire < emitters.size(); ++tire) {
        for (std::size_t index = 0; index < 5; ++index) {
            const auto& strip = emitters[tire].strips()[index];
            auto& cached = skid_meshes_[tire * 5 + index];
            if (cached.revision == strip.revision)
                continue;
            if (cached.instance.value)
                renderer_.destroy_instance(cached.instance);
            if (cached.mesh.value)
                renderer_.destroy_mesh(cached.mesh);
            cached = {};
            cached.revision = strip.revision;
            if (strip.vertices.size() < 4)
                continue;
            std::vector<Vec3> positions;
            std::vector<std::array<float, 2>> uv;
            std::vector<std::array<float, 4>> colors;
            std::vector<std::uint32_t> indices;
            for (const auto& vertex : strip.vertices) {
                positions.push_back(vertex.position);
                uv.push_back(vertex.uv);
                const auto c = vertex.color_argb;
                colors.push_back({((c >> 16) & 255) / 255.0f, ((c >> 8) & 255) / 255.0f,
                                  (c & 255) / 255.0f, (c >> 24) / 255.0f});
            }
            for (std::uint32_t i = 2; i < positions.size(); ++i) {
                if (i % 2)
                    indices.insert(indices.end(), {i - 1, i - 2, i});
                else
                    indices.insert(indices.end(), {i - 2, i - 1, i});
            }
            cached.mesh =
                renderer_.upload_surface(positions, uv, indices, skid_material_, {}, colors);
            if (!cached.mesh.value)
                return Result::unavailable;
            cached.instance = renderer_.create_instance(cached.mesh);
            if (!cached.instance.value)
                return Result::unavailable;
        }
    }
    return Result::ok;
}
void RacePresentation::stop() {
    frontend_.stop();
    for (auto& cached : skid_meshes_) {
        if (cached.instance.value)
            renderer_.destroy_instance(cached.instance);
        if (cached.mesh.value)
            renderer_.destroy_mesh(cached.mesh);
        cached = {};
    }
    skid_material_ = {};
    if (tire_particles_.value)
        renderer_.destroy_particles(tire_particles_);
    tire_particles_ = {};
    if (particles_.value)
        renderer_.destroy_particles(particles_);
    particles_ = {};
    for (const auto& instance : instances_)
        renderer_.destroy_instance(instance.handle);
    for (auto mesh : meshes_)
        renderer_.destroy_mesh(mesh);
    for (const auto& [key, material] : materials_)
        renderer_.destroy_material(material);
    for (auto texture : textures_)
        renderer_.destroy_texture(texture);
    instances_.clear();
    vehicle_instances_.clear();
    animated_instances_.clear();
    animated_world_nodes_.clear();
    world_nodes_.clear();
    meshes_.clear();
    materials_.clear();
    textures_.clear();
}
} // namespace roadwars
