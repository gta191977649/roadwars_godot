#include "game_src/unresolved/scene_turret_combat.hpp"
#include "game_src/unresolved/contact_orientation.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>

namespace roadwars {
SceneTurretCombat::CallbackToken SceneTurretCombat::callback_token(const Callback& callback) {
    return 1 + callback.turret * 128 + static_cast<std::uint64_t>(callback.kind) * 32 +
           callback.projectile;
}
void SceneTurretCombat::register_callback(CallbackKind kind, std::size_t turret,
                                          std::size_t index) {
    const Callback callback{kind, turret, index};
    const auto token = callback_token(callback);
    if (std::none_of(callbacks_.begin(), callbacks_.end(),
                     [&](const Callback& c) { return c.live && callback_token(c) == token; }))
        callbacks_.push_back(callback);
    if (callback_observer_)
        callback_observer_(token, true);
}
void SceneTurretCombat::remove_callback(std::size_t index) {
    callbacks_[index].live = false;
    if (callback_observer_)
        callback_observer_(callback_token(callbacks_[index]), false);
}
void SceneTurretCombat::observe_callbacks(std::function<void(CallbackToken, bool)> observer) {
    callback_observer_ = std::move(observer);
    if (callback_observer_)
        for (const auto token : callback_tokens())
            callback_observer_(token, true);
}
std::vector<SceneTurretCombat::CallbackToken> SceneTurretCombat::callback_tokens() const {
    std::vector<CallbackToken> output;
    for (const auto& c : callbacks_)
        if (c.live)
            output.push_back(callback_token(c));
    return output;
}
Result SceneTurretCombat::advance_controllers(float dt, std::span<const TurretTarget> targets,
                                              std::string& error, bool unlocked) {
    for (std::size_t i = 0; i < items_.size(); ++i) {
        std::optional<RaceTurretFireEvent> event;
        const auto result = turrets_.advance_one(i, dt, targets, event, error, unlocked);
        if (result != Result::ok)
            return result;
        if (!event)
            continue;
        auto& item = items_[i];
        const auto& asset = assets_[item.asset];
        const WeaponFireRequest request{event->firing.origin, event->firing.heading,
                                        event->firing.pitch, event->firing.speed};
        bool accepted{};
        if (asset.rocket) {
            bool launched{};
            const auto launch =
                launch_weapon_rocket(item.rocket, asset.parameters, request, launched);
            if (launch != Result::ok)
                return launch;
            if (launched) {
                accepted = true;
                register_callback(CallbackKind::rocket, i);
                ++launched_;
            }
        } else if (item.salvo.begin(request)) {
            register_callback(CallbackKind::salvo, i);
            accepted = true;
        }
        if (accepted) {
            std::optional<CombatAudioEvent> audio;
            combat_audio_event(CombatAudioCue::accepted_fire, asset.name, request.origin, audio);
            if (audio)
                audio_events_.push_back(*audio);
        }
    }
    return Result::ok;
}
Result SceneTurretCombat::dispatch_callback(CallbackToken token, float dt,
                                            std::span<const TurretTarget> targets,
                                            std::span<const WeaponProjectileTarget> bodies,
                                            const TrackCollision& track,
                                            const SceneCombatServices& services, std::string& error,
                                            bool unlocked) {
    const auto tokens = callback_tokens();
    if (std::find(tokens.begin(), tokens.end(), token) == tokens.end()) {
        error = "Scene combat callback token is not registered";
        return Result::invalid_argument;
    }
    dispatch_filter_ = token;
    const auto result = advance(dt, targets, bodies, track, services, error, unlocked);
    dispatch_filter_.reset();
    return result;
}
namespace {
constexpr std::int32_t kSceneIdentity = std::numeric_limits<std::int32_t>::min();
Vec3 add(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 point(const Transform& pose, Vec3 local) {
    return {pose.origin.x + pose.basis[0].x * local.x + pose.basis[1].x * local.y +
                pose.basis[2].x * local.z,
            pose.origin.y + pose.basis[0].y * local.x + pose.basis[1].y * local.y +
                pose.basis[2].y * local.z,
            pose.origin.z + pose.basis[0].z * local.x + pose.basis[1].z * local.y +
                pose.basis[2].z * local.z};
}
bool finite(Vec3 p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }
Result read_mesh(ResourceStore& resources, const std::string& path, MeshObject& mesh,
                 std::string& error) {
    std::vector<std::byte> bytes;
    const auto result = resources.read(path, bytes);
    if (result != Result::ok) {
        error = "Original turret weapon mesh unavailable: " + path;
        return result;
    }
    return decode_msh(bytes, mesh, error);
}
Result locate_muzzle(MeshObject& node, Vec3 parent, bool transformed, std::string_view name,
                     Vec3& position, std::size_t& matches, std::string& error) {
    const auto local = add(parent, node.position);
    if (!finite(local))
        return Result::invalid_argument;
    if (node.name == name) {
        if (transformed || node.type == 4) {
            error = "Original muzzle has an unrecovered dummy basis: " + std::string(name);
            return Result::unavailable;
        }
        position = local;
        // The caller copies the actual flash subtree before hiding it in the persistent gun mesh.
        node.visible = false;
        ++matches;
    }
    for (auto& child : node.children) {
        const auto result = locate_muzzle(child, local, transformed || node.type == 4, name,
                                          position, matches, error);
        if (result != Result::ok)
            return result;
    }
    return Result::ok;
}
Result rocket_bounds(const MeshObject& mesh, WeaponRocketShape& output, std::string& error) {
    Vec3 low{std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(),
             std::numeric_limits<float>::infinity()};
    Vec3 high{-low.x, -low.y, -low.z};
    for (const auto& part : mesh.parts)
        for (const auto p : part.positions) {
            if (!finite(p))
                return Result::invalid_argument;
            low = {std::min(low.x, p.x), std::min(low.y, p.y), std::min(low.z, p.z)};
            high = {std::max(high.x, p.x), std::max(high.y, p.y), std::max(high.z, p.z)};
        }
    output = {high.z - low.z, high.x - low.x, high.y - low.y};
    if (!finite(low) || !finite(high) || output.length <= 0 || output.width <= 0 ||
        output.height <= 0) {
        error = "Original rocket root mesh bounds are unavailable";
        return Result::unavailable;
    }
    return Result::ok;
}
} // namespace
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004c7c80
// Weapon ownership/attachment subset; use the original scene turret controller separately.
Result SceneTurretCombat::load(ResourceStore& resources, std::string_view track,
                               const MeshObject& world, std::string& error) {
    SceneTurretCombat next;
    const auto result = next.turrets_.load(resources, track, world, error);
    if (result != Result::ok)
        return result;
    std::map<std::string, std::size_t> asset_map;
    for (const auto& turret : next.turrets_.poses()) {
        TurretParameters parsed;
        if (parse_turret_parameters(turret.name, parsed) != Result::ok)
            return Result::invalid_argument;
        Item item;
        const auto cached = asset_map.find(parsed.weapon);
        if (cached != asset_map.end())
            item.asset = cached->second;
        else {
            SceneCombatWeaponAsset asset;
            asset.name = parsed.weapon;
            asset.rocket = parsed.weapon == "Rocket Launcher";
            asset.has_trail = asset.rocket || parsed.weapon == "Plasma Blaster";
            next.requires_trails_ = next.requires_trails_ || asset.has_trail;
            const auto* descriptor = find_weapon_descriptor(asset.name);
            if (!descriptor || load_weapon_parameters(resources, *descriptor, asset.parameters,
                                                      error) != Result::ok)
                return Result::unavailable;
            const auto base = "Cars/Shared/Weapons/" +
                              std::string(weapon_slot_name(descriptor->slot)) + "/" + asset.name +
                              "/";
            if (read_mesh(resources, base + "SpatialObject.msh", asset.weapon, error) != Result::ok)
                return Result::unavailable;
            // 004d2ea1 resets the attached weapon ROOT position/angles, not its muzzle children.
            asset.weapon.position = {};
            if (asset.rocket) {
                if (rocket_bounds(asset.weapon, asset.rocket_shape, error) != Result::ok)
                    return Result::unavailable;
                const std::function<MeshObject*(MeshObject&, Vec3)> flare =
                    [&](MeshObject& node, Vec3 parent) -> MeshObject* {
                    const auto position = add(parent, node.position);
                    if (node.name == "39ULmissFlar") {
                        asset.flare_position = position;
                        return &node;
                    }
                    for (auto& child : node.children)
                        if (auto* found = flare(child, position))
                            return found;
                    return nullptr;
                };
                auto* node = flare(asset.weapon, {});
                if (!node) {
                    error = "Original scene Rocket flare node unavailable";
                    return Result::unavailable;
                }
                asset.flare_mesh = *node;
                asset.flare_mesh.position = {};
                asset.flare_mesh.visible = true;
                node->visible = false;
                asset.ammunition = asset.weapon;
            } else {
                WeaponSalvoRuntime salvo;
                if (salvo.initialize(asset.name, error) != Result::ok)
                    return Result::unavailable;
                const auto muzzle_count = asset.name == "Machine Gun"   ? 2u
                                          : asset.name == "Gatling Gun" ? 5u
                                                                        : 1u;
                for (unsigned muzzle = 0; muzzle < muzzle_count; ++muzzle) {
                    Vec3 local;
                    std::size_t matches{};
                    const auto found = locate_muzzle(
                        asset.weapon, {}, false, salvo.muzzle_name(muzzle), local, matches, error);
                    if (found != Result::ok || matches != 1) {
                        if (error.empty())
                            error = "Original turret muzzle is missing or ambiguous: " + asset.name;
                        return Result::unavailable;
                    }
                    asset.muzzle_local_positions.push_back(local);
                    std::vector<std::size_t> path;
                    const std::function<const MeshObject*(const MeshObject&)> find =
                        [&](const MeshObject& root) -> const MeshObject* {
                        if (root.name == salvo.muzzle_name(muzzle))
                            return &root;
                        for (const auto& child : root.children)
                            if (const auto* found = find(child))
                                return found;
                        return nullptr;
                    };
                    asset.muzzle_meshes.push_back(*find(asset.weapon));
                    asset.muzzle_meshes.back().position = {};
                    asset.muzzle_meshes.back().visible = true;
                }
                if (read_mesh(resources, base + "AmmoSpatialObject.msh", asset.ammunition, error) !=
                    Result::ok)
                    return Result::unavailable;
                asset.ammunition.position = {};
                asset.ammunition.dummy_heading_degrees = 0;
            }
            item.asset = next.assets_.size();
            asset_map.emplace(parsed.weapon, item.asset);
            next.assets_.push_back(std::move(asset));
        }
        const auto& asset = next.assets_[item.asset];
        if (!asset.rocket) {
            if (item.salvo.initialize(asset.name, error) != Result::ok)
                return Result::unavailable;
            item.projectiles.resize(item.salvo.pool_capacity());
        }
        next.items_.push_back(std::move(item));
    }
    next.update_visuals();
    *this = std::move(next);
    return Result::ok;
}
void SceneTurretCombat::reset() {
    turrets_.reset();
    for (auto& item : items_) {
        item.rocket = {};
        for (auto& projectile : item.projectiles)
            projectile = {};
        if (!assets_[item.asset].rocket) {
            std::string error;
            item.salvo.initialize(assets_[item.asset].name, error);
        }
    }
    if (callback_observer_)
        for (const auto token : callback_tokens())
            callback_observer_(token, false);
    callbacks_.clear();
    damage_events_.clear();
    audio_events_.clear();
    launched_ = impacts_ = 0;
    update_visuals();
}
Result SceneTurretCombat::emit_damage(std::size_t turret, const WeaponProjectileHit& hit,
                                      const WeaponProjectile& projectile,
                                      const SceneCombatServices& services, std::string& error) {
    if (hit.kind != ProjectileHitKind::vehicle)
        return Result::ok;
    auto& item = items_[turret];
    const auto& asset = assets_[item.asset];
    SceneCombatDamageEvent event;
    event.turret = turret;
    event.participant = hit.participant;
    event.contact_side = hit.contact_side;
    event.hitpoints = asset.parameters.hitpoints;
    event.point = hit.point;
    event.has_impulse = asset.rocket || item.salvo.applies_impact_impulse();
    if (event.has_impulse) {
        const auto result = asset.rocket
                                ? weapon_rocket_impact_impulse(item.rocket, asset.parameters,
                                                               hit.point, event.impulse)
                                : weapon_projectile_impact_impulse(projectile, asset.parameters,
                                                                   hit.point, event.impulse);
        if (result != Result::ok)
            return result;
    }
    const auto result = services.damage(event);
    if (result != Result::ok) {
        error = "Original scene projectile damage dispatch failed";
        return result;
    }
    damage_events_.push_back(event);
    ++impacts_;
    return Result::ok;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004ba95f
// Turret/weapon callback phase only; other world effects remain separate.
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004d72a9
// Tail-registration and initial-count callback subset, not the complete global scene manager.
Result SceneTurretCombat::advance(float delta, std::span<const TurretTarget> targets,
                                  std::span<const WeaponProjectileTarget> bodies,
                                  const TrackCollision& track, const SceneCombatServices& services,
                                  std::string& error, bool weapons_unlocked) {
    if (!std::isfinite(delta) || delta < 0)
        return Result::invalid_argument;
    if (!services.damage ||
        (requires_trails_ && (!services.trail_count || !services.advance_trail))) {
        error =
            "Scene combat requires original damage dispatch and configured projectile trail pools";
        return Result::unavailable;
    }
    for (const auto& body : bodies) {
        if (body.identity == kSceneIdentity) {
            error = "Vehicle identity collides with portable scene shooter identity";
            return Result::invalid_argument;
        }
        if (!finite(body.position) || !std::isfinite(body.height) || body.height <= 0)
            return Result::invalid_argument;
        for (auto corner : body.footprint)
            if (!finite(corner))
                return Result::invalid_argument;
    }
    damage_events_.clear();
    if (!dispatch_filter_) {
        const auto result = advance_controllers(delta, targets, error, weapons_unlocked);
        if (result != Result::ok)
            return result;
    }
    // The owner-only reference harness; live uses the shared registry's exact cursor dispatch.
    const auto initial_count = callbacks_.size();
    for (std::size_t cursor = 0; cursor < initial_count; ++cursor) {
        const auto callback = callbacks_[cursor]; // Registration can grow/reallocate the vector.
        if (dispatch_filter_ && callback_token(callback) != *dispatch_filter_)
            continue;
        auto& item = items_[callback.turret];
        const auto& asset = assets_[item.asset];
        const auto& pose = turrets_.poses()[callback.turret];
        if (callback.kind == CallbackKind::controller) {
            std::optional<RaceTurretFireEvent> event;
            const auto result = turrets_.advance_one(callback.turret, delta, targets, event, error,
                                                     weapons_unlocked);
            if (result != Result::ok)
                return result;
            if (event) {
                const WeaponFireRequest request{event->firing.origin, event->firing.heading,
                                                event->firing.pitch, event->firing.speed};
                if (asset.rocket) {
                    bool launched{};
                    const auto launch =
                        launch_weapon_rocket(item.rocket, asset.parameters, request, launched);
                    if (launch != Result::ok)
                        return launch;
                    if (launched) {
                        register_callback(CallbackKind::rocket, callback.turret);
                        ++launched_;
                    }
                } else if (item.salvo.begin(request))
                    register_callback(CallbackKind::salvo, callback.turret);
            }
            continue;
        }
        if (callback.kind == CallbackKind::salvo) {
            std::optional<WeaponSalvoShot> shot;
            const auto result = item.salvo.advance(delta, pose.heading, pose.pitch, shot);
            if (result != Result::ok)
                return result;
            if (shot) {
                const auto transform =
                    original_vehicle_transform(pose.world_position, pose.heading, pose.pitch, 0);
                const auto muzzle =
                    point(transform, asset.muzzle_local_positions[shot->muzzle_index]);
                bool launched{};
                const auto launch = launch_weapon_projectile(
                    item.projectiles[shot->pool_index],
                    item.salvo.projectile_spec(asset.parameters), shot->heading, shot->pitch,
                    shot->starting_speed, muzzle, launched);
                if (launch != Result::ok)
                    return launch;
                if (launched) {
                    register_callback(CallbackKind::projectile, callback.turret, shot->pool_index);
                    ++launched_;
                }
            }
            if (item.salvo.ready())
                remove_callback(cursor);
            continue;
        }
        const SceneCombatTrailKey key{callback.turret, callback.projectile, asset.rocket};
        std::size_t trail_count{};
        if (asset.has_trail && services.trail_count(key, trail_count) != Result::ok)
            return Result::unavailable;
        WeaponProjectileHit hit;
        if (asset.rocket) {
            const auto trail = [&](bool emitting, Vec3 position, float dt) {
                const auto& rocket = item.rocket;
                const auto acceleration = asset.parameters.acceleration * dt;
                const Vec3 velocity{rocket.motion.velocity.x + rocket.direction.x * acceleration,
                                    rocket.motion.velocity.y + rocket.direction.y * acceleration,
                                    rocket.motion.velocity.z + rocket.direction.z * acceleration};
                return services.advance_trail(key, dt, emitting, position, velocity);
            };
            const auto result =
                advance_weapon_rocket(item.rocket, asset.parameters, asset.rocket_shape, delta,
                                      trail_count == 0, kSceneIdentity, bodies, track, hit, trail);
            if (result != Result::ok)
                return result;
            if (hit.kind != ProjectileHitKind::none &&
                (!services.impact ||
                 services.impact({key, hit, item.rocket.motion.velocity,
                                  item.rocket.motion.heading_degrees,
                                  item.rocket.motion.pitch_degrees}) != Result::ok)) {
                error = "Original scene rocket impact callback unavailable";
                return Result::unavailable;
            }
            if (hit.kind != ProjectileHitKind::none) {
                std::optional<CombatAudioEvent> audio;
                combat_audio_event(hit.kind == ProjectileHitKind::vehicle
                                       ? CombatAudioCue::projectile_vehicle
                                       : CombatAudioCue::projectile_world,
                                   asset.name, hit.point, audio);
                if (audio)
                    audio_events_.push_back(*audio);
            }
            if (emit_damage(callback.turret, hit, item.rocket.motion, services, error) !=
                Result::ok)
                return Result::unavailable;
            if (!item.rocket.callback_running)
                remove_callback(cursor);
            continue;
        }
        auto& projectile = item.projectiles[callback.projectile];
        const auto spec = item.salvo.projectile_spec(asset.parameters);
        if (advance_weapon_projectile_motion(projectile, spec, delta, trail_count) != Result::ok)
            return Result::invalid_argument;
        if (asset.has_trail) {
            if (services.advance_trail(
                    key, delta, projectile.active && !projectile.stopping && projectile.age <= 5,
                    projectile.position, projectile.velocity) != Result::ok ||
                services.trail_count(key, trail_count) != Result::ok)
                return Result::unavailable;
        }
        hit = weapon_projectile_vehicle_hit(projectile, kSceneIdentity, bodies);
        if (hit.kind == ProjectileHitKind::none)
            hit = weapon_projectile_world_hit(projectile, spec, track);
        if (hit.kind != ProjectileHitKind::none) {
            std::optional<CombatAudioEvent> audio;
            combat_audio_event(hit.kind == ProjectileHitKind::vehicle
                                   ? CombatAudioCue::projectile_vehicle
                                   : CombatAudioCue::projectile_world,
                               asset.name, hit.point, audio);
            if (audio)
                audio_events_.push_back(*audio);
            if (!services.impact ||
                services.impact({key, hit, projectile.velocity, projectile.heading_degrees,
                                 projectile.pitch_degrees}) != Result::ok) {
                error = "Original scene projectile impact callback unavailable";
                return Result::unavailable;
            }
            if (emit_damage(callback.turret, hit, projectile, services, error) != Result::ok)
                return Result::unavailable;
            stop_weapon_projectile(projectile, spec, trail_count);
        }
        if (!projectile.active)
            remove_callback(cursor);
    }
    std::erase_if(callbacks_, [](const Callback& callback) { return !callback.live; });
    update_visuals();
    return Result::ok;
}
void SceneTurretCombat::update_visuals() {
    visuals_.clear();
    for (std::size_t turret = 0; turret < items_.size(); ++turret) {
        const auto& item = items_[turret];
        const auto& asset = assets_[item.asset];
        if (asset.rocket) {
            const auto& p = item.rocket.motion;
            visuals_.push_back(
                {turret, item.asset, 0,
                 original_vehicle_transform(p.position, p.heading_degrees, p.pitch_degrees, 0),
                 p.active, true});
            const auto parent =
                original_vehicle_transform(p.position, p.heading_degrees, p.pitch_degrees, 0);
            auto flare =
                original_vehicle_transform(point(parent, asset.flare_position), p.heading_degrees,
                                           p.pitch_degrees, item.rocket.flare_roll);
            visuals_.push_back(
                {turret, item.asset, 0, flare, p.active, true, false, 0xffffffff, true});
        } else {
            for (std::size_t index = 0; index < item.projectiles.size(); ++index) {
                const auto& p = item.projectiles[index];
                visuals_.push_back(
                    {turret, item.asset, index,
                     original_vehicle_transform(p.position, p.heading_degrees, p.pitch_degrees, 0),
                     p.active && !asset.has_trail, false});
            }
            const auto& owner = turrets_.poses()[turret];
            const auto mounted =
                original_vehicle_transform(owner.world_position, owner.heading, owner.pitch, 0);
            for (std::size_t m = 0; m < item.salvo.muzzle_flashes().size(); ++m) {
                const auto& flash = item.salvo.muzzle_flashes()[m];
                auto pose =
                    original_vehicle_transform(point(mounted, asset.muzzle_local_positions[m]),
                                               owner.heading, owner.pitch, flash.local_roll);
                for (auto& basis : pose.basis) {
                    basis.x *= flash.scale;
                    basis.y *= flash.scale;
                    basis.z *= flash.scale;
                }
                visuals_.push_back(
                    {turret, item.asset, m, pose, flash.visible, false, true, flash.color_argb});
            }
        }
    }
}
std::vector<CombatAudioEvent> SceneTurretCombat::drain_audio_events() {
    std::vector<CombatAudioEvent> output;
    output.swap(audio_events_);
    return output;
}
} // namespace roadwars
