#include "game_src/unresolved/race_presentation.hpp"
#include "game_src/unresolved/bitmap.hpp"
#include "game_src/unresolved/chrome_mapping.hpp"
#include "game_src/unresolved/contact_orientation.hpp"
#include "game_src/unresolved/race_session.hpp"
#include "game_src/unresolved/vehicle_combat.hpp"
#include "game_src/unresolved/vehicle_damage_visuals.hpp"
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
MaterialSettings surface_settings(const MeshPart& part, std::uint32_t flags,
                                  TextureTransparency transparency) {
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
    return settings;
}
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
                              std::string_view vehicle, std::string& error,
                              const TireSelection& tires) {
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
        if (load_chrome_mapping(resources, renderer_, chrome_texture_, chrome_enabled_, error) !=
            Result::ok)
            throw std::runtime_error(error);
        if (chrome_texture_.value)
            textures_.push_back(chrome_texture_);
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
        if (load_vehicle_visual(resources, vehicle, error, tires) != Result::ok)
            throw std::runtime_error(error);
        update_vehicle(collision_.spawn().position, collision_.spawn().heading_degrees);
    } catch (const std::exception& exception) {
        stop();
        error = exception.what();
        return Result::unavailable;
    }
    error.clear();
    return Result::ok;
}

Result RacePresentation::load_vehicle_visual(ResourceStore& resources, std::string_view vehicle,
                                             std::string& error, const TireSelection& tires,
                                             int paint_job) {
    if (paint_job < 0 || paint_job > 6)
        return Result::invalid_argument;
    const auto vehicle_path = "Cars/" + std::string(vehicle) + "/Body";
    std::vector<std::byte> bytes;
    vehicle_mesh_.position = {}; // VehicleState replaces the serialized chassis root pose.
    const auto body_textures = paint_job > 0 ? "Cars/" + std::string(vehicle) +
                                                   "/Paint/Paint Job " + std::to_string(paint_job)
                                             : vehicle_path;
    vehicle_hierarchy_ = std::make_shared<MeshObject>(vehicle_mesh_);
    vehicle_angles_.reset();
    body_angles_ = {};
    upload(resources, vehicle_mesh_, body_textures, {}, true);
    const auto tires_path = "Cars/" + std::string(vehicle) + "/Tires/" + tires.type;
    if (resources.read(tires_path + "/SpatialObject.msh", bytes) != Result::ok)
        throw std::runtime_error("Missing selected tire MSH: " + tires_path);
    MeshObject tire;
    if (decode_msh(bytes, tire, error) != Result::ok)
        throw std::runtime_error(error);
    if (load_wheel_scale(resources, vehicle, tires, wheel_scale_x_, error) != Result::ok)
        throw std::runtime_error(error);
    wheel_spins_ = {};
    tire.position = {}; // 00412834 -> 004d2ea1 clears the loaded child transform on attach.
    const auto attach = [&](const auto& self, const MeshObject& node, Vec3 parent) -> void {
        const auto position = add(parent, node.position);
        if (node.name == "Tire1" || node.name == "Tire2" || node.name == "Tire3" ||
            node.name == "Tire4") {
            const auto index = static_cast<std::size_t>(node.name.back() - '1');
            wheel_poses_[index] = original_wheel_transform(index, position, 0, 0, wheel_scale_x_);
            upload(resources, tire, tires_path, {}, true, static_cast<int>(index));
        }
        for (const auto& child : node.children)
            self(self, child, position);
    };
    attach(attach, vehicle_mesh_, {});
    return Result::ok;
}
void RacePresentation::swap_visual(VehicleVisual& value) {
    vehicle_instances_.swap(value.instances);
    brake_lights_.swap(value.lights);
    std::swap(brake_lights_visible_, value.lights_visible);
    wheel_spins_.swap(value.spins);
    wheel_poses_.swap(value.wheels);
    std::swap(wheel_scale_x_, value.wheel_scale);
    vehicle_hierarchy_.swap(value.hierarchy);
    vehicle_angles_.swap(value.vehicle_angles);
    std::swap(body_angles_, value.body_angles);
}
Result RacePresentation::add_opponent(ResourceStore& resources, std::string_view vehicle,
                                      std::string& error, const TireSelection& tires,
                                      int paint_job) {
    VehicleVisual player;
    swap_visual(player);
    auto player_mesh = std::move(vehicle_mesh_);
    Result result = Result::ok;
    try {
        std::vector<std::byte> bytes;
        const auto path = "Cars/" + std::string(vehicle) + "/Body/SpatialObject.msh";
        if (resources.read(path, bytes) != Result::ok ||
            decode_msh(bytes, vehicle_mesh_, error) != Result::ok)
            throw std::runtime_error("Opponent mesh unavailable: " + path);
        initialize_stock_weapon_mounts(vehicle_mesh_);
        load_vehicle_visual(resources, vehicle, error, tires, paint_job);
        opponents_.emplace_back();
        swap_visual(opponents_.back());
    } catch (const std::exception& failure) {
        error = failure.what();
        result = Result::unavailable;
    }
    vehicle_mesh_ = std::move(player_mesh);
    swap_visual(player);
    return result;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00411ecc
Result RacePresentation::replace_player_visual(ResourceStore& resources, std::string_view vehicle,
                                               const RaceLoadout& loadout, std::string& error) {
    if (loadout.paint_job < 0 || loadout.paint_job > 6)
        return Result::invalid_argument;
    damage_visuals_.clear();
    for (const auto index : vehicle_instances_) {
        renderer_.destroy_instance(instances_[index].handle);
        instances_[index].handle = {};
    }
    vehicle_instances_.clear();
    brake_lights_.clear();
    try {
        return load_vehicle_visual(resources, vehicle, error, loadout.tires, loadout.paint_job);
    } catch (const std::exception& failure) {
        error = failure.what();
        return Result::unavailable;
    }
}
void RacePresentation::update_opponent(std::size_t index, const VehicleDynamics& vehicle,
                                       float delta, bool reset) {
    auto& visual = opponents_.at(index);
    swap_visual(visual);
    update_wheels(vehicle, delta, vehicle.state().brake_amount, reset);
    update_vehicle(vehicle.transform());
    swap_visual(visual);
}
Result RacePresentation::configure_racers(ResourceStore& resources, std::string_view track,
                                          std::span<const std::string> characters,
                                          std::string& error) {
    return frontend_.load(resources, track, collision_.spawn().position, characters, error);
}

// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004d2fcd
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004ea597
MaterialHandle RacePresentation::material(ResourceStore& resources, const std::string& base,
                                           const MeshPart& part, std::uint32_t flags, bool chrome) {
    auto key = ResourceStore::canonical(base + "/" + part.texture);
    const auto shared_paint = damage_paint_textures_.find(key);
    if (shared_paint != damage_paint_textures_.end())
        key += ":mutable=" + std::to_string(shared_paint->second.handle.value);
    for (const auto component : part.color)
        key += ":" + std::to_string(std::bit_cast<std::uint32_t>(component));
    key += ":flags=" + std::to_string(flags);
    key += ":chrome=" + std::to_string(chrome ? chrome_texture_.value : 0);
    const auto found = materials_.find(key);
    if (found != materials_.end())
        return found->second;
    if (shared_paint != damage_paint_textures_.end()) {
        auto settings = surface_settings(part, flags, shared_paint->second.transparency);
        if (chrome)
            settings.chrome_map = chrome_texture_;
        const auto handle = renderer_.create_material(shared_paint->second.handle, settings);
        if (!handle.value)
            throw std::runtime_error("Mutable original paint material upload failed: " + key);
        materials_.emplace(key, handle);
        return handle;
    }
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
        const auto paint = base.find("/Paint/");
        if (paint != std::string::npos)
            candidates.insert(candidates.begin() + 1,
                              base.substr(0, paint) + "/Body/" + part.texture);
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
    auto settings = surface_settings(part, flags, transparency);
    if (chrome)
        settings.chrome_map = chrome_texture_;
    const auto handle = renderer_.create_material(texture, settings);
    if (!handle.value)
        throw std::runtime_error("Material upload failed: " + key);
    materials_.emplace(key, handle);
    return handle;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004d314d
void RacePresentation::upload(ResourceStore& resources, const MeshObject& object,
                              const std::string& base, Vec3 parent, bool vehicle, int wheel_index,
                              int parent_node, std::vector<std::size_t> node_path) {
    // 004d314d returns before submitting children when visibility +0x5c is zero.
    if (!object.visible)
        return;
    const auto position = add(parent, object.position);
    int world_node = -1;
    if (!vehicle) {
        WorldNode node;
        node.local = object.position;
        node.parent = parent_node;
        node.path = node_path;
        TurretParameters turret;
        node.turret = parse_turret_parameters(object.name, turret) == Result::ok;
        const auto parsed = parse_track_animation(object.name, node.animation);
        if (parsed != Result::ok && parsed != Result::not_found)
            throw std::runtime_error("Invalid original RotObject name: " + object.name);
        node.animated = node.turret || node.animation.speeds.x != 0 ||
                        node.animation.speeds.y != 0 || node.animation.speeds.z != 0 ||
                        (parent_node >= 0 && world_nodes_[parent_node].animated);
        node.pose.origin = position;
        world_node = static_cast<int>(world_nodes_.size());
        world_nodes_.push_back(node);
        if (node.animated)
            animated_world_nodes_.push_back(static_cast<std::size_t>(world_node));
    }
    for (std::size_t part_index = 0; part_index < object.parts.size(); ++part_index) {
        const auto& part = object.parts[part_index];
        if (part.indices.empty())
            continue;
        std::vector<std::array<float, 2>> uv;
        for (const auto& coordinate : part.uv)
            uv.push_back({coordinate.x, coordinate.y});
        const auto mesh = renderer_.upload_surface(part.positions, uv, part.indices,
                                                   material(resources, base, part, object.flags,
                                                       chrome_mapping_applies(chrome_enabled_,
                                                           object.flags, part_index,
                                                           vehicle && chrome_texture_.value)),
                                                   part.normals, part.colors);
        if (!mesh.value)
            throw std::runtime_error("Mesh upload failed: " + object.name);
        meshes_.push_back(mesh);
        const auto instance = renderer_.create_instance(mesh);
        if (!instance.value)
            throw std::runtime_error("Instance upload failed: " + object.name);
        // 0040ee2c resolves this exact body node; texture names are not lamp identities.
        if (vehicle && wheel_index < 0 && object.name == "295ULTailL") {
            brake_lights_.push_back(instance);
            renderer_.set_instance_visible(instance, false);
        }
        renderer_.set_instance_static(instance, !vehicle && !world_nodes_[world_node].animated);
        if (vehicle)
            vehicle_instances_.push_back(instances_.size());
        else if (world_nodes_[world_node].animated)
            animated_instances_.push_back(instances_.size());
        instances_.push_back({instance, position, vehicle, object.flags, wheel_index, world_node,
                              mesh, node_path, part_index, object.name});
        Transform transform;
        transform.origin = position;
        renderer_.update_instance(instance, transform);
    }
    for (std::size_t i = 0; i < object.children.size(); ++i) {
        auto child_path = node_path;
        child_path.push_back(i);
        upload(resources, object.children[i], base, position, vehicle, wheel_index, world_node,
               std::move(child_path));
    }
}
void RacePresentation::update_track_animation(float delta) {
    for (const auto index : animated_world_nodes_) {
        auto& node = world_nodes_[index];
        advance_track_animation(node.animation, delta);
        node.world_angles = node.turret ? node.turret_angles : node.animation.angles;
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
Result RacePresentation::update_turrets(std::span<const RaceTurretPose> poses) {
    for (const auto& pose : poses) {
        const auto found =
            std::find_if(world_nodes_.begin(), world_nodes_.end(), [&](const WorldNode& node) {
                return node.turret && node.path == pose.node_path;
            });
        if (found == world_nodes_.end())
            return Result::not_found;
        found->turret_angles = {pose.heading, pose.pitch, 0};
    }
    update_track_animation(0);
    return Result::ok;
}
Result RacePresentation::add_combat_visual(ResourceStore& resources, const MeshObject& mesh,
                                           std::string_view base, std::size_t& id,
                                           std::string& error) {
    VehicleVisual saved, visual;
    swap_visual(saved);
    try {
        upload(resources, mesh, std::string(base), {}, true);
    } catch (const std::exception& exception) {
        swap_visual(visual);
        swap_visual(saved);
        error = exception.what();
        return Result::unavailable;
    }
    swap_visual(visual);
    swap_visual(saved);
    for (const auto index : visual.instances)
        renderer_.set_instance_visible(instances_[index].handle, false);
    id = combat_visuals_.size();
    combat_visuals_.push_back(std::move(visual));
    return Result::ok;
}
Result RacePresentation::update_combat_visual(std::size_t id, const Transform& pose, bool visible) {
    if (id >= combat_visuals_.size())
        return Result::invalid_argument;
    auto& visual = combat_visuals_[id];
    swap_visual(visual);
    update_vehicle(pose);
    for (const auto index : vehicle_instances_)
        renderer_.set_instance_visible(instances_[index].handle, visible);
    swap_visual(visual);
    return Result::ok;
}
// Modern loading-time preparation, independent of the recovered effect timeline.
Result RacePresentation::prepare_combat(ResourceStore& resources, const RaceSession& session,
                                        std::string& error) {
    const auto prepare = [&](std::size_t capacity, std::string_view texture, bool mix,
                             bool lower_atlas = false) {
        if (!capacity)
            return;
        const auto separator = texture.find_last_of('/');
        MeshPart part;
        part.texture = texture.substr(separator + 1);
        renderer_.prewarm_particles(static_cast<std::uint32_t>(capacity),
                                    material(resources, std::string(texture.substr(0, separator)),
                                             part, mix ? 0x125 : 0x127),
                                    mix, lower_atlas);
    };
    // These bounded pools already exist in the session before gameplay. Prepare
    // their actual capacities, rather than assuming a particular effects scalar.
    for (const auto& particles : session.vehicle_particles()) {
        prepare(particles.sparks.particles().size(), "Textures/16ParticleFX.bmp", false);
        for (const auto& tire : particles.tires)
            prepare(tire.particles().size(), "Textures/xxParticleFX.bmp", true, true);
        prepare(particles.damage.particles().size(), "Textures/16ParticleFX.bmp", false);
        prepare(particles.damage.particles().size(), "Textures/xxParticleFX.bmp", true);
    }
    for (const auto& chimney : session.chimneys())
        prepare(chimney.pool.particles().size(), chimney.pool.texture_path(), true, true);
    for (const auto& pool : session.nitrous_trails())
        prepare(pool.particles().size(), pool.texture_path(), pool.uses_tire_atlas());
    for (const auto& pools : session.rear_blast_pools())
        for (const auto& blast : pools) {
            prepare(blast.primary_particles().size(), "Textures/16ParticleFX.bmp", false, true);
            prepare(blast.secondary_particles().size(), "Textures/xxParticleFX.bmp", true, true);
        }
    const auto weapon = [&](std::string_view name, float missile_count) -> Result {
        WeaponImpactPool impact;
        auto result = impact.configure(name, session.special_effects_scalar(), missile_count);
        if (result == Result::ok) {
            prepare(impact.particles().size(), impact.texture_path(), false);
            if (impact.requires_secondary_sprite())
                prepare(1, VulcanImpactSprite{}.texture_path(), false);
        } else if (result != Result::unavailable)
            return result;
        WeaponTrailPool trail;
        result = trail.configure(name, session.special_effects_scalar());
        if (result == Result::ok)
            prepare(trail.particles().size(), trail.texture_path(), trail.uses_tire_atlas());
        else if (result != Result::unavailable)
            return result;
        return Result::ok;
    };
    for (std::size_t owner = 0; owner < session.racers().size(); ++owner) {
        const auto& racer = session.racers()[owner];
        const auto* combat = racer.vehicle->combat();
        if (!combat)
            continue;
        const auto missiles = combat->guided_missiles().size();
        for (const auto& asset : combat->assets()) {
            if (!asset.descriptor)
                continue;
            if (weapon(asset.descriptor->name, missiles ? static_cast<float>(missiles) : 1) !=
                Result::ok) {
                error = "Combat particle preparation failed";
                return Result::unavailable;
            }
            const auto base = "Cars/Shared/Weapons/" +
                              std::string(weapon_slot_name(asset.descriptor->slot)) + "/" +
                              std::string(asset.descriptor->name);
            const auto visual = [&](VehicleCombatVisualKind kind, std::size_t index,
                                    const MeshObject& mesh, const std::string& path) {
                const std::array<std::size_t, 4> key{
                    owner + 1, static_cast<std::size_t>(asset.descriptor->slot),
                    static_cast<std::size_t>(kind), index};
                if (combat_visual_ids_.contains(key))
                    return Result::ok;
                std::size_t id{};
                const auto result = add_combat_visual(resources, mesh, path, id, error);
                if (result == Result::ok)
                    combat_visual_ids_.emplace(key, id);
                return result;
            };
            if (asset.has_weapon_mesh &&
                visual(VehicleCombatVisualKind::mounted, 0, asset.weapon, base) != Result::ok)
                return Result::unavailable;
            for (std::size_t m = 0; m < asset.muzzle_meshes.size(); ++m)
                if (visual(VehicleCombatVisualKind::muzzle_flash, m, asset.muzzle_meshes[m],
                           base) != Result::ok)
                    return Result::unavailable;
            const auto capacity = combat->projectile_capacity(asset.descriptor->slot);
            for (std::size_t m = 0; m < capacity; ++m) {
                const auto kind = asset.descriptor->slot == WeaponSlot::rear
                                      ? VehicleCombatVisualKind::rear_instance
                                      : VehicleCombatVisualKind::projectile;
                if (asset.has_ammunition_mesh &&
                    visual(kind, m, asset.ammunition, base) != Result::ok)
                    return Result::unavailable;
                if (asset.has_flare_mesh && visual(VehicleCombatVisualKind::missile_flare, m,
                                                   asset.flare_mesh, base) != Result::ok)
                    return Result::unavailable;
                if (asset.has_leg_meshes)
                    for (std::size_t leg = 0; leg < 2; ++leg)
                        if (visual(VehicleCombatVisualKind::rear_leg, m * 2 + leg,
                                   asset.leg_meshes[leg], base) != Result::ok)
                            return Result::unavailable;
            }
            if (asset.has_mount_housing)
                for (std::size_t m = 0; m < asset.mount_housing_meshes.size(); ++m)
                    if (visual(VehicleCombatVisualKind::mount_housing, m,
                               asset.mount_housing_meshes[m],
                               "Cars/" + racer.character + "/Paint/Paint Job " +
                                   std::to_string(racer.loadout.paint_job)) != Result::ok)
                        return Result::unavailable;
        }
    }
    for (const auto& asset : session.scene_combat().assets())
        if (weapon(asset.name, 1) != Result::ok) {
            error = "Scene particle preparation failed";
            return Result::unavailable;
        }
    return Result::ok;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004f878f
Result RacePresentation::update_combat(ResourceStore& resources, const RaceSession& session,
                                       std::string& error) {
    std::size_t undefined_uv_slots{};
    for (const auto& [key, visual] : combat_particle_ids_) {
        const std::vector<ParticleDrawState> hidden(visual.capacity);
        if (renderer_.update_particles(visual.handle, hidden) != Result::ok)
            return Result::unavailable;
    }
    const auto submit_particles = [&](std::array<std::size_t, 5> key,
                                      std::span<const ParticleRenderState> particles,
                                      std::string_view texture, bool alpha_mix, bool lower_atlas,
                                      std::span<const std::uint32_t> order = {}) {
        if (particles.empty())
            return Result::ok;
        auto found = combat_particle_ids_.find(key);
        if (found == combat_particle_ids_.end()) {
            // Allocate known bounded pools during the initial loading submission,
            // even when all slots are inactive. No emitter is advanced here.
            const auto separator = texture.find_last_of('/');
            MeshPart part;
            part.texture = texture.substr(separator + 1);
            const auto handle = renderer_.create_particles(
                static_cast<std::uint32_t>(particles.size()),
                material(resources, std::string(texture.substr(0, separator)), part,
                         alpha_mix ? 0x125 : 0x127),
                alpha_mix, lower_atlas);
            if (!handle.value)
                return Result::unavailable;
            found =
                combat_particle_ids_.emplace(key, CombatParticleVisual{handle, particles.size()})
                    .first;
        }
        std::vector<ParticleDrawState> states;
        const auto append = [&](const ParticleRenderState& particle) {
            states.push_back({particle.position, particle.size, particle.color_argb,
                              particle.atlas_frame, particle.active});
        };
        if (order.empty()) {
            for (const auto& particle : particles)
                append(particle);
        } else {
            for (const auto index : order) {
                if (index >= particles.size())
                    return Result::invalid_argument;
                append(particles[index]);
            }
            states.resize(particles.size());
        }
        return renderer_.update_particles(found->second.handle, states);
    };
    for (std::size_t i = 0; i < session.chimneys().size(); ++i) {
        const auto& pool = session.chimneys()[i].pool;
        const std::array<std::size_t, 5> key{9, i, 0, 0, 0};
        if (submit_particles(key, pool.particles(), pool.texture_path(), true, true) != Result::ok)
            return Result::unavailable;
        const auto found = combat_particle_ids_.find(key);
        if (found == combat_particle_ids_.end())
            continue;
        std::vector<ParticleDrawState> states(pool.particles().size());
        std::size_t draw_index{};
        for (const auto n : pool.draw_order()) {
            if (n >= states.size() || n >= pool.triangles().size())
                return Result::invalid_argument;
            const auto& particle = pool.particles()[n];
            auto& state = states[draw_index++];
            state = {particle.position, 1, particle.color_argb, particle.atlas_frame,
                     particle.active};
            state.triangle = pool.triangles()[n];
            state.custom_triangle = true;
        }
        if (renderer_.update_particles(found->second.handle, states) != Result::ok)
            return Result::unavailable;
    }
    for (std::size_t i = 0; i < session.impact_pools().size(); ++i) {
        const auto& impact = session.impact_pools()[i];
        if (submit_particles({4, i, 0, 0, 0}, impact.pool.particles(), impact.pool.texture_path(),
                             false, false, impact.pool.draw_order()) != Result::ok ||
            submit_particles({4, i, 1, 0, 0}, {&impact.sprite.particle(), 1},
                             impact.sprite.texture_path(), false, false) != Result::ok)
            return Result::unavailable;
    }
    for (std::size_t i = 0; i < session.rear_blast_pools().size(); ++i)
        for (std::size_t n = 0; n < 5; ++n) {
            const auto& blast = session.rear_blast_pools()[i][n];
            if (submit_particles({5, i, n, 0, 0}, blast.primary_particles(),
                                 "Textures/16ParticleFX.bmp", false, true,
                                 blast.primary_order()) != Result::ok ||
                submit_particles({5, i, n, 1, 0}, blast.secondary_particles(),
                                 "Textures/xxParticleFX.bmp", true, true,
                                 blast.secondary_order()) != Result::ok)
                return Result::unavailable;
        }
    for (std::size_t i = 0; i < session.vehicle_particles().size(); ++i) {
        const auto& particles = session.vehicle_particles()[i];
        if (submit_particles({6, i, 0, 0, 0}, particles.sparks.particles(),
                             "Textures/16ParticleFX.bmp", false, false,
                             particles.sparks.draw_order()) != Result::ok)
            return Result::unavailable;
        for (std::size_t n = 0; n < particles.tires.size(); ++n)
            if (submit_particles({6, i, n + 1, 0, 0}, particles.tires[n].particles(),
                                 "Textures/xxParticleFX.bmp", true, true,
                                 particles.tires[n].draw_order()) != Result::ok)
                return Result::unavailable;
        const auto& damage = particles.damage;
        for (std::size_t material_kind = 0; material_kind < 2; ++material_kind) {
            const std::array<std::size_t, 5> key{8, i, material_kind, 0, 0};
            std::vector<ParticleDrawState> states;
            bool visible{};
            for (const auto n : damage.draw_order()) {
                if (n >= damage.particles().size())
                    return Result::invalid_argument;
                const auto& particle = damage.particles()[n];
                const bool matching = damage.fire_material(n) == (material_kind == 0);
                ParticleDrawState state{particle.position, particle.size, particle.color_argb,
                                        particle.atlas_frame, particle.active && matching};
                if (state.active && damage.atlas_frame(n, state.atlas_frame) != Result::ok) {
                    state.active = false;
                    ++undefined_uv_slots;
                }
                state.triangle = VehicleDamageParticles::triangle();
                state.custom_triangle = true;
                visible |= state.active;
                states.push_back(state);
            }
            states.resize(damage.particles().size());
            auto found = combat_particle_ids_.find(key);
            if (found == combat_particle_ids_.end()) {
                if (!visible)
                    continue;
                MeshPart part;
                part.texture = material_kind == 0 ? "16ParticleFX.bmp" : "xxParticleFX.bmp";
                const auto handle = renderer_.create_particles(
                    static_cast<std::uint32_t>(states.size()),
                    material(resources, "Textures", part, material_kind == 0 ? 0x127 : 0x125),
                    material_kind != 0, true);
                if (!handle.value)
                    return Result::unavailable;
                found =
                    combat_particle_ids_.emplace(key, CombatParticleVisual{handle, states.size()})
                        .first;
            }
            if (renderer_.update_particles(found->second.handle, states) != Result::ok)
                return Result::unavailable;
        }
    }
    const auto submit_pool = [&](std::array<std::size_t, 5> key, const WeaponTrailPool& pool) {
        if (pool.particles().empty())
            return Result::ok;
        auto found = combat_particle_ids_.find(key);
        if (found == combat_particle_ids_.end()) {
            MeshPart part;
            const auto path = std::string(pool.texture_path());
            const auto separator = path.find_last_of('/');
            part.texture = path.substr(separator + 1);
            const auto handle =
                renderer_.create_particles(static_cast<std::uint32_t>(pool.particles().size()),
                                           material(resources, path.substr(0, separator), part,
                                                    pool.uses_tire_atlas() ? 0x125 : 0x127),
                                           pool.uses_tire_atlas());
            if (!handle.value) {
                error = "Native combat particle resource is unavailable";
                return Result::unavailable;
            }
            found = combat_particle_ids_
                        .emplace(key, CombatParticleVisual{handle, pool.particles().size()})
                        .first;
        }
        std::vector<ParticleDrawState> states;
        for (const auto i : pool.draw_order()) {
            if (i >= pool.particles().size())
                return Result::invalid_argument;
            const auto& particle = pool.particles()[i];
            ParticleDrawState state{particle.position, particle.size, particle.color_argb,
                                    particle.atlas_frame, particle.active};
            if (particle.active && pool.atlas_frame(i, state.atlas_frame) != Result::ok) {
                // Original raw frames4..9 have no initialized UV to reconstruct.
                // Omit this undefined visual only, and report the gap explicitly below.
                state.active = false;
                ++undefined_uv_slots;
            }
            if (pool.smoke_screen()) {
                state.triangle = pool.screen_triangles()[i];
                state.custom_triangle = true;
            }
            states.push_back(state);
        }
        states.resize(pool.particles().size());
        return renderer_.update_particles(found->second.handle, states);
    };
    for (const auto& trail : session.scene_trails())
        if (submit_pool({0, trail.key.turret, trail.key.projectile,
                         static_cast<std::size_t>(trail.key.rocket), 0},
                        trail.pool) != Result::ok)
            return Result::unavailable;
    for (const auto& trail : session.vehicle_trails())
        if (submit_pool({1, trail.participant, static_cast<std::size_t>(trail.key.slot),
                         static_cast<std::size_t>(trail.key.kind), trail.key.projectile},
                        trail.pool) != Result::ok)
            return Result::unavailable;
    for (std::size_t i = 0; i < session.nitrous_trails().size(); ++i)
        if (submit_pool({2, i, 0, 0, 0}, session.nitrous_trails()[i]) != Result::ok)
            return Result::unavailable;
    combat_render_status_ =
        undefined_uv_slots
            ? "Unavailable original effect UV: " + std::to_string(undefined_uv_slots) +
                  " undefined slots omitted from this frame"
            : "";
    // Presentation consumes recovered poses and asset ownership; it never advances combat.
    for (const auto& [key, id] : combat_visual_ids_)
        for (const auto index : combat_visuals_[id].instances)
            renderer_.set_instance_visible(instances_[index].handle, false);
    const auto submit = [&](std::array<std::size_t, 4> key, const MeshObject& mesh,
                            const std::string& base, const Transform& pose, bool visible) {
        auto found = combat_visual_ids_.find(key);
        if (found == combat_visual_ids_.end()) {
            if (!visible)
                return Result::ok;
            std::size_t id{};
            const auto result = add_combat_visual(resources, mesh, base, id, error);
            if (result != Result::ok)
                return result;
            found = combat_visual_ids_.emplace(key, id).first;
        }
        return update_combat_visual(found->second, pose, visible);
    };
    const auto& scene = session.scene_combat();
    if (session.weather().enabled()) {
        const auto& weather = session.weather();
        for (std::size_t i = 0; i < weather.particles().size(); ++i) {
            const auto& particle = weather.particles()[i];
            if (submit({session.racers().size() + 1, 0, 0, i}, weather.mesh(), "Textures",
                       original_vehicle_transform(particle.position, particle.heading,
                                                  particle.pitch, 0),
                       particle.visible) != Result::ok)
                return Result::unavailable;
        }
    }
    if (update_turrets(scene.turrets().poses()) != Result::ok) {
        error = "Combat turret pose does not match the uploaded world hierarchy";
        return Result::unavailable;
    }
    for (const auto& visual : scene.projectiles()) {
        if (visual.asset >= scene.assets().size())
            return Result::invalid_argument;
        const auto& asset = scene.assets()[visual.asset];
        const auto* descriptor = find_weapon_descriptor(asset.name);
        if (!descriptor)
            return Result::invalid_argument;
        const auto base = "Cars/Shared/Weapons/" + std::string(weapon_slot_name(descriptor->slot)) +
                          "/" + asset.name;
        if (visual.muzzle_flash && visual.projectile >= asset.muzzle_meshes.size())
            return Result::invalid_argument;
        const std::array<std::size_t, 4> key{0, visual.turret, visual.projectile,
                                             visual.muzzle_flash    ? 1u
                                             : visual.missile_flare ? 2u
                                                                    : 0u};
        const auto& mesh = visual.muzzle_flash    ? asset.muzzle_meshes[visual.projectile]
                           : visual.missile_flare ? asset.flare_mesh
                                                  : asset.ammunition;
        if (submit(key, mesh, base, visual.pose, visual.visible) != Result::ok)
            return Result::unavailable;
        if (visual.muzzle_flash && visual.visible) {
            const auto id = combat_visual_ids_.at(key);
            for (const auto index : combat_visuals_[id].instances)
                if (renderer_.set_instance_vertex_color(instances_[index].handle,
                                                        visual.color_argb) != Result::ok)
                    return Result::unavailable;
        }
    }
    const auto& traps = session.scene_electron_traps();
    trap_visuals_.resize(traps.poses().size());
    for (std::size_t i = 0; i < traps.poses().size(); ++i) {
        const auto& trap = traps.poses()[i];
        const auto pose =
            trap.beam_visible ? std::optional<Transform>{trap.beam_pose} : std::nullopt;
        if (update_skeletal_visual(resources, trap_visuals_[i], traps.geometry(i), pose) !=
            Result::ok)
            return Result::unavailable;
    }
    for (std::size_t i = 0; i < session.scene_electron_trails().size(); ++i)
        if (submit_pool({7, i, 0, 0, 0}, session.scene_electron_trails()[i]) != Result::ok)
            return Result::unavailable;
    for (std::size_t i = 0; i < session.racers().size(); ++i) {
        const auto* body_damage = session.racers()[i].vehicle->damage_visuals();
        if (!body_damage)
            return Result::unavailable;
        damage_visuals_.resize(session.racers().size());
        const auto& body_instances =
            i == session.player_index()
                ? vehicle_instances_
                : opponents_.at(i < session.player_index() ? i : i - 1).instances;
        if (update_damage_visual(resources, *body_damage, body_instances, damage_visuals_[i]) !=
            Result::ok) {
            error = "Native vehicle damage mesh or paint update is unavailable";
            return Result::unavailable;
        }
        const auto* combat = session.racers()[i].vehicle->combat();
        if (!combat)
            return Result::unavailable;
        if (electron_visuals_.size() <= i)
            electron_visuals_.resize(i + 1);
        if (update_skeletal_visual(resources, electron_visuals_[i], combat->electron_geometry(),
                                   combat->electron_beam_pose()) != Result::ok)
            return Result::unavailable;
        if (oil_visuals_.size() <= i)
            oil_visuals_.resize(i + 1);
        auto& oil = oil_visuals_[i];
        const auto triangles = combat->oil_visual_triangles();
        if (oil.triangle_count != triangles.size()) {
            if (oil.instance.value)
                renderer_.destroy_instance(oil.instance);
            if (oil.mesh.value)
                renderer_.destroy_mesh(oil.mesh);
            oil = {};
            if (!triangles.empty()) {
                std::vector<Vec3> vertices;
                std::vector<std::array<float, 2>> coordinates;
                std::vector<std::uint32_t> indices;
                std::vector<std::array<float, 4>> colors;
                for (const auto& triangle : triangles) {
                    const auto uv = tire_particle_atlas_uv(triangle.atlas_frame);
                    for (std::size_t n = 0; n < 3; ++n) {
                        indices.push_back(static_cast<std::uint32_t>(vertices.size()));
                        vertices.push_back(triangle.triangle[n]);
                        coordinates.push_back(uv[n]);
                        colors.push_back({0, 0, 0, 1});
                    }
                }
                MeshPart part;
                part.texture = "xxParticleFX.bmp";
                oil.mesh = renderer_.upload_surface(vertices, coordinates, indices,
                                                    material(resources, "Textures", part, 0x125),
                                                    {}, colors);
                if (!oil.mesh.value)
                    return Result::unavailable;
                oil.instance = renderer_.create_instance(oil.mesh);
                if (!oil.instance.value)
                    return Result::unavailable;
                renderer_.update_instance(oil.instance, {});
                renderer_.set_instance_static(oil.instance, true);
                oil.triangle_count = triangles.size();
            }
        }
        if (const auto* flames = combat->flame_particles()) {
            const std::array<std::size_t, 5> key{3, i, 0, 0, 0};
            auto found = combat_particle_ids_.find(key);
            if (found == combat_particle_ids_.end()) {
                MeshPart part;
                part.texture = "16ParticleFX.bmp";
                const auto handle = renderer_.create_particles(
                    100, material(resources, "Textures", part, 0x127), false, true);
                if (!handle.value)
                    return Result::unavailable;
                found = combat_particle_ids_.emplace(key, CombatParticleVisual{handle, 100}).first;
            }
            std::vector<ParticleDrawState> states(100);
            if (flames->particles.size() > states.size())
                return Result::invalid_argument;
            for (std::size_t n = 0; n < flames->particles.size(); ++n) {
                const auto& particle = flames->particles[n];
                states[n] = {particle.position, 1, particle.color_argb, particle.atlas_frame, true};
            }
            if (renderer_.update_particles(found->second.handle, states) != Result::ok)
                return Result::unavailable;
        }
        for (const auto& visual : combat->visuals()) {
            const auto slot = static_cast<std::size_t>(visual.slot);
            if (slot >= combat->assets().size())
                return Result::invalid_argument;
            const auto& asset = combat->assets()[slot];
            const bool mounted = visual.kind == VehicleCombatVisualKind::mounted;
            const bool flash = visual.kind == VehicleCombatVisualKind::muzzle_flash;
            const bool flare = visual.kind == VehicleCombatVisualKind::missile_flare;
            const bool leg = visual.kind == VehicleCombatVisualKind::rear_leg;
            const bool housing = visual.kind == VehicleCombatVisualKind::mount_housing;
            if (!asset.descriptor || (flash     ? visual.index >= asset.muzzle_meshes.size()
                                      : flare   ? !asset.has_flare_mesh
                                      : leg     ? !asset.has_leg_meshes
                                      : housing ? (!asset.has_mount_housing || visual.index >= 2)
                                      : mounted ? !asset.has_weapon_mesh
                                                : !asset.has_ammunition_mesh)) {
                if (visual.visible) {
                    error = "Visible combat instance has no recovered mesh asset";
                    return Result::unavailable;
                }
                continue;
            }
            const auto base =
                housing ? "Cars/" + session.racers()[i].character + "/Paint/Paint Job " +
                              std::to_string(session.racers()[i].loadout.paint_job)
                        : "Cars/Shared/Weapons/" + std::string(weapon_slot_name(visual.slot)) +
                              "/" + std::string(asset.descriptor->name);
            const auto kind = static_cast<std::size_t>(visual.kind);
            const auto& mesh = flash     ? asset.muzzle_meshes[visual.index]
                               : flare   ? asset.flare_mesh
                               : leg     ? asset.leg_meshes[visual.index % 2]
                               : housing ? asset.mount_housing_meshes[visual.index]
                               : mounted ? asset.weapon
                                         : asset.ammunition;
            if (submit({i + 1, slot, kind, visual.index}, mesh, base, visual.pose,
                       visual.visible) != Result::ok)
                return Result::unavailable;
            if ((flash || (mounted && asset.descriptor->name == "Missile Jammer")) &&
                visual.visible) {
                const auto id = combat_visual_ids_.at({i + 1, slot, kind, visual.index});
                for (const auto index : combat_visuals_[id].instances)
                    if (renderer_.set_instance_vertex_color(instances_[index].handle,
                                                            visual.color_argb) != Result::ok) {
                        error = "Native jammer vertex-color override is unavailable";
                        return Result::unavailable;
                    }
            }
        }
    }
    return Result::ok;
}
Result RacePresentation::update_damage_visual(ResourceStore& resources,
                                              const VehicleDamageVisuals& source,
                                              std::span<const std::size_t> indices,
                                              DamageVisual& visual) {
    if (visual.revision && *visual.revision == source.revision())
        return Result::ok;
    const auto& paint = source.paint();
    const auto full_texture = ResourceStore::canonical(source.texture_path());
    if (!visual.paint.value) {
        const auto cached = damage_paint_textures_.find(full_texture);
        if (cached != damage_paint_textures_.end())
            visual.paint = cached->second.handle;
        else {
            visual.paint = renderer_.upload_texture(paint.width, paint.height, paint.rgba);
            if (!visual.paint.value)
                return Result::unavailable;
            textures_.push_back(visual.paint);
            damage_paint_textures_.emplace(
                full_texture, DamagePaintTexture{visual.paint, source.texture_transparency()});
        }
    }
    if (renderer_.update_texture(visual.paint, paint.width, paint.height, paint.rgba) != Result::ok)
        return Result::unavailable;
    const auto slash = full_texture.find_last_of('/');
    const auto paint_name = full_texture.substr(slash + 1);
    const auto body_root = std::find_if(indices.begin(), indices.end(), [&](std::size_t index) {
        return instances_[index].wheel_index < 0 &&
               instances_[index].node_name == source.body_lods()[0].name;
    });
    if (body_root == indices.end())
        return Result::not_found;
    const auto root_path = instances_[*body_root].path;
    for (const auto index : indices) {
        auto& instance = instances_.at(index);
        if (instance.wheel_index >= 0 || !instance.handle.value)
            continue;
        if (instance.path.size() < root_path.size() ||
            !std::equal(root_path.begin(), root_path.end(), instance.path.begin()))
            continue;
        const auto* node = &source.body_lods()[0];
        for (std::size_t depth = root_path.size(); depth < instance.path.size(); ++depth) {
            const auto child = instance.path[depth];
            if (child >= node->children.size())
                return Result::invalid_argument;
            node = &node->children[child];
        }
        if (instance.part_index >= node->parts.size())
            return Result::invalid_argument;
        const auto& part = node->parts[instance.part_index];
        MaterialHandle surface;
        if (ResourceStore::canonical(part.texture) == paint_name) {
            const bool chrome = chrome_mapping_applies(chrome_enabled_, node->flags,
                                                       instance.part_index, chrome_texture_.value);
            std::string key = std::to_string(node->flags) + ":chrome=" +
                              std::to_string(chrome ? chrome_texture_.value : 0);
            for (const auto color : part.color)
                key += ":" + std::to_string(std::bit_cast<std::uint32_t>(color));
            const auto cached = visual.materials.find(key);
            if (cached != visual.materials.end())
                surface = cached->second;
            else {
                const auto global_key = "damage:" + std::to_string(visual.paint.value) + ":" + key;
                const auto shared = materials_.find(global_key);
                if (shared != materials_.end())
                    surface = shared->second;
                else {
                    auto settings =
                        surface_settings(part, node->flags, source.texture_transparency());
                    if (chrome)
                        settings.chrome_map = chrome_texture_;
                    surface = renderer_.create_material(visual.paint, settings);
                    if (!surface.value)
                        return Result::unavailable;
                    materials_.emplace(global_key, surface);
                }
                visual.materials.emplace(key, surface);
            }
        } else {
            const auto base = std::string(source.texture_path().substr(0, slash));
            surface = material(resources, base, part, node->flags,
                chrome_mapping_applies(chrome_enabled_, node->flags, instance.part_index,
                                       chrome_texture_.value));
        }
        std::vector<std::array<float, 2>> uv;
        for (const auto coordinate : part.uv)
            uv.push_back({coordinate.x, coordinate.y});
        const auto mesh = renderer_.upload_surface(part.positions, uv, part.indices, surface,
                                                   part.normals, part.colors);
        if (!mesh.value)
            return Result::unavailable;
        meshes_.push_back(mesh);
        if (renderer_.set_instance_mesh(instance.handle, mesh) != Result::ok)
            return Result::unavailable;
        renderer_.destroy_mesh(instance.mesh);
        const auto previous = std::find_if(meshes_.begin(), meshes_.end(), [&](MeshHandle handle) {
            return handle.value == instance.mesh.value;
        });
        if (previous != meshes_.end())
            *previous = {};
        instance.mesh = mesh;
    }
    visual.revision = source.revision();
    return Result::ok;
}
Result RacePresentation::update_skeletal_visual(ResourceStore& resources, CombatMeshVisual& beam,
                                                const SkeletalMeshRuntime* skeletal,
                                                const std::optional<Transform>& pose) {
    if (beam.instance.value)
        renderer_.set_instance_visible(beam.instance, pose.has_value());
    if (!pose)
        return Result::ok;
    if (!skeletal)
        return Result::unavailable;
    if (!beam.mesh.value || beam.frame != skeletal->frame()) {
        if (beam.instance.value)
            renderer_.destroy_instance(beam.instance);
        if (beam.mesh.value)
            renderer_.destroy_mesh(beam.mesh);
        beam = {};
        const auto& part = skeletal->geometry();
        std::vector<std::array<float, 2>> uv;
        for (const auto coordinate : part.uv)
            uv.push_back({coordinate.x, coordinate.y});
        beam.mesh = renderer_.upload_surface(
            part.positions, uv, part.indices,
            material(resources, "Cars/Shared/Textures", part, 0x107), part.normals, part.colors);
        if (!beam.mesh.value)
            return Result::unavailable;
        beam.instance = renderer_.create_instance(beam.mesh);
        if (!beam.instance.value)
            return Result::unavailable;
        beam.frame = skeletal->frame();
    }
    return renderer_.update_instance(beam.instance, *pose);
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
        if (instance.wheel_index < 0 && vehicle_hierarchy_ && vehicle_angles_) {
            const std::array<MeshNodeEulerOverride, 1> overrides{{{"1024Body", body_angles_}}};
            transform = original_mesh_node_pose(*vehicle_hierarchy_, instance.path, pose.origin,
                                                *vehicle_angles_, overrides);
        }
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
void RacePresentation::set_camera_vehicle_hidden(int target, std::size_t player_index) {
    const auto mask = [&](const auto& indices, bool hidden) {
        for (const auto index : indices)
            renderer_.set_instance_camera_hidden(instances_[index].handle, hidden);
    };
    mask(vehicle_instances_, target == static_cast<int>(player_index));
    for (std::size_t i = 0; i < opponents_.size(); ++i)
        mask(opponents_[i].instances, target == static_cast<int>(i));
    for (const auto& [key, id] : combat_visual_ids_) {
        const auto kind = static_cast<VehicleCombatVisualKind>(key[2]);
        const bool attached = kind == VehicleCombatVisualKind::mounted ||
                              kind == VehicleCombatVisualKind::muzzle_flash ||
                              kind == VehicleCombatVisualKind::rear_leg ||
                              kind == VehicleCombatVisualKind::mount_housing;
        mask(combat_visuals_[id].instances, attached && target >= 0 &&
             key[0] == static_cast<std::size_t>(target + 1));
    }
}
void RacePresentation::snap_racer(std::size_t index, std::size_t player_index) {
    const auto snap = [&](const std::vector<std::size_t>& indices) {
        for (const auto n : indices)
            renderer_.reset_instance_interpolation(instances_[n].handle);
    };
    snap(index == player_index ? vehicle_instances_ : opponents_.at(index).instances);
    for (const auto& [key, id] : combat_visual_ids_)
        if (key[0] == index + 1)
            snap(combat_visuals_[id].instances);
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
    vehicle_angles_ = Vec3{state.heading_degrees, state.pitch_degrees, state.roll_degrees};
    body_angles_ = {0, vehicle.body_orientation().body_pitch, vehicle.body_orientation().body_roll};
    update_lights(state);
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
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0040fc99
void RacePresentation::update_lights(const VehicleState& state) {
    // 00424aa2 enables the overlay on braking; controller release and race reset disable it.
    const bool visible = state.brake_amount != 0;
    if (brake_lights_.empty() || visible == brake_lights_visible_)
        return;
    brake_lights_visible_ = visible;
    for (const auto light : brake_lights_)
        renderer_.set_instance_visible(light, visible);
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
    opponents_.clear();
    vehicle_hierarchy_.reset();
    vehicle_angles_.reset();
    body_angles_ = {};
    combat_visuals_.clear();
    combat_visual_ids_.clear();
    for (const auto& [key, visual] : combat_particle_ids_)
        renderer_.destroy_particles(visual.handle);
    combat_particle_ids_.clear();
    combat_render_status_.clear();
    for (const auto& oil : oil_visuals_) {
        if (oil.instance.value)
            renderer_.destroy_instance(oil.instance);
        if (oil.mesh.value)
            renderer_.destroy_mesh(oil.mesh);
    }
    oil_visuals_.clear();
    for (const auto& beam : electron_visuals_) {
        if (beam.instance.value)
            renderer_.destroy_instance(beam.instance);
        if (beam.mesh.value)
            renderer_.destroy_mesh(beam.mesh);
    }
    electron_visuals_.clear();
    for (const auto& beam : trap_visuals_) {
        if (beam.instance.value)
            renderer_.destroy_instance(beam.instance);
        if (beam.mesh.value)
            renderer_.destroy_mesh(beam.mesh);
    }
    trap_visuals_.clear();
    damage_visuals_.clear();
    damage_paint_textures_.clear();
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
        if (instance.handle.value)
            renderer_.destroy_instance(instance.handle);
    for (auto mesh : meshes_)
        if (mesh.value)
            renderer_.destroy_mesh(mesh);
    for (const auto& [key, material] : materials_)
        renderer_.destroy_material(material);
    for (auto texture : textures_)
        renderer_.destroy_texture(texture);
    instances_.clear();
    vehicle_instances_.clear();
    brake_lights_.clear();
    brake_lights_visible_ = false;
    animated_instances_.clear();
    animated_world_nodes_.clear();
    world_nodes_.clear();
    meshes_.clear();
    materials_.clear();
    textures_.clear();
    chrome_texture_ = {};
    chrome_enabled_ = false;
}
} // namespace roadwars
