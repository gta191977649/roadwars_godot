#include "game_src/unresolved/vehicle_equipment_effects.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace roadwars {
namespace {
constexpr std::array<std::string_view, 7> kDriveCategories{
    "AIR FILTER",      "CAM SHAFT",     "CYLINDER HEADS", "EXHAUST",
    "INTAKE MANIFOLD", "SUPER CHARGER", "NITROUS BOOST"};
constexpr std::array<std::int32_t, 7> kPoorUpgrade{2, 6, 10, 5, 4, 15, 0};
std::string canonical(std::string_view value) { return ResourceStore::canonical(value); }
bool valid_hp(float value) { return std::isfinite(value) && value > 0; }
bool valid_damage(float value) { return std::isfinite(value) && value >= 0; }
bool blocked(const VehicleDamagePermission& permission) {
    return permission.route_invulnerable || permission.dying;
}
} // namespace
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0040fd85
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00410232
Result drivetrain_upgrade(std::string_view category, std::string_view quality,
                          std::int32_t& output) {
    const auto name = canonical(category), level = canonical(quality);
    const auto found = std::find(kDriveCategories.begin(), kDriveCategories.end(), name);
    if (found == kDriveCategories.end() || (level != "POOR" && level != "GOOD" && level != "BEST"))
        return Result::invalid_argument;
    const auto index = static_cast<std::size_t>(found - kDriveCategories.begin());
    const float factor = level == "BEST" ? 2.0f : level == "GOOD" ? 1.5f : 1.0f;
    // Assembly 00410478/004104a9: x87 FILD, multiply, __ftol truncation.
    output = static_cast<std::int32_t>(static_cast<float>(kPoorUpgrade[index]) * factor);
    return Result::ok;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00410853
Result rescale_vehicle_armor(VehicleEquipmentEffects& state, float hp_scale) {
    if (!std::isfinite(hp_scale))
        return Result::invalid_argument;
    auto next = state;
    for (std::size_t i = 0; i < state.armor_hp.size(); ++i) {
        if (!valid_hp(state.base_armor_hp[i]) || !valid_hp(state.maximum_armor_hp[i]) ||
            !std::isfinite(state.armor_hp[i]))
            return Result::invalid_argument;
        // Assembly stores the current/maximum ratio to float before the multiplication.
        const float ratio = state.armor_hp[i] / state.maximum_armor_hp[i];
        next.maximum_armor_hp[i] = hp_scale * state.base_armor_hp[i] + state.base_armor_hp[i];
        next.armor_hp[i] = ratio * next.maximum_armor_hp[i];
        if (!valid_hp(next.maximum_armor_hp[i]) || !std::isfinite(next.armor_hp[i]))
            return Result::invalid_argument;
    }
    state = std::move(next);
    return Result::ok;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0040d4cb
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0040d2b0
Result load_vehicle_equipment_effects(
    ResourceStore& resources, std::string_view character, std::string_view vehicle,
    std::string_view armor_type, std::string_view armor_quality,
    std::span<const std::pair<std::string, std::string>> drivetrain, std::string_view front_weapon,
    VehicleEquipmentEffects& output, std::string& error) {
    VehicleEquipmentEffects next;
    Configuration config;
    auto number = [&](std::string_view section, std::string_view key, float& value) {
        if (config.number(section, key, value) == Result::ok && valid_hp(value))
            return true;
        error = "Missing or invalid equipment configuration: " + std::string(section) + "/" +
                std::string(key);
        return false;
    };
    if (config.load(resources, "Cars/" + std::string(vehicle) + "/Data/Data.ini") != Result::ok) {
        error = "Vehicle damage configuration unavailable";
        return Result::unavailable;
    }
    constexpr std::array<std::string_view, 4> kArmorKeys{"FRONTHP", "RIGHTHP", "REARHP", "LEFTHP"};
    for (std::size_t i = 0; i < kArmorKeys.size(); ++i) {
        if (!number("DAMAGE", kArmorKeys[i], next.base_armor_hp[i]))
            return Result::invalid_argument;
        next.maximum_armor_hp[i] = next.armor_hp[i] = next.base_armor_hp[i];
    }
    if (!number("DAMAGE", "INTERNALHP", next.internal_hp))
        return Result::invalid_argument;
    next.maximum_internal_hp = next.internal_hp;
    if (config.load(resources, "Characters/" + std::string(character) + "/Data.ini") !=
            Result::ok ||
        !number("PRICE_SCALARS", "NITRO", next.nitrous_duration_scalar))
        return Result::unavailable;
    if (config.number("PRICE_SCALARS", "INTERNAL_REPAIR_PER_SEC",
                      next.internal_repair_per_second) != Result::ok) {
        error = "Character internal repair rate is unavailable";
        return Result::unavailable;
    }
    if (config.number("PRICE_SCALARS", "TRACTION", next.traction_multiplier) != Result::ok ||
        !std::isfinite(next.traction_multiplier) || next.traction_multiplier < 0) {
        error = "Character original TRACTION multiplier unavailable";
        return Result::unavailable;
    }
    // 0040d4cb installs NONE/NONE armor for a fresh garage vehicle.
    if (!armor_type.empty() && canonical(armor_type) != "NONE") {
        if (config.load(resources, "Cars/Shared/Armor/" + std::string(armor_type) + "/" +
                                       std::string(armor_quality) + "/Data.ini") != Result::ok) {
            error = "Selected armor HP configuration unavailable";
            return Result::unavailable;
        }
        float armor_scale{};
        if (config.number("HP", "HPSCALE", armor_scale) != Result::ok ||
            rescale_vehicle_armor(next, armor_scale) != Result::ok) {
            error = "Selected armor HP scale is invalid";
            return Result::invalid_argument;
        }
    }
    for (const auto& [category, quality] : drivetrain) {
        std::int32_t bonus{};
        if (drivetrain_upgrade(category, quality, bonus) != Result::ok) {
            error = "Unsupported original drivetrain category or quality";
            return Result::invalid_argument;
        }
        const auto name = canonical(category);
        const auto index = static_cast<std::size_t>(
            std::find(kDriveCategories.begin(), kDriveCategories.end(), name) -
            kDriveCategories.begin());
        next.engine_upgrades[index] = bonus;
        if (index != 6)
            continue;
        if (config.load(resources, "Cars/Shared/Drive Train/Nitrous Boost/" + quality +
                                       "/Data.ini") != Result::ok) {
            error = "Selected nitrous ammunition configuration unavailable";
            return Result::unavailable;
        }
        float ammo{};
        if (config.number("PRICE", "DEFAULTAMMO", ammo) != Result::ok || !std::isfinite(ammo) ||
            ammo < 0 || ammo > static_cast<double>(std::numeric_limits<std::int32_t>::max()) ||
            std::floor(ammo) != ammo) {
            error = "Selected nitrous ammunition is invalid";
            return Result::invalid_argument;
        }
        next.nitrous_ammo = static_cast<std::int32_t>(ammo);
    }
    next.front_weapon = std::string(front_weapon);
    output = std::move(next);
    return Result::ok;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00414d2a
Result damage_vehicle_internal(VehicleEquipmentEffects& state, float damage,
                               const VehicleDamagePermission& permission) {
    if (!valid_damage(damage) || !std::isfinite(state.internal_hp) || state.internal_hp < 0)
        return Result::invalid_argument;
    if (!blocked(permission) && permission.local_authority && state.internal_hp != 0)
        state.internal_hp = std::max(state.internal_hp - damage, 0.0f);
    return Result::ok;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00414e7d
Result damage_vehicle_armor(VehicleEquipmentEffects& state, std::size_t index, float damage,
                            const VehicleDamagePermission& permission) {
    if (index >= state.armor_hp.size() || !valid_damage(damage) ||
        !std::isfinite(state.armor_hp[index]))
        return Result::invalid_argument;
    if (blocked(permission))
        return Result::ok;
    if (index == 0) {
        const auto front = canonical(state.front_weapon);
        if (front == "BATTERING SPIKES")
            damage *= .5f;
        else if (front == "HYDRAULIC RAM")
            damage *= .25f;
    }
    state.armor_hp[index] -= damage; // Original armor is deliberately not clamped to zero.
    return Result::ok;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0045b9b0
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0040cbc4
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0047d8c4
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004cee24
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0044d26d
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0049ff95
Result damage_vehicle_contact(VehicleEquipmentEffects& state, std::size_t contact_side,
                              float damage, const VehicleDamagePermission& permission) {
    constexpr std::array<std::size_t, 4> kArmorIndex{0, 2, 3, 1};
    if (contact_side >= kArmorIndex.size())
        return Result::invalid_argument;
    const auto index = kArmorIndex[contact_side];
    return state.armor_hp[index] <= 0 ? damage_vehicle_internal(state, damage, permission)
                                      : damage_vehicle_armor(state, index, damage, permission);
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00473e8e
bool activate_vehicle_nitrous(VehicleEquipmentEffects& state) {
    if (state.nitrous_timer != 0 || state.nitrous_ammo <= 0)
        return false;
    state.nitrous_timer = .01f;
    --state.nitrous_ammo;
    return true;
}
Result advance_vehicle_nitrous(VehicleEquipmentEffects& state, float delta, Vec3 vehicle_forward,
                               std::size_t live_particles, NitrousStep& output) {
    const auto emitter = [live_particles](const NitrousStep&, std::size_t& count) {
        count = live_particles;
        return Result::ok;
    };
    return advance_vehicle_nitrous(state, delta, vehicle_forward, emitter, output);
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00473fa6
Result advance_vehicle_nitrous(
    VehicleEquipmentEffects& state, float delta, Vec3 vehicle_forward,
    const std::function<Result(const NitrousStep&, std::size_t&)>& advance_emitter,
    NitrousStep& output) {
    if (!std::isfinite(delta) || delta < 0 || !std::isfinite(state.nitrous_timer) ||
        state.nitrous_timer < 0 || !valid_hp(state.nitrous_duration_scalar))
        return Result::invalid_argument;
    output = {};
    if (state.nitrous_timer == 0)
        return Result::ok;
    if (!advance_emitter)
        return Result::unavailable;
    const float modulus =
        std::sqrt(vehicle_forward.x * vehicle_forward.x + vehicle_forward.y * vehicle_forward.y +
                  vehicle_forward.z * vehicle_forward.z);
    if (!valid_hp(modulus))
        return Result::invalid_argument;
    const float duration = 3.0f * state.nitrous_duration_scalar;
    output.emitting = state.nitrous_timer < duration;
    if (output.emitting) {
        const float scalar = 12.0f * delta;
        // Normalize before scaling, matching the original two D3DRM calls.
        output.velocity_delta = {(vehicle_forward.x / modulus) * scalar,
                                 (vehicle_forward.y / modulus) * scalar,
                                 (vehicle_forward.z / modulus) * scalar};
    }
    std::size_t live_particles{};
    const auto result = advance_emitter(output, live_particles);
    if (result != Result::ok)
        return result;
    state.nitrous_timer += delta;
    if (duration < state.nitrous_timer && live_particles == 0) {
        stop_vehicle_nitrous(state);
        output.stopped = true;
    }
    return Result::ok;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00473f5d
void stop_vehicle_nitrous(VehicleEquipmentEffects& state) { state.nitrous_timer = 0; }
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00480b40
// Per-participant health/disabled-timer phase only; complete results/effects remain separate.
Result advance_vehicle_health(VehicleEquipmentEffects& state, float delta) {
    if (!std::isfinite(delta) || delta < 0 || !std::isfinite(state.internal_hp) ||
        !valid_hp(state.maximum_internal_hp) || !std::isfinite(state.internal_repair_per_second) ||
        !std::isfinite(state.dying_timer))
        return Result::invalid_argument;
    if (state.internal_repair_per_second != 0 && state.internal_hp > 0)
        state.internal_hp = std::min(state.internal_hp + delta * state.internal_repair_per_second,
                                     state.maximum_internal_hp);
    state.dying_timer = state.internal_hp > 0 ? 0.0f : state.dying_timer + delta;
    return Result::ok;
}
} // namespace roadwars
