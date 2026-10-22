#include "game_src/unresolved/vehicle_combat.hpp"
#include "game_src/unresolved/contact_orientation.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace roadwars {
namespace {
Vec3 add(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 scale(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
Vec3 vector(const Transform& t, Vec3 p) {
    return {t.basis[0].x * p.x + t.basis[1].x * p.y + t.basis[2].x * p.z,
            t.basis[0].y * p.x + t.basis[1].y * p.y + t.basis[2].y * p.z,
            t.basis[0].z * p.x + t.basis[1].z * p.y + t.basis[2].z * p.z};
}
Vec3 point(const Transform& t, Vec3 p) { return add(t.origin, vector(t, p)); }
Transform compose(const Transform& a, const Transform& b) {
    return {{{vector(a, b.basis[0]), vector(a, b.basis[1]), vector(a, b.basis[2])}},
            point(a, b.origin)};
}
bool find_node(const MeshObject& n, std::string_view name, std::vector<std::size_t>& path) {
    if (n.name == name)
        return true;
    for (std::size_t i = 0; i < n.children.size(); ++i) {
        path.push_back(i);
        if (find_node(n.children[i], name, path))
            return true;
        path.pop_back();
    }
    return false;
}
Transform node_pose(const MeshObject& root, std::span<const std::size_t> path,
                    std::span<const std::size_t> roll_path = {}, float roll = 0) {
    const std::array<MeshNodeEulerOverride, 1> overrides{{{"ROR1", {0, 0, roll}}}};
    return original_mesh_node_pose(root, path, root.position, root.angles_degrees,
                                   roll_path.empty() ? std::span<const MeshNodeEulerOverride>{}
                                                     : overrides);
}
Result read_mesh(ResourceStore& resources, const std::string& name, MeshObject& mesh,
                 std::string& error) {
    std::vector<std::byte> bytes;
    auto result = resources.read(name, bytes);
    if (result != Result::ok) {
        error = "Original vehicle weapon resource unavailable: " + name;
        return result;
    }
    return decode_msh(bytes, mesh, error);
}
Result bounds(const MeshObject& mesh, WeaponRocketShape& shape, std::string& error) {
    Vec3 low{INFINITY, INFINITY, INFINITY}, high{-INFINITY, -INFINITY, -INFINITY};
    for (const auto& part : mesh.parts)
        for (const auto p : part.positions) {
            if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z))
                return Result::invalid_argument;
            low = {std::min(low.x, p.x), std::min(low.y, p.y), std::min(low.z, p.z)};
            high = {std::max(high.x, p.x), std::max(high.y, p.y), std::max(high.z, p.z)};
        }
    shape = {high.z - low.z, high.x - low.x, high.y - low.y};
    if (!std::isfinite(shape.length) || shape.length <= 0 || shape.width <= 0 ||
        shape.height <= 0) {
        error = "Original weapon root bounds are unavailable: " + mesh.name;
        return Result::unavailable;
    }
    return Result::ok;
}
std::vector<WeaponProjectileTarget> projectile_targets(const VehicleCombatFrame& frame) {
    std::vector<WeaponProjectileTarget> output;
    for (const auto& t : frame.targets)
        output.push_back({t.participant, t.identity, t.position, t.height, t.footprint});
    return output;
}
std::vector<SpecialWeaponTarget> special_targets(const VehicleCombatFrame& frame) {
    std::vector<SpecialWeaponTarget> output;
    for (const auto& t : frame.targets)
        output.push_back({t.participant, static_cast<std::uint64_t>(t.identity), t.footprint});
    return output;
}
std::vector<RearInstanceTarget> rear_targets(const VehicleCombatFrame& frame) {
    std::vector<RearInstanceTarget> output;
    for (const auto& t : frame.targets)
        output.push_back({t.participant, static_cast<std::uint64_t>(t.identity), t.position,
                          t.footprint, t.air_time});
    return output;
}
Result effect_count(const VehicleCombatServices& services, VehicleCombatEffectKey key,
                    std::size_t& count, std::string& error) {
    if (!services.effect_count || services.effect_count(key, count) != Result::ok) {
        error = "Original vehicle weapon particle count unavailable";
        return Result::unavailable;
    }
    return Result::ok;
}
Result effect_step(const VehicleCombatServices& services, const VehicleCombatEffectStep& step,
                   std::string& error) {
    if (!services.effect_step || services.effect_step(step) != Result::ok) {
        error = "Original vehicle weapon effect update unavailable";
        return Result::unavailable;
    }
    return Result::ok;
}
Result emit_damage(const VehicleCombatServices& services, const VehicleCombatDamage& event,
                   std::string& error) {
    if (!services.damage || services.damage(event) != Result::ok) {
        error = "Original vehicle weapon damage dispatch unavailable";
        return Result::unavailable;
    }
    return Result::ok;
}
Result emit_impact(const VehicleCombatServices& services, const VehicleCombatImpact& event,
                   std::string& error) {
    if (!services.impact || services.impact(event) != Result::ok) {
        error = "Original vehicle weapon impact/explosion callback unavailable";
        return Result::unavailable;
    }
    return Result::ok;
}
} // namespace

// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0040face
// Weapon mount discovery subset; the remaining vehicle construction belongs elsewhere.
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00410ea2
// Factory branch ownership/attachment subset, not every native constructor/resource effect.
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0044e926
// Actual guided model bounds and flare-child attachment subset.
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0049f460
// Actual Rocket model bounds and flare-child attachment subset.
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0040667f
// Actual Leg1/Leg2 mesh ownership and attachment subset.
Result VehicleCombat::load(ResourceStore& resources, std::string_view car,
                           const std::array<std::string, 3>& weapons, std::string& error) {
    VehicleCombat next;
    if (read_mesh(resources, "Cars/" + std::string(car) + "/Body/SpatialObject.msh", next.body_,
                  error) != Result::ok)
        return Result::unavailable;
    // Every existing side mount overwrites vehicle+4e24: the final index wins.
    std::string side_mount;
    for (unsigned i = 1; i <= 3; ++i) {
        std::vector<std::size_t> path;
        const auto name = "ULRORMount" + std::to_string(i);
        if (find_node(next.body_, name, path))
            side_mount = name;
    }
    for (std::size_t i = 0; i < 3; ++i) {
        auto& asset = next.assets_[i];
        auto& item = next.items_[i];
        if (weapons[i].empty())
            continue;
        asset.descriptor = find_weapon_descriptor(weapons[i]);
        if (!asset.descriptor || static_cast<std::size_t>(asset.descriptor->slot) != i) {
            error = "Original weapon slot mismatch: " + weapons[i];
            return Result::invalid_argument;
        }
        const auto name = asset.descriptor->name;
        if (load_weapon_parameters(resources, *asset.descriptor, asset.parameters, error) !=
            Result::ok)
            return Result::unavailable;
        asset.mount_name = i == 0   ? "FrontWeaponMount"
                           : i == 2 ? "RearWeaponMount"
                                    : side_mount + "Weapon";
        if (!find_node(next.body_, asset.mount_name, asset.mount_path) ||
            (i == 1 && (side_mount.empty() || !find_node(next.body_, "ROR1", next.roll_path_)))) {
            error = "Original vehicle weapon attachment unavailable: " + asset.mount_name;
            return Result::unavailable;
        }
        item.mount.initialize(asset.descriptor->slot, asset.parameters.default_ammunition);
        if (i == 1) {
            find_node(next.body_, side_mount, asset.mount_housing_paths[0]);
            asset.mount_housing_paths[1] = next.roll_path_;
            for (std::size_t part = 0; part < 2; ++part) {
                const auto* node = &next.body_;
                for (const auto index : asset.mount_housing_paths[part])
                    node = &node->children[index];
                asset.mount_housing_meshes[part] = *node;
                auto& mesh = asset.mount_housing_meshes[part];
                mesh.position = {};
                mesh.angles_degrees = {};
                mesh.visible = true;
                if (part == 0) {
                    std::vector<std::size_t> path;
                    if (find_node(mesh, "ROR1", path)) {
                        auto* rotating = &mesh;
                        for (const auto index : path)
                            rotating = &rotating->children[index];
                        rotating->visible = false;
                    }
                }
            }
            asset.has_mount_housing = true;
        }
        const auto base = "Cars/Shared/Weapons/" +
                          std::string(weapon_slot_name(asset.descriptor->slot)) + "/" +
                          std::string(name) + "/";
        if (name == "Machine Gun" || name == "20mm Cannon" || name == "Plasma Blaster" ||
            name == "Vulcanized Gun" || name == "Gatling Gun") {
            item.kind = Kind::salvo;
            if (item.salvo.initialize(name, error) != Result::ok)
                return Result::unavailable;
            item.projectiles.resize(item.salvo.pool_capacity());
        } else if (weapon_is_guided(name))
            item.kind = Kind::guided;
        else if (name == "Rocket Launcher")
            item.kind = Kind::rocket;
        else if (name == "Battering Spikes" || name == "Hydraulic Ram")
            item.kind = Kind::contact;
        else if (name == "Electron Cannon")
            item.kind = Kind::electron;
        else if (name == "Flame Thrower")
            item.kind = Kind::flame;
        else if (name == "Missile Jammer")
            item.kind = Kind::jammer;
        else if (name == "Oil Slick")
            item.kind = Kind::oil;
        else if (name == "Smoke Screen")
            item.kind = Kind::smoke;
        else {
            item.kind = Kind::rear;
            item.deploy_kind = name == "Grenades"         ? RearDeployKind::grenades
                               : name == "Land Mines"     ? RearDeployKind::land_mines
                               : name == "Reactive Mines" ? RearDeployKind::reactive_mines
                                                          : RearDeployKind::arachnoids;
            const auto instance_kind = name == "Grenades"         ? RearInstanceKind::grenade
                                       : name == "Land Mines"     ? RearInstanceKind::land_mine
                                       : name == "Reactive Mines" ? RearInstanceKind::reactive_mine
                                                                  : RearInstanceKind::arachnoid;
            for (auto& instance : item.instances)
                instance.kind = instance_kind;
        }
        asset.has_weapon_mesh =
            item.kind != Kind::rear && item.kind != Kind::oil && item.kind != Kind::smoke;
        asset.has_ammunition_mesh = item.kind == Kind::salvo || item.kind == Kind::rear ||
                                    item.kind == Kind::rocket || item.kind == Kind::guided;
        if (asset.has_weapon_mesh) {
            if (read_mesh(resources, base + "SpatialObject.msh", asset.weapon, error) != Result::ok)
                return Result::unavailable;
            asset.weapon.position = {};
            asset.weapon.dummy_heading_degrees = 0;
        }
        if (item.kind == Kind::guided || item.kind == Kind::rocket || item.kind == Kind::contact) {
            if (bounds(asset.weapon, asset.shape, error) != Result::ok)
                return Result::unavailable;
        }
        if (item.kind == Kind::guided || item.kind == Kind::rocket) {
            std::vector<std::size_t> path;
            if (!find_node(asset.weapon, "39ULmissFlar", path)) {
                error = "Original missile flare node unavailable";
                return Result::unavailable;
            }
            asset.flare_position = node_pose(asset.weapon, path).origin;
            MeshObject* node = &asset.weapon;
            for (const auto index : path)
                node = &node->children[index];
            asset.flare_mesh = *node;
            asset.flare_mesh.position = {};
            asset.flare_mesh.visible = true;
            asset.has_flare_mesh = true;
            node->visible = false;
        }
        if (item.kind == Kind::salvo) {
            const unsigned muzzle_count = name == "Machine Gun" ? 2 : name == "Gatling Gun" ? 5 : 1;
            for (unsigned m = 0; m < muzzle_count; ++m) {
                std::vector<std::size_t> path;
                if (!find_node(asset.weapon, item.salvo.muzzle_name(m), path)) {
                    error = "Original weapon muzzle unavailable: " + std::string(name);
                    return Result::unavailable;
                }
                asset.muzzle_positions.push_back(node_pose(asset.weapon, path).origin);
                MeshObject* node = &asset.weapon;
                for (const auto index : path)
                    node = &node->children[index];
                asset.muzzle_meshes.push_back(*node);
                asset.muzzle_meshes.back().position = {};
                asset.muzzle_meshes.back().visible = true;
                node->visible = false;
            }
        }
        if (item.kind == Kind::salvo || item.kind == Kind::rear) {
            if (read_mesh(resources, base + "AmmoSpatialObject.msh", asset.ammunition, error) !=
                Result::ok)
                return Result::unavailable;
        } else if (item.kind == Kind::guided || item.kind == Kind::rocket)
            asset.ammunition = asset.weapon;
        if (asset.has_ammunition_mesh) {
            asset.ammunition.position = {};
            asset.ammunition.dummy_heading_degrees = 0;
        }
        if (name == "Arachnoids") {
            for (unsigned leg = 0; leg < 2; ++leg) {
                std::vector<std::size_t> path;
                if (!find_node(asset.ammunition, leg == 0 ? "Leg1" : "Leg2", path)) {
                    error = "Original Arachnoid leg geometry unavailable";
                    return Result::unavailable;
                }
                asset.leg_positions[leg] = node_pose(asset.ammunition, path).origin;
                auto* node = &asset.ammunition;
                for (const auto index : path)
                    node = &node->children[index];
                asset.leg_meshes[leg] = *node;
                asset.leg_meshes[leg].position = {};
                asset.leg_meshes[leg].visible = true;
                node->visible = false;
            }
            asset.has_leg_meshes = true;
        }
        if (item.kind == Kind::guided &&
            item.guided.initialize(name, asset.parameters, {asset.shape.width, asset.shape.height},
                                   error) != Result::ok)
            return Result::unavailable;
        if (item.kind == Kind::contact &&
            item.contact.initialize(name, asset.shape.length, error) != Result::ok)
            return Result::unavailable;
        if (item.kind == Kind::electron &&
            item.electron_mesh.load(resources, base + "Lightning.ska", base + "LightningAnim.ska",
                                    error) != Result::ok)
            return Result::unavailable;
    }
    next.initialized_ = true;
    *this = std::move(next);
    return Result::ok;
}
bool VehicleCombat::active(std::size_t i) const {
    const auto& s = items_[i];
    switch (s.kind) {
    case Kind::salvo:
        return s.salvo.active();
    case Kind::rocket:
        return s.rocket.motion.active;
    case Kind::guided:
        return s.guided.active();
    case Kind::contact:
        return s.contact.active();
    case Kind::electron:
        return s.electron.active_270;
    case Kind::flame:
        return s.flame.active_270;
    case Kind::jammer:
        return s.jammer.active_270;
    case Kind::oil:
        return s.oil.active_270;
    case Kind::smoke:
        return s.smoke.active_270;
    case Kind::rear:
        return s.deployment.active_270;
    default:
        return false;
    }
}
bool VehicleCombat::ready(std::size_t i) const {
    const auto& s = items_[i];
    switch (s.kind) {
    case Kind::salvo:
        return s.salvo.ready();
    case Kind::rocket:
        return s.rocket.ready && !s.rocket.motion.active;
    case Kind::guided:
        return s.guided.ready();
    case Kind::contact:
        return s.contact.ready();
    case Kind::electron:
        return electron_cannon_can_activate(s.electron);
    case Kind::flame:
        return flame_thrower_can_activate(s.flame);
    case Kind::jammer:
        return missile_jammer_can_activate(s.jammer);
    case Kind::oil:
        return oil_slick_can_activate(s.oil);
    case Kind::smoke:
        return !s.smoke.active_270;
    case Kind::rear:
        return !s.deployment.active_270;
    default:
        return false;
    }
}
void VehicleCombat::register_callback(CallbackKind kind, std::size_t slot, std::size_t index) {
    if (std::none_of(callbacks_.begin(), callbacks_.end(), [&](const Callback& c) {
            return c.live && c.kind == kind && c.slot == slot && c.index == index;
        }))
        callbacks_.push_back({kind, slot, index});
    // Native search mutates the global cursor even when this registration already exists.
    if (callback_observer_)
        callback_observer_(callback_token({kind, slot, index}), true);
}
VehicleCombat::CallbackToken VehicleCombat::callback_token(const Callback& c) {
    return 1 + c.slot * 128 + static_cast<std::uint64_t>(c.kind) * 32 + c.index;
}
void VehicleCombat::remove_callback(std::size_t index) {
    callbacks_[index].live = false;
    if (callback_observer_)
        callback_observer_(callback_token(callbacks_[index]), false);
}
void VehicleCombat::observe_callbacks(std::function<void(CallbackToken, bool)> observer) {
    callback_observer_ = std::move(observer);
    if (callback_observer_)
        for (const auto token : callback_tokens())
            callback_observer_(token, true);
}
std::vector<VehicleCombat::CallbackToken> VehicleCombat::callback_tokens() const {
    std::vector<CallbackToken> output;
    for (const auto& c : callbacks_)
        if (c.live)
            output.push_back(callback_token(c));
    return output;
}
Result VehicleCombat::dispatch_callback(CallbackToken token, float dt,
                                        const VehicleCombatFrame& frame,
                                        const TrackCollision& track, const RaceRoute* route,
                                        const VehicleCombatServices& services, std::string& error) {
    const auto tokens = callback_tokens();
    if (std::find(tokens.begin(), tokens.end(), token) == tokens.end()) {
        error = "Vehicle combat callback token is not registered";
        return Result::invalid_argument;
    }
    dispatch_filter_ = token;
    const auto result = advance(dt, frame, track, route, services, error);
    dispatch_filter_.reset();
    return result;
}
bool VehicleCombat::request(WeaponSlot slot, float speed) {
    const auto i = static_cast<std::size_t>(slot);
    if (!initialized_ || i >= 3 || !items_[i].mount.request(speed + 3, ready(i)))
        return false;
    items_[i].missile_offsets_prepared = false;
    register_callback(CallbackKind::mount, i);
    return true;
}
bool VehicleCombat::request(WeaponSlot slot, float speed, float heading, float pitch,
                            float body_pitch) {
    if (!std::isfinite(heading) || !std::isfinite(pitch) || !std::isfinite(body_pitch) ||
        !request(slot, speed))
        return false;
    prepare_missile_attachment(static_cast<std::size_t>(slot), heading, pitch + body_pitch);
    return true;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004cf787
// Native world Euler angles form an offset subsequently stored as LOCAL position004d5750.
void VehicleCombat::prepare_missile_attachment(std::size_t i, float heading, float pitch) {
    auto& item = items_[i];
    if (item.kind != Kind::rocket && item.kind != Kind::guided)
        return;
    const auto& asset = assets_[i];
    const auto count = static_cast<std::size_t>(asset.parameters.ammunition_count);
    item.attached_missile_offsets.assign(count, {});
    constexpr float radians = .017455555498600006f;
    const Vec3 side{std::sin((heading + 90) * radians), 0, std::cos((heading + 90) * radians)};
    const Vec3 up{0, std::cos(pitch * radians), -std::sin(pitch * radians)};
    for (std::size_t m = 0; m < count; ++m)
        if (count > 1) {
            item.attached_missile_offsets[m] =
                scale(side, asset.shape.width * .75f * (m % 2 ? 1 : -1));
            if (m > 1)
                item.attached_missile_offsets[m] =
                    add(item.attached_missile_offsets[m], scale(up, -asset.shape.height));
        }
    item.missile_offsets_prepared = true;
}
Transform VehicleCombat::mounted_pose(std::size_t i, const VehicleCombatFrame& frame) const {
    const std::array<MeshNodeEulerOverride, 2> overrides{
        {{"1024Body", {0, frame.body_pitch, frame.body_roll}},
         {"ROR1", {0, 0, items_[1].mount.deployment_roll()}}}};
    return original_mesh_node_pose(body_, assets_[i].mount_path, frame.vehicle_pose.origin,
                                   {frame.heading, frame.pitch, frame.roll}, overrides);
}
Vec3 VehicleCombat::mounted_angles(std::size_t i, const VehicleCombatFrame& frame) const {
    const std::array<MeshNodeEulerOverride, 2> overrides{
        {{"1024Body", {0, frame.body_pitch, frame.body_roll}},
         {"ROR1", {0, 0, items_[1].mount.deployment_roll()}}}};
    return original_mesh_node_angles(body_, assets_[i].mount_path,
                                     {frame.heading, frame.pitch, frame.roll}, overrides);
}
std::optional<Transform> VehicleCombat::node_world_pose(std::string_view name,
                                                        const VehicleCombatFrame& frame) const {
    std::vector<std::size_t> path;
    if (!initialized_ || !find_node(body_, name, path))
        return std::nullopt;
    const std::array<MeshNodeEulerOverride, 2> overrides{
        {{"1024Body", {0, frame.body_pitch, frame.body_roll}},
         {"ROR1", {0, 0, items_[1].mount.deployment_roll()}}}};
    return original_mesh_node_pose(body_, path, frame.vehicle_pose.origin,
                                   {frame.heading, frame.pitch, frame.roll}, overrides);
}
Result VehicleCombat::begin(std::size_t i, const WeaponFireRequest& fire,
                            const VehicleCombatFrame& frame, const VehicleCombatServices& services,
                            std::string& error) {
    auto& item = items_[i];
    item.launch_pitch = fire.pitch;
    bool launched = false;
    switch (item.kind) {
    case Kind::salvo:
        launched = item.salvo.begin(fire);
        break;
    case Kind::rocket:
        if (launch_weapon_rocket(item.rocket, assets_[i].parameters, fire, launched) != Result::ok)
            return Result::invalid_argument;
        break;
    case Kind::guided:
        if (!item.guided.ready())
            break;
        if (!services.select_guided_target) {
            error = "Original guided launch target selection unavailable";
            return Result::unavailable;
        }
        if (const auto selected = services.select_guided_target(); selected != Result::ok)
            return selected;
        if (item.guided.launch(fire, launched, error, frame.route_node, frame.on_branch) !=
            Result::ok)
            return Result::unavailable;
        break;
    case Kind::contact:
        launched = item.contact.begin(fire);
        break;
    case Kind::electron:
        begin_electron_cannon(item.electron);
        launched = true;
        break;
    case Kind::flame:
        begin_flame_thrower(item.flame);
        launched = true;
        break;
    case Kind::jammer:
        begin_missile_jammer(item.jammer);
        item.jammer_output.heading = frame.heading;
        item.jammer_output.color_argb = 0xffffffff;
        launched = true;
        break;
    case Kind::oil:
        begin_oil_slick(item.oil, frame.route_node, frame.vehicle_pose.origin);
        launched = true;
        break;
    case Kind::smoke:
        begin_smoke_screen(item.smoke);
        launched = true;
        break;
    case Kind::rear:
        item.deployment.timer_2d0 = 0;
        item.deployment.active_270 = true;
        launched = true;
        break;
    default:
        return Result::unavailable;
    }
    if (launched) {
        if (item.kind == Kind::rocket || item.kind == Kind::guided)
            item.missile_flare_enabled = true;
        register_callback(CallbackKind::weapon, i);
        if (item.kind != Kind::jammer) {
            std::optional<CombatAudioEvent> event;
            const auto sound_origin =
                item.kind == Kind::guided ? item.guided.missiles().front().position : fire.origin;
            const auto result = combat_audio_event(
                CombatAudioCue::accepted_fire, assets_[i].descriptor->name, sound_origin, event);
            if (result != Result::ok)
                return result;
            if (event)
                audio_events_.push_back(*event);
        }
    }
    return Result::ok;
}

Result VehicleCombat::update_weapon(std::size_t i, float dt, const VehicleCombatFrame& frame,
                                    const TrackCollision& track, const RaceRoute* route,
                                    const VehicleCombatServices& services, std::string& error) {
    auto& item = items_[i];
    const auto& asset = assets_[i];
    const auto slot = static_cast<WeaponSlot>(i);
    const auto pose = mounted_pose(i, frame);
    const auto angles = mounted_angles(i, frame);
    const auto bodies = projectile_targets(frame);
    const auto targets = special_targets(frame);
    if (item.kind == Kind::salvo) {
        std::optional<WeaponSalvoShot> shot;
        const auto result = item.salvo.advance(dt, angles.x, angles.y, shot, angles.z);
        if (result != Result::ok)
            return result;
        if (shot) {
            bool launched{};
            if (launch_weapon_projectile(item.projectiles[shot->pool_index],
                                         item.salvo.projectile_spec(asset.parameters),
                                         shot->heading, shot->pitch, shot->starting_speed,
                                         point(pose, asset.muzzle_positions[shot->muzzle_index]),
                                         launched) != Result::ok)
                return Result::invalid_argument;
            if (launched)
                register_callback(CallbackKind::projectile, i, shot->pool_index);
        }
        return Result::ok;
    }
    if (item.kind == Kind::contact)
        return item.contact.advance(dt);
    if (item.kind == Kind::rocket) {
        const VehicleCombatEffectKey key{slot, 0, VehicleCombatEffectKind::projectile_trail};
        std::size_t count{};
        if (effect_count(services, key, count, error) != Result::ok)
            return Result::unavailable;
        WeaponProjectileHit hit;
        auto result = advance_weapon_rocket(
            item.rocket, asset.parameters, asset.shape, dt, count == 0, frame.owner_identity,
            bodies, track, hit, [&](bool emitting, Vec3 position, float elapsed) {
                const auto acceleration = asset.parameters.acceleration * elapsed;
                const auto velocity =
                    add(item.rocket.motion.velocity, scale(item.rocket.direction, acceleration));
                return effect_step(services,
                                   {key, elapsed, emitting, position, velocity,
                                    item.rocket.motion.heading_degrees,
                                    item.rocket.motion.pitch_degrees},
                                   error);
            });
        if (result != Result::ok)
            return result;
        if (hit.kind != ProjectileHitKind::none &&
            emit_impact(services,
                        {slot, 0, hit, item.rocket.motion.velocity,
                         item.rocket.motion.heading_degrees, item.rocket.motion.pitch_degrees},
                        error) != Result::ok)
            return Result::unavailable;
        if (hit.kind != ProjectileHitKind::none) {
            std::optional<CombatAudioEvent> event;
            combat_audio_event(hit.kind == ProjectileHitKind::vehicle
                                   ? CombatAudioCue::projectile_vehicle
                                   : CombatAudioCue::projectile_world,
                               asset.descriptor->name, hit.point, event);
            if (event)
                audio_events_.push_back(*event);
        }
        if (hit.kind == ProjectileHitKind::vehicle) {
            VehicleCombatDamage damage{slot,
                                       hit.participant,
                                       hit.contact_side,
                                       asset.parameters.hitpoints,
                                       hit.point,
                                       false,
                                       true,
                                       true,
                                       {}};
            weapon_rocket_impact_impulse(item.rocket, asset.parameters, hit.point, damage.impulse);
            return emit_damage(services, damage, error);
        }
        return Result::ok;
    }
    if (item.kind == Kind::guided) {
        GuidedWeaponServices world;
        world.random = services.random;
        world.ground = [&](Vec3 p, float& height) {
            const auto ground = track.sample(p);
            height = ground.point.y;
            // 004bb23c always writes 004fb769's result, including no-surface -99999.
            return Result::ok;
        };
        auto result = item.guided.begin_update(dt, frame.guided_target, route, world, error);
        if (result != Result::ok)
            return result;
        std::vector<std::size_t> counts(item.guided.missiles().size());
        for (std::size_t m = 0; m < counts.size(); ++m) {
            const auto missile = item.guided.missiles()[m];
            WeaponProjectile motion;
            motion.position = missile.position;
            motion.previous_position = missile.previous_position;
            motion.heading_degrees = missile.heading;
            motion.velocity = missile.velocity;
            motion.scalar_speed = std::sqrt(missile.velocity.x * missile.velocity.x +
                                            missile.velocity.z * missile.velocity.z);
            motion.active = item.guided.active() && !missile.impacted;
            // 00450b3c computes but never tests height. Reuse only the XZ edge intersection.
            auto flat = bodies;
            for (auto& target : flat) {
                target.position.y = missile.position.y;
                target.identity =
                    target.identity == frame.owner_identity && item.guided.age() <= .5f ? 1 : 0;
            }
            auto hit = weapon_projectile_vehicle_hit(motion, 1, flat);
            WeaponProjectileSpec shape;
            shape.diagonal_radius = std::hypot(asset.shape.width * .5f, asset.shape.length * .5f);
            shape.corner_angle_degrees = static_cast<float>(
                std::atan(static_cast<double>(asset.shape.width / asset.shape.length)) *
                57.288352966308594);
            shape.wall_radius = asset.shape.length;
            if (hit.kind == ProjectileHitKind::none)
                hit = weapon_projectile_world_hit(motion, shape, track);
            if (hit.kind != ProjectileHitKind::none) {
                std::optional<CombatAudioEvent> event;
                combat_audio_event(hit.kind == ProjectileHitKind::vehicle
                                       ? CombatAudioCue::projectile_vehicle
                                       : CombatAudioCue::projectile_world,
                                   asset.descriptor->name, hit.point, event);
                if (event)
                    audio_events_.push_back(*event);
                if (emit_impact(
                        services,
                        {slot, m, hit, missile.velocity, missile.heading, item.launch_pitch},
                        error) != Result::ok)
                    return Result::unavailable;
                if (hit.kind == ProjectileHitKind::vehicle) {
                    VehicleCombatDamage damage{slot,
                                               hit.participant,
                                               hit.contact_side,
                                               asset.parameters.hitpoints,
                                               hit.point,
                                               false,
                                               true,
                                               true,
                                               {}};
                    weapon_projectile_impact_impulse(motion, asset.parameters, hit.point,
                                                     damage.impulse);
                    if (emit_damage(services, damage, error) != Result::ok)
                        return Result::unavailable;
                }
                item.guided.record_impact(m);
            }
            const VehicleCombatEffectKey key{slot, m, VehicleCombatEffectKind::guided_trail};
            if (effect_step(services,
                            {key, dt, !missile.impacted && hit.kind == ProjectileHitKind::none,
                             missile.position, missile.velocity, missile.heading,
                             item.launch_pitch},
                            error) != Result::ok ||
                effect_count(services, key, counts[m], error) != Result::ok)
                return Result::unavailable;
        }
        return item.guided.finish_update(counts);
    }
    if (item.kind == Kind::electron) {
        if (item.electron.active_270 && item.electron_mesh.advance(dt) != Result::ok)
            return Result::unavailable;
        std::size_t particles{};
        if (effect_count(services, {slot, 0, VehicleCombatEffectKind::electron_impact}, particles,
                         error) != Result::ok)
            return Result::unavailable;
        auto& output = item.electron_output;
        const auto result = advance_electron_cannon(
            item.electron, frame.owner_identity, pose.origin, frame.heading,
            asset.parameters.hitpoints, targets, particles, dt, output, error);
        if (result != Result::ok)
            return result;
        item.beam_pose = original_skeletal_transform(
            pose.origin, angles.x, -angles.y, 0,
            {item.electron.width_6d4, item.electron.height_6d8, output.beam_length});
        if (output.play_hit_sound) {
            std::optional<CombatAudioEvent> event;
            combat_audio_event(CombatAudioCue::electron_contact, asset.descriptor->name,
                               output.hit_point.value_or(pose.origin), event);
            if (event)
                audio_events_.push_back(*event);
        }
        for (const auto& hit : output.damage)
            if (emit_damage(services,
                            {slot,
                             hit.participant,
                             hit.contact_side,
                             hit.damage,
                             {},
                             false,
                             false,
                             false,
                             {}},
                            error) != Result::ok)
                return Result::unavailable;
        Vec3 target_velocity{};
        if (!output.damage.empty())
            for (const auto& target : frame.targets)
                if (target.participant == output.damage.front().participant) {
                    target_velocity = target.velocity;
                    break;
                }
        return effect_step(services,
                           {{slot, 0, VehicleCombatEffectKind::electron_impact},
                            dt,
                            output.hit_point.has_value(),
                            output.hit_point.value_or(pose.origin),
                            target_velocity,
                            frame.heading,
                            frame.pitch},
                           error);
    }
    if (item.kind == Kind::flame) {
        FlameThrowerOutput output;
        const auto result = advance_flame_thrower(item.flame, frame.owner_identity, pose.origin,
                                                  scale(frame.forward, frame.cached_speed + 10),
                                                  asset.parameters.hitpoints, targets, dt,
                                                  services.random, output, error);
        if (result != Result::ok)
            return result;
        for (const auto& hit : output.damage)
            if (emit_damage(services,
                            {slot,
                             hit.participant,
                             hit.contact_side,
                             hit.damage,
                             {},
                             false,
                             false,
                             false,
                             {}},
                            error) != Result::ok)
                return Result::unavailable;
        return Result::ok;
    }
    if (item.kind == Kind::jammer) {
        if (!services.random || !services.jam) {
            error = "Original jammer shared RNG or guided-weapon dispatch unavailable";
            return Result::unavailable;
        }
        const MissileJammerDraws draws{services.random(), services.random(), services.random()};
        auto& output = item.jammer_output;
        const auto previous_heading = output.heading;
        const auto result = advance_missile_jammer(
            item.jammer,
            {static_cast<std::uint64_t>(frame.owner_identity), frame.vehicle_pose.origin,
             previous_heading, frame.jammer_weapons},
            draws, dt, output, error);
        if (result != Result::ok)
            return result;
        for (const auto target : output.jam_weapon_indices)
            if (services.jam(target, 1) != Result::ok)
                return Result::unavailable;
        return Result::ok;
    }
    if (item.kind == Kind::oil) {
        OilSlickOutput output;
        const auto result = advance_oil_slick(
            item.oil, frame.vehicle_pose.origin, frame.forward, dt,
            [&](std::array<Vec3, 3>& triangle) {
                for (auto& vertex : triangle) {
                    const auto ground = track.sample(vertex);
                    vertex.y = ground.point.y;
                }
                return Result::ok;
            },
            output, error);
        if (output.visual_triangle && item.oil_render_counts[item.oil.current_patch_2dc] < 49) {
            if (!services.random) {
                error = "Original Oil Slick triangle atlas RNG unavailable";
                return Result::unavailable;
            }
            item.oil_visual_triangles.push_back({*output.visual_triangle, services.random() % 4});
            ++item.oil_render_counts[item.oil.current_patch_2dc];
        }
        return result;
    }
    if (item.kind == Kind::smoke) {
        SmokeScreenOutput output;
        const auto result = advance_smoke_screen(item.smoke, dt, output, error);
        if (result != Result::ok)
            return result;
        return effect_step(services,
                           {{slot, 0, VehicleCombatEffectKind::smoke},
                            dt,
                            output.emit,
                            pose.origin,
                            {},
                            frame.heading,
                            frame.pitch},
                           error);
    }
    if (item.kind == Kind::rear) {
        RearDeployOutput output;
        return advance_rear_deployment(
            item.deploy_kind, item.deployment, dt,
            [&](unsigned index) {
                bool launched{};
                const auto result = launch_rear_weapon_instance(item.instances[index], pose.origin,
                                                                frame.heading, launched, error);
                if (launched)
                    register_callback(CallbackKind::rear_instance, i, index);
                if (launched && item.deploy_kind != RearDeployKind::grenades) {
                    std::optional<CombatAudioEvent> event;
                    combat_audio_event(CombatAudioCue::mine_deployment, asset.descriptor->name,
                                       item.instances[index].position, event);
                    if (event)
                        audio_events_.push_back(*event);
                }
                return result;
            },
            output, error);
    }
    return Result::unavailable;
}

Result VehicleCombat::advance(float dt, const VehicleCombatFrame& frame,
                              const TrackCollision& track, const RaceRoute* route,
                              const VehicleCombatServices& services, std::string& error) {
    if (!initialized_)
        return Result::unavailable;
    if (!std::isfinite(dt) || dt < 0 || !std::isfinite(frame.cached_speed))
        return Result::invalid_argument;
    const auto initial_count = callbacks_.size();
    for (std::size_t c = 0; c < initial_count; ++c) {
        const auto callback = callbacks_[c];
        if (dispatch_filter_ && callback_token(callback) != *dispatch_filter_)
            continue;
        auto& item = items_[callback.slot];
        const auto& asset = assets_[callback.slot];
        const auto slot = static_cast<WeaponSlot>(callback.slot);
        if (callback.kind == CallbackKind::mount) {
            std::optional<WeaponFireRequest> fire;
            const bool retractable =
                item.kind == Kind::guided || item.kind == Kind::rocket || !active(callback.slot);
            const auto angles = mounted_angles(callback.slot, frame);
            auto result = item.mount.advance(dt, retractable, angles.x, angles.y,
                                             mounted_pose(callback.slot, frame).origin, fire);
            if (result != Result::ok)
                return result;
            if (fire && begin(callback.slot, *fire, frame, services, error) != Result::ok)
                return Result::unavailable;
            if (!item.mount.pending() && (slot != WeaponSlot::side || !item.mount.visible()))
                remove_callback(c);
        } else if (callback.kind == CallbackKind::weapon) {
            const auto result =
                update_weapon(callback.slot, dt, frame, track, route, services, error);
            if (result != Result::ok)
                return result;
            bool done = ready(callback.slot);
            if (item.kind == Kind::contact)
                done = !item.contact.active() && item.contact.pose().local_z == 0;
            else if (item.kind == Kind::oil || item.kind == Kind::rear)
                done = !active(callback.slot);
            if (done)
                remove_callback(c);
        } else if (callback.kind == CallbackKind::projectile) {
            auto& projectile = item.projectiles[callback.index];
            const auto spec = item.salvo.projectile_spec(asset.parameters);
            const VehicleCombatEffectKey key{slot, callback.index,
                                             VehicleCombatEffectKind::projectile_trail};
            std::size_t particles{};
            if (spec.has_trail && effect_count(services, key, particles, error) != Result::ok)
                return Result::unavailable;
            if (advance_weapon_projectile_motion(projectile, spec, dt, particles) != Result::ok)
                return Result::invalid_argument;
            if (spec.has_trail &&
                (effect_step(services,
                             {key, dt,
                              projectile.active && !projectile.stopping && projectile.age <= 5,
                              projectile.position, projectile.velocity, projectile.heading_degrees,
                              projectile.pitch_degrees},
                             error) != Result::ok ||
                 effect_count(services, key, particles, error) != Result::ok))
                return Result::unavailable;
            auto hit = weapon_projectile_vehicle_hit(projectile, frame.owner_identity,
                                                     projectile_targets(frame));
            if (hit.kind == ProjectileHitKind::none)
                hit = weapon_projectile_world_hit(projectile, spec, track);
            if (hit.kind != ProjectileHitKind::none) {
                std::optional<CombatAudioEvent> event;
                combat_audio_event(hit.kind == ProjectileHitKind::vehicle
                                       ? CombatAudioCue::projectile_vehicle
                                       : CombatAudioCue::projectile_world,
                                   asset.descriptor->name, hit.point, event);
                if (event)
                    audio_events_.push_back(*event);
                if (emit_impact(services,
                                {slot, callback.index, hit, projectile.velocity,
                                 projectile.heading_degrees, projectile.pitch_degrees},
                                error) != Result::ok)
                    return Result::unavailable;
                if (hit.kind == ProjectileHitKind::vehicle) {
                    VehicleCombatDamage damage{slot,
                                               hit.participant,
                                               hit.contact_side,
                                               asset.parameters.hitpoints,
                                               hit.point,
                                               false,
                                               item.salvo.applies_impact_impulse(),
                                               true,
                                               {}};
                    if (damage.has_impulse)
                        weapon_projectile_impact_impulse(projectile, asset.parameters, hit.point,
                                                         damage.impulse);
                    if (emit_damage(services, damage, error) != Result::ok)
                        return Result::unavailable;
                }
                stop_weapon_projectile(projectile, spec, particles);
            }
            if (!projectile.active)
                remove_callback(c);
        } else {
            RearInstanceWorld world;
            world.ground = [&](Vec3 p, GroundContact& contact) {
                contact = track.sample(p);
                return Result::ok;
            };
            world.arachnoid_walls = [&](Vec3 p, std::vector<WallRecord>& walls) {
                walls = track.nearby_walls(p, p, 1, true);
                return Result::ok;
            };
            if (services.random)
                world.random = [&]() -> std::optional<std::uint32_t> { return services.random(); };
            RearInstanceOutput output;
            const auto result = advance_rear_weapon_instance(
                item.instances[callback.index],
                {asset.parameters.hitpoints, asset.parameters.acceleration,
                 asset.parameters.ammunition_mass_kg},
                frame.owner_identity, rear_targets(frame), world, dt, output, error);
            if (result != Result::ok)
                return result;
            if (output.exploded && item.deploy_kind == RearDeployKind::grenades) {
                std::optional<CombatAudioEvent> event;
                combat_audio_event(CombatAudioCue::grenade_explosion, asset.descriptor->name,
                                   output.explosion_position, event);
                if (event)
                    audio_events_.push_back(*event);
            }
            if (output.exploded &&
                emit_impact(services,
                            {slot,
                             callback.index,
                             {ProjectileHitKind::ground, output.explosion_position},
                             {},
                             item.instances[callback.index].heading,
                             0,
                             true,
                             output.extra_reactive_effect},
                            error) != Result::ok)
                return Result::unavailable;
            for (const auto& hit : output.impacts) {
                VehicleCombatDamage damage{slot,
                                           hit.participant,
                                           0,
                                           hit.internal_damage,
                                           output.explosion_position,
                                           true,
                                           true,
                                           hit.apply_yaw,
                                           {hit.force, hit.application}};
                if (emit_damage(services, damage, error) != Result::ok)
                    return Result::unavailable;
            }
            if (!item.instances[callback.index].callback_running)
                remove_callback(c);
        }
    }
    std::erase_if(callbacks_, [](const Callback& c) { return !c.live; });
    update_visuals(frame);
    return Result::ok;
}
void VehicleCombat::update_visuals(const VehicleCombatFrame& frame) {
    visuals_.clear();
    for (std::size_t i = 0; i < 3; ++i) {
        const auto& asset = assets_[i];
        const auto& item = items_[i];
        const auto slot = static_cast<WeaponSlot>(i);
        if (!asset.descriptor)
            continue;
        auto pose = mounted_pose(i, frame);
        const auto angles = mounted_angles(i, frame);
        if (item.mount.pending() && (item.kind == Kind::rocket || item.kind == Kind::guided) &&
            !item.missile_offsets_prepared)
            prepare_missile_attachment(i, frame.heading, frame.pitch + frame.body_pitch);
        if (item.kind == Kind::jammer) {
            pose = frame.vehicle_pose;
            const auto delta =
                original_vehicle_transform({}, item.jammer_output.heading - frame.heading, 0, 0);
            for (auto& basis : pose.basis)
                basis = vector(delta, basis);
        }
        if (item.kind == Kind::contact)
            pose.origin = point(pose, {0, 0, item.contact.pose().local_z});
        bool root_visible = asset.has_weapon_mesh && item.kind != Kind::guided &&
                            item.kind != Kind::rocket && item.mount.visible();
        if (item.kind == Kind::jammer)
            root_visible = !item.jammer.ready_540;
        visuals_.push_back(
            {slot, 0, VehicleCombatVisualKind::mounted, pose, root_visible,
             item.kind == Kind::jammer ? item.jammer_output.color_argb : 0xffffffff});
        if (asset.has_mount_housing) {
            const std::array<MeshNodeEulerOverride, 2> overrides{
                {{"1024Body", {0, frame.body_pitch, frame.body_roll}},
                 {"ROR1", {0, 0, item.mount.deployment_roll()}}}};
            for (std::size_t part = 0; part < 2; ++part)
                visuals_.push_back(
                    {slot, part, VehicleCombatVisualKind::mount_housing,
                     original_mesh_node_pose(body_, asset.mount_housing_paths[part],
                                             frame.vehicle_pose.origin,
                                             {frame.heading, frame.pitch, frame.roll}, overrides),
                     item.mount.visible() &&
                         (part == 0 || (asset.mount_housing_paths[0].size() <=
                                            asset.mount_housing_paths[1].size() &&
                                        std::equal(asset.mount_housing_paths[0].begin(),
                                                   asset.mount_housing_paths[0].end(),
                                                   asset.mount_housing_paths[1].begin())))});
        }
        for (std::size_t m = 0; m < item.projectiles.size(); ++m) {
            const auto& p = item.projectiles[m];
            visuals_.push_back(
                {slot, m, VehicleCombatVisualKind::projectile,
                 original_vehicle_transform(p.position, p.heading_degrees, p.pitch_degrees, 0),
                 p.active && !item.salvo.projectile_spec(asset.parameters).has_trail});
        }
        if (item.kind == Kind::salvo)
            for (std::size_t m = 0; m < item.salvo.muzzle_flashes().size(); ++m) {
                const auto& flash = item.salvo.muzzle_flashes()[m];
                auto flash_pose = original_vehicle_transform(
                    point(mounted_pose(i, frame), asset.muzzle_positions[m]), angles.x, angles.y,
                    angles.z + flash.local_roll);
                for (auto& basis : flash_pose.basis)
                    basis = scale(basis, flash.scale);
                visuals_.push_back({slot, m, VehicleCombatVisualKind::muzzle_flash, flash_pose,
                                    flash.visible && item.mount.visible(), flash.color_argb});
            }
        if (item.kind == Kind::rocket) {
            const auto& p = item.rocket.motion;
            const bool attached = item.mount.pending();
            const auto offset =
                item.attached_missile_offsets.empty() ? Vec3{} : item.attached_missile_offsets[0];
            const auto body_pose =
                attached
                    ? original_vehicle_transform(point(pose, offset), angles.x, angles.y, angles.z)
                    : original_vehicle_transform(p.position, p.heading_degrees, p.pitch_degrees, 0);
            visuals_.push_back(
                {slot, 0, VehicleCombatVisualKind::projectile, body_pose, attached || p.active});
            const auto flare_pose = original_vehicle_transform(
                point(body_pose, asset.flare_position), attached ? angles.x : p.heading_degrees,
                attached ? angles.y : p.pitch_degrees,
                item.rocket.flare_roll + (attached ? angles.z : 0));
            visuals_.push_back({slot, 0, VehicleCombatVisualKind::missile_flare, flare_pose,
                                item.missile_flare_enabled && (attached || p.active)});
        }
        for (std::size_t m = 0; m < item.guided.missiles().size(); ++m) {
            const auto& p = item.guided.missiles()[m];
            const bool attached = item.mount.pending();
            const auto attached_heading =
                angles.x + (asset.descriptor->name == "Back Missile" ? 180 : 0);
            const auto body_pose =
                attached ? original_vehicle_transform(point(pose, item.attached_missile_offsets[m]),
                                                      attached_heading, angles.y, angles.z)
                         : original_vehicle_transform(p.position, p.heading, item.launch_pitch, 0);
            visuals_.push_back(
                {slot, m, VehicleCombatVisualKind::projectile, body_pose, attached || p.visible});
            const auto flare_pose = original_vehicle_transform(
                point(body_pose, asset.flare_position), attached ? attached_heading : p.heading,
                attached ? angles.y : item.launch_pitch, p.flare_roll + (attached ? angles.z : 0));
            visuals_.push_back({slot, m, VehicleCombatVisualKind::missile_flare, flare_pose,
                                item.missile_flare_enabled && (attached || p.visible)});
        }
        if (item.kind == Kind::rear)
            for (std::size_t m = 0; m < item.instances.size(); ++m) {
                const auto& p = item.instances[m];
                visuals_.push_back({slot, m, VehicleCombatVisualKind::rear_instance,
                                    original_vehicle_transform(p.position, p.heading, 0, 0),
                                    p.active});
                if (asset.has_leg_meshes) {
                    const auto parent = original_vehicle_transform(p.position, p.heading, 0, 0);
                    for (unsigned leg = 0; leg < 2; ++leg) {
                        const auto local = original_vehicle_transform(
                            asset.leg_positions[leg],
                            leg == 0 ? p.left_leg_heading : p.right_leg_heading, 0, 0);
                        visuals_.push_back({slot, m * 2 + leg, VehicleCombatVisualKind::rear_leg,
                                            compose(parent, local), p.active});
                    }
                }
            }
    }
}
std::vector<CombatAudioEvent> VehicleCombat::drain_audio_events() {
    std::vector<CombatAudioEvent> output;
    output.swap(audio_events_);
    return output;
}
VehicleCombatSlotView VehicleCombat::slot(WeaponSlot slot) const {
    const auto i = static_cast<std::size_t>(slot);
    if (i >= 3)
        return {};
    const auto& item = items_[i];
    std::optional<float> elapsed;
    switch (item.kind) {
    case Kind::salvo:
        elapsed = item.salvo.elapsed();
        break;
    case Kind::rocket:
        elapsed = item.rocket.motion.age;
        break;
    case Kind::guided:
        elapsed = item.guided.age();
        break;
    case Kind::electron:
        elapsed = item.electron.elapsed_6b8;
        break;
    case Kind::flame:
        elapsed = item.flame.elapsed_2f2c;
        break;
    case Kind::jammer:
        elapsed = item.jammer.elapsed_53c;
        break;
    case Kind::oil:
        elapsed = item.oil.elapsed_2d0;
        break;
    case Kind::smoke:
        elapsed = item.smoke.elapsed_c88;
        break;
    case Kind::rear:
        elapsed = item.deployment.timer_2d0;
        break;
    default:
        break;
    }
    return {assets_[i].descriptor,
            assets_[i].parameters,
            item.mount.ammunition(),
            active(i),
            ready(i),
            item.mount.pending(),
            item.mount.deployment_roll(),
            elapsed};
}
Result VehicleCombat::jam(float seconds) {
    if (!initialized_ || !std::isfinite(seconds) || seconds < 0)
        return Result::invalid_argument;
    // Non-guided side weapons' virtual+3c is an evidenced no-op (e.g.0049f7eb).
    return items_[1].kind == Kind::guided ? items_[1].guided.jam(seconds) : Result::ok;
}
std::span<const GuidedMissileState> VehicleCombat::guided_missiles() const {
    return items_[1].kind == Kind::guided ? items_[1].guided.missiles()
                                          : std::span<const GuidedMissileState>{};
}
const ContactWeaponRuntime* VehicleCombat::contact_weapon() const {
    return items_[0].kind == Kind::contact ? &items_[0].contact : nullptr;
}
const OilSlickState* VehicleCombat::oil_slick() const {
    return items_[2].kind == Kind::oil ? &items_[2].oil : nullptr;
}
const FlameParticlePool* VehicleCombat::flame_particles() const {
    return items_[0].kind == Kind::flame ? &items_[0].flame.pool : nullptr;
}
const ElectronCannonState* VehicleCombat::electron_state() const {
    return items_[0].kind == Kind::electron ? &items_[0].electron : nullptr;
}
const ElectronCannonOutput* VehicleCombat::electron_output() const {
    return items_[0].kind == Kind::electron ? &items_[0].electron_output : nullptr;
}
const SkeletalMeshRuntime* VehicleCombat::electron_geometry() const {
    return items_[0].kind == Kind::electron ? &items_[0].electron_mesh : nullptr;
}
std::optional<Transform> VehicleCombat::electron_beam_pose() const {
    return items_[0].kind == Kind::electron && items_[0].electron.beam_attached
               ? std::optional<Transform>{items_[0].beam_pose}
               : std::nullopt;
}
const MissileJammerOutput* VehicleCombat::jammer_output() const {
    return items_[2].kind == Kind::jammer ? &items_[2].jammer_output : nullptr;
}
std::span<const VehicleCombatOilVisual> VehicleCombat::oil_visual_triangles() const {
    return items_[2].oil_visual_triangles;
}
void VehicleCombat::cancel() {
    for (auto& item : items_) {
        item.mount.cancel();
        item.salvo.cancel();
        item.contact.cancel();
        item.guided.cancel();
        item.rocket.motion.active = false;
        item.electron.active_270 = item.flame.active_270 = item.jammer.active_270 = false;
        item.oil.active_270 = item.smoke.active_270 = item.deployment.active_270 = false;
    }
}
} // namespace roadwars
