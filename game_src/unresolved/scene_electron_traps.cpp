#include "game_src/unresolved/scene_electron_traps.hpp"
#include "game_src/unresolved/contact_orientation.hpp"
#include <algorithm>
#include <cmath>
#include <map>

namespace roadwars {
namespace {
Vec3 add(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
float distance(Vec3 a, Vec3 b) {
    const float x = a.x - b.x, y = a.y - b.y, z = a.z - b.z;
    return std::sqrt(x * x + y * y + z * z);
}
struct Node {
    const MeshObject* mesh{};
    Vec3 position{};
    bool transformed{};
};
} // namespace
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004b7439
// Non-factory scene Electron weapon construction/attachment subset.
Result SceneElectronTraps::load(ResourceStore& resources, const MeshObject& world,
                                std::string& error) {
    SceneElectronTraps next;
    std::vector<Node> nodes;
    const std::function<void(const MeshObject&, Vec3, bool)> visit =
        [&](const MeshObject& mesh, Vec3 parent, bool transformed) {
            const auto position = add(parent, mesh.position);
            nodes.push_back({&mesh, position, transformed});
            for (const auto& child : mesh.children)
                visit(child, position,
                      transformed || mesh.angles_degrees.x != 0 || mesh.angles_degrees.y != 0 ||
                          mesh.angles_degrees.z != 0 || mesh.name.starts_with("RotObject"));
        };
    visit(world, {}, false);
    std::map<unsigned, bool> seen;
    for (const auto& node : nodes) {
        const auto& name = node.mesh->name;
        if (!name.starts_with("Electron"))
            continue;
        if (name.size() < 11 || name[8] < '1' || name[8] > '9' || name[9] != '_' ||
            (name[10] != '1' && name[10] != '2') || node.transformed || node.mesh->type != 4) {
            error = "Original scene Electron endpoint format/ancestry unavailable: " + name;
            return Result::unavailable;
        }
        const unsigned id = static_cast<unsigned>(name[8] - '0');
        if (seen[id])
            continue;
        // Native array uses id-1 while constructing in discovered order. Reject ambiguous layouts.
        if (id != next.items_.size() + 1) {
            error = "Original scene Electron endpoint order is ambiguous";
            return Result::unavailable;
        }
        seen[id] = true;
        const auto prefix = name.substr(0, 10);
        const auto first = std::find_if(nodes.begin(), nodes.end(), [&](const Node& n) {
            return n.mesh->name == prefix + "1";
        });
        const auto second = std::find_if(nodes.begin(), nodes.end(), [&](const Node& n) {
            return n.mesh->name == prefix + "2";
        });
        if (first == nodes.end() || second == nodes.end() || first->transformed ||
            second->transformed) {
            error = "Original scene Electron paired endpoints unavailable";
            return Result::unavailable;
        }
        Item item;
        const std::string base = "Cars/Shared/Weapons/Front/Electron Cannon/";
        if (item.mesh.load(resources, base + "Lightning.ska", base + "LightningAnim.ska", error) !=
            Result::ok)
            return Result::unavailable;
        item.state.range_6d0 = distance(first->position, second->position) * 1.2999999523162842f;
        item.state.width_6d4 = item.state.height_6d8 = 8;
        next.poses_.push_back({name,
                               node.position,
                               node.mesh->dummy_heading_degrees,
                               item.state.range_6d0,
                               id % 2 == 0 ? 1.5f : 0,
                               false,
                               false,
                               {}});
        next.items_.push_back(std::move(item));
    }
    *this = std::move(next);
    return Result::ok;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004ba95f
// Direct per-track inactive-timer activation subset; no race weapon-lock gate applies here.
Result SceneElectronTraps::advance_world(float dt, std::string& error) {
    if (!std::isfinite(dt) || dt < 0)
        return Result::invalid_argument;
    for (std::size_t i = 0; i < items_.size(); ++i) {
        auto& item = items_[i];
        auto& pose = poses_[i];
        if (item.state.active_270)
            continue;
        pose.timer -= dt;
        if (pose.timer < 0) {
            pose.timer = 1.5f;
            if (electron_cannon_can_activate(item.state)) {
                begin_electron_cannon(item.state);
                std::optional<CombatAudioEvent> audio;
                combat_audio_event(CombatAudioCue::accepted_fire, "Electron Cannon", pose.origin,
                                   audio);
                if (audio)
                    audio_events_.push_back(*audio);
                item.registered = true;
                if (observer_)
                    observer_(i + 1, true);
            }
        }
        pose.active = item.state.active_270;
    }
    error.clear();
    return Result::ok;
}
void SceneElectronTraps::observe_callbacks(std::function<void(std::uint64_t, bool)> observer) {
    observer_ = std::move(observer);
    if (observer_)
        for (const auto token : callback_tokens())
            observer_(token, true);
}
std::vector<std::uint64_t> SceneElectronTraps::callback_tokens() const {
    std::vector<std::uint64_t> tokens;
    for (std::size_t i = 0; i < items_.size(); ++i)
        if (items_[i].registered)
            tokens.push_back(i + 1);
    return tokens;
}
const SkeletalMeshRuntime* SceneElectronTraps::geometry(std::size_t i) const {
    return i < items_.size() ? &items_[i].mesh : nullptr;
}
Result SceneElectronTraps::dispatch_callback(std::uint64_t token, float dt,
                                             std::span<const VehicleCombatTarget> targets,
                                             const VehicleCombatServices& services,
                                             std::string& error) {
    if (token == 0 || token > items_.size() || !items_[token - 1].registered)
        return Result::invalid_argument;
    auto& item = items_[token - 1];
    auto& pose = poses_[token - 1];
    const VehicleCombatEffectKey key{WeaponSlot::front, static_cast<std::size_t>(token - 1),
                                     VehicleCombatEffectKind::electron_impact};
    std::size_t particles{};
    if (!services.effect_count || !services.effect_step || !services.damage ||
        services.effect_count(key, particles) != Result::ok) {
        error = "Original scene Electron impact/damage services unavailable";
        return Result::unavailable;
    }
    if (item.state.active_270 && item.mesh.advance(dt) != Result::ok)
        return Result::unavailable;
    std::vector<SpecialWeaponTarget> bodies;
    for (const auto& target : targets)
        bodies.push_back(
            {target.participant, static_cast<std::uint64_t>(target.identity), target.footprint});
    ElectronCannonOutput output;
    const auto result = advance_electron_cannon(item.state, UINT64_MAX, pose.origin, pose.heading,
                                                200, bodies, particles, dt, output, error);
    if (result != Result::ok)
        return result;
    Vec3 velocity{};
    for (const auto& damage : output.damage) {
        if (services.damage({WeaponSlot::front,
                             damage.participant,
                             damage.contact_side,
                             damage.damage,
                             {},
                             false,
                             false,
                             false,
                             {}}) != Result::ok)
            return Result::unavailable;
        for (const auto& target : targets)
            if (target.participant == damage.participant)
                velocity = target.velocity;
    }
    if (services.effect_step({key, dt, output.hit_point.has_value(),
                              output.hit_point.value_or(pose.origin), velocity, pose.heading, 0}) !=
        Result::ok)
        return Result::unavailable;
    pose.active = item.state.active_270;
    if (output.play_hit_sound) {
        std::optional<CombatAudioEvent> audio;
        combat_audio_event(CombatAudioCue::electron_contact, "Electron Cannon",
                           output.hit_point.value_or(pose.origin), audio);
        if (audio)
            audio_events_.push_back(*audio);
    }
    pose.beam_visible = item.state.beam_attached;
    pose.beam_pose =
        original_skeletal_transform(pose.origin, pose.heading, 0, 0, {8, 8, output.beam_length});
    if (output.remove_callback) {
        item.registered = false;
        if (observer_)
            observer_(token, false);
    }
    return Result::ok;
}
std::vector<CombatAudioEvent> SceneElectronTraps::drain_audio_events() {
    std::vector<CombatAudioEvent> output;
    output.swap(audio_events_);
    return output;
}
} // namespace roadwars
