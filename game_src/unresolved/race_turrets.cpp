#include "game_src/unresolved/race_turrets.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>

namespace roadwars {
namespace {
Vec3 add(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
bool finite(Vec3 a) { return std::isfinite(a.x) && std::isfinite(a.y) && std::isfinite(a.z); }
} // namespace
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0040d2b0
// Only the character's original turret exemption field at vehicle+4410 is loaded here.
Result load_turret_target_exemption(ResourceStore& resources, std::string_view character,
                                    bool& output, std::string& error) {
    Configuration config;
    float value{};
    if (config.load(resources, "Characters/" + std::string(character) + "/Data.ini") !=
            Result::ok ||
        config.number("PRICE_SCALARS", "NOT_A_TURRET_TARGET", value) != Result::ok ||
        std::floor(value) != value) {
        error = "Original character turret exemption unavailable: " + std::string(character);
        return Result::unavailable;
    }
    output = value != 0;
    return Result::ok;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004b7439
// Only its GUNS setting and Turret discovery subset. Other track effects are separate.
Result RaceTurrets::load(ResourceStore& resources, std::string_view track, const MeshObject& world,
                         std::string& error) {
    Configuration config;
    float guns{};
    if (config.load(resources, "Tracks/" + std::string(track) + "/World/Data.txt") != Result::ok ||
        config.number("DATA", "GUNS", guns) != Result::ok || std::floor(guns) != guns) {
        error = "Original track GUNS configuration unavailable";
        return Result::unavailable;
    }
    std::vector<Item> items;
    std::vector<RaceTurretPose> poses;
    std::vector<std::size_t> path;
    const std::function<Result(const MeshObject&, Vec3, bool)> visit =
        [&](const MeshObject& node, Vec3 parent, bool transformed_ancestor) -> Result {
        if (!finite(node.position)) {
            error = "Nonfinite track object position";
            return Result::invalid_argument;
        }
        const auto position = add(parent, node.position);
        const bool is_turret = node.name.starts_with("Turret");
        if (is_turret) {
            if (transformed_ancestor) {
                error = "Original turret transformed ancestry is unavailable: " + node.name;
                return Result::unavailable;
            }
            if (items.size() >= 1000) {
                error = "Original turret search capacity exceeded";
                return Result::invalid_argument;
            }
            TurretParameters settings;
            auto result = parse_turret_parameters(node.name, settings);
            if (result != Result::ok) {
                error = "Malformed original turret name: " + node.name;
                return result;
            }
            const auto* descriptor = find_weapon_descriptor(settings.weapon);
            Item item;
            item.descriptor = descriptor;
            if (!descriptor || load_weapon_parameters(resources, *descriptor, item.parameters,
                                                      error) != Result::ok)
                return Result::unavailable;
            settings.ammunition_acceleration = item.parameters.acceleration;
            // 004d414f bounds only this object's own vertices; it does not traverse children.
            float low = std::numeric_limits<float>::infinity();
            float high = -std::numeric_limits<float>::infinity();
            for (const auto& part : node.parts)
                for (auto vertex : part.positions) {
                    if (!finite(vertex)) {
                        error = "Nonfinite original turret geometry: " + node.name;
                        return Result::invalid_argument;
                    }
                    low = std::min(low, vertex.z);
                    high = std::max(high, vertex.z);
                }
            if (!std::isfinite(low) || !std::isfinite(high)) {
                error = "Original turret barrel bounds are unavailable: " + node.name;
                return Result::unavailable;
            }
            settings.barrel_length = high - low;
            if (item.runtime.initialize(std::move(settings), position) != Result::ok) {
                error = "Original turret parameters are invalid: " + node.name;
                return Result::invalid_argument;
            }
            poses.push_back({node.name, path, node.position, position, item.runtime.heading(),
                             item.runtime.pitch(), item.runtime.target()});
            items.push_back(std::move(item));
        }
        // Serialized dummy basis/heading is metadata, not a runtime Euler rotation004d9aca.
        // Actual dynamic parent rotations still require their native update phase.
        const bool transformed = transformed_ancestor || node.angles_degrees.x != 0 ||
                                 node.angles_degrees.y != 0 || node.angles_degrees.z != 0 ||
                                 is_turret || node.name.starts_with("RotObject");
        for (std::size_t i = 0; i < node.children.size(); ++i) {
            path.push_back(i);
            const auto result = visit(node.children[i], position, transformed);
            path.pop_back();
            if (result != Result::ok)
                return result;
        }
        return Result::ok;
    };
    const auto result = visit(world, {}, false);
    if (result != Result::ok)
        return result;
    items_ = std::move(items);
    poses_ = std::move(poses);
    guns_enabled_ = guns != 0;
    return Result::ok;
}
Result RaceTurrets::advance(float dt, std::span<const TurretTarget> targets,
                            std::vector<RaceTurretFireEvent>& events, std::string& error,
                            bool weapons_unlocked) {
    events.clear();
    if (!std::isfinite(dt) || dt < 0) {
        error = "Invalid turret simulation delta";
        return Result::invalid_argument;
    }
    for (std::size_t i = 0; i < items_.size(); ++i) {
        auto& item = items_[i];
        std::optional<TurretFireEvent> fire;
        const auto result =
            item.runtime.advance(dt, targets, guns_enabled_ && weapons_unlocked, fire);
        if (result != Result::ok) {
            events.clear();
            error = "Original turret update failed: " + poses_[i].name;
            return result;
        }
        poses_[i].heading = item.runtime.heading();
        poses_[i].pitch = item.runtime.pitch();
        poses_[i].target = item.runtime.target();
        if (fire)
            events.push_back({i, item.descriptor, item.parameters, *fire});
    }
    return Result::ok;
}
Result RaceTurrets::advance_one(std::size_t index, float dt, std::span<const TurretTarget> targets,
                                std::optional<RaceTurretFireEvent>& event, std::string& error,
                                bool unlocked) {
    event.reset();
    if (index >= items_.size())
        return Result::invalid_argument;
    auto& item = items_[index];
    std::optional<TurretFireEvent> fire;
    const auto result = item.runtime.advance(dt, targets, guns_enabled_ && unlocked, fire);
    if (result != Result::ok) {
        error = "Original individual turret callback failed";
        return result;
    }
    poses_[index].heading = item.runtime.heading();
    poses_[index].pitch = item.runtime.pitch();
    poses_[index].target = item.runtime.target();
    if (fire)
        event = RaceTurretFireEvent{index, item.descriptor, item.parameters, *fire};
    return Result::ok;
}
void RaceTurrets::reset() {
    for (std::size_t i = 0; i < items_.size(); ++i) {
        items_[i].runtime.reset();
        poses_[i].heading = items_[i].runtime.heading();
        poses_[i].pitch = items_[i].runtime.pitch();
        poses_[i].target = items_[i].runtime.target();
    }
}
} // namespace roadwars
