#include "game_src/unresolved/weapon_runtime.hpp"
#include <cmath>
#include <limits>

namespace roadwars {
namespace {
constexpr std::array<WeaponDescriptor, 21> kWeapons{{
    {"Smoke Screen", WeaponSlot::rear, 0x004a6110, 0x004a61ec, 0x005f5960},
    {"Oil Slick", WeaponSlot::rear, 0x00474270, 0x004743bb, 0x005f57ac},
    {"Missile Jammer", WeaponSlot::rear, 0x00464f50, 0x00465038, 0x005f573c},
    {"Grenades", WeaponSlot::rear, 0x0044db70, 0x0044dc6a, 0x005f5534},
    {"Reactive Mines", WeaponSlot::rear, 0x0049ce70, 0x0049cf6a, 0x005f5894},
    {"Arachnoids", WeaponSlot::rear, 0x00406320, 0x0040641a, 0x005f5024},
    {"Land Mines", WeaponSlot::rear, 0x004563d0, 0x004564ca, 0x005f5660},
    {"Machine Gun", WeaponSlot::front, 0x0045ae10, 0x0045af2e, 0x005f56d0},
    {"20mm Cannon", WeaponSlot::front, 0x0040c300, 0x0040c41e, 0x005f5178},
    {"Electron Cannon", WeaponSlot::front, 0x00433710, 0x00433867, 0x005f53b4},
    {"Plasma Blaster", WeaponSlot::front, 0x0047d000, 0x0047d11e, 0x005f582c},
    {"Battering Spikes", WeaponSlot::front, 0x00407ec0, 0x00407f9c, 0x005f50b8},
    {"Hydraulic Ram", WeaponSlot::front, 0x00455bc0, 0x00407f9c, 0x005f560c},
    {"Vulcanized Gun", WeaponSlot::front, 0x004ce560, 0x004ce67e, 0x005f59e0},
    {"Flame Thrower", WeaponSlot::front, 0x004352f0, 0x004353f0, 0x005f5460},
    {"Rocket Launcher", WeaponSlot::side, 0x0049f330, 0x0049f460, 0x005f590c},
    {"Gatling Gun", WeaponSlot::side, 0x0044c720, 0x0044c83e, 0x005f54d8},
    {"Guided Missile", WeaponSlot::side, 0x0044e820, 0x0044e926, 0x005f5594},
    {"Back Missile", WeaponSlot::side, 0x0044e820, 0x0044e926, 0x005f5594},
    {"Swarmers", WeaponSlot::side, 0x0044e820, 0x0044e926, 0x005f5594},
    {"The Excalibur", WeaponSlot::side, 0x00434b90, 0x0044e926, 0x005f5408},
}};
bool equal(std::string_view a, std::string_view b) {
    if (a.size() != b.size())
        return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        const auto lower = [](char c) { return 'A' <= c && c <= 'Z' ? c + ('a' - 'A') : c; };
        if (lower(a[i]) != lower(b[i]))
            return false;
    }
    return true;
}
} // namespace
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00410ea2
// Factory identity coverage only, not full allocation/attachment/gameplay restoration.
std::span<const WeaponDescriptor> weapon_descriptors() { return kWeapons; }
const WeaponDescriptor* find_weapon_descriptor(std::string_view name) {
    for (const auto& item : kWeapons)
        if (equal(name, item.name))
            return &item;
    return nullptr;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004cf8d4
bool weapon_is_guided(std::string_view name) {
    return equal(name, "Guided Missile") || equal(name, "Swarmers") ||
           equal(name, "The Excalibur") || equal(name, "Back Missile");
}
std::string_view weapon_slot_name(WeaponSlot slot) {
    switch (slot) {
    case WeaponSlot::front:
        return "Front";
    case WeaponSlot::side:
        return "Side";
    case WeaponSlot::rear:
        return "Rear";
    }
    return {};
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004cf29a
// Configuration subset; native attachment, localization and callback registration stay separate.
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004d0778
// Default ammunition read only; caller owns native-equivalent mount selection.
Result load_weapon_parameters(ResourceStore& resources, const WeaponDescriptor& descriptor,
                              WeaponParameters& output, std::string& error) {
    const auto* known = find_weapon_descriptor(descriptor.name);
    if (!known || known->slot != descriptor.slot) {
        error = "Unknown original weapon identity or slot";
        return Result::invalid_argument;
    }
    Configuration config;
    const auto path = "Cars/Shared/Weapons/" + std::string(weapon_slot_name(descriptor.slot)) +
                      "/" + std::string(known->name) + "/Data.ini";
    if (config.load(resources, path) != Result::ok) {
        error = "Original weapon configuration unavailable: " + path;
        return Result::unavailable;
    }
    WeaponParameters parsed;
    const std::array<std::pair<std::string_view, float*>, 7> physics{{
        {"AMMO_COUNT", &parsed.ammunition_count},
        {"AMMO_TOP_SPEED", &parsed.top_speed},
        {"AMMO_ACC", &parsed.acceleration},
        {"AMMO_IMPACT_MULTIPLIER", &parsed.impact_multiplier},
        {"AMMO_YROTATIONAL_SPEED", &parsed.rotational_speed},
        {"AMMO_MASS_IN_POUNDS", &parsed.ammunition_mass_kg},
        {"AMMO_TRACKING_TIMER_SECONDS", &parsed.tracking_seconds},
    }};
    const auto read = [&](std::string_view section, std::string_view key, float& value) {
        return config.number(section, key, value) == Result::ok && std::isfinite(value);
    };
    float ammunition{};
    if (!read("DAMAGE", "HITPOINTS", parsed.hitpoints) ||
        !read("PRICE", "DEFAULTAMMO", ammunition) || ammunition < 0 ||
        static_cast<double>(ammunition) > std::numeric_limits<std::int32_t>::max() ||
        std::floor(ammunition) != ammunition) {
        error = "Malformed original weapon damage or ammunition: " + path;
        return Result::invalid_argument;
    }
    for (auto [key, value] : physics) {
        if (!read("PHYSICS", key, *value)) {
            error = "Malformed original weapon physics " + std::string(key) + ": " + path;
            return Result::invalid_argument;
        }
    }
    parsed.default_ammunition = static_cast<std::int32_t>(ammunition);
    // 005f51f8 is the original float pounds-to-kilograms factor, not rounded 0.45.
    parsed.ammunition_mass_kg *= 0.4535999894142151f;
    output = parsed;
    return Result::ok;
}
Result WeaponMountRuntime::initialize(WeaponSlot slot, std::int32_t ammunition) {
    if (weapon_slot_name(slot).empty() || ammunition < 0)
        return Result::invalid_argument;
    slot_ = slot;
    ammunition_ = ammunition;
    deployment_roll_ = launch_speed_ = 0;
    pending_ = deployed_ = false;
    visible_ = slot != WeaponSlot::side;
    initialized_ = true;
    return Result::ok;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004cfe98
// Recovered reservation/state gates; attachment placement and replay record are caller-owned.
bool WeaponMountRuntime::request(float speed, bool ready) {
    if (!initialized_ || !std::isfinite(speed) || pending_ || ammunition_ == 0 || !ready)
        return false;
    launch_speed_ = speed;
    pending_ = true;
    visible_ = true;
    --ammunition_;
    return true;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004d009d
// Pending state only; native weapon child detachment is handled by its presentation owner.
void WeaponMountRuntime::cancel() { pending_ = false; }
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004d03d6
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004d0574
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004d060d
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004d0725
// Portable dispatch/state and emitted firing arguments; native callbacks/transforms are separate.
Result WeaponMountRuntime::advance(float dt, bool idle, float heading, float pitch, Vec3 origin,
                                   std::optional<WeaponFireRequest>& fired) {
    fired.reset();
    if (!initialized_)
        return Result::unavailable;
    if (!std::isfinite(dt) || dt < 0 || !std::isfinite(heading) || !std::isfinite(pitch) ||
        !std::isfinite(origin.x) || !std::isfinite(origin.y) || !std::isfinite(origin.z))
        return Result::invalid_argument;
    if (slot_ == WeaponSlot::side) {
        if (!pending_) {
            if (idle) {
                deployed_ = false;
                deployment_roll_ += 180 * dt;
                if (0 <= deployment_roll_) {
                    deployment_roll_ = 0;
                    visible_ = false;
                }
            }
        } else if (!deployed_) {
            deployment_roll_ -= 180 * dt;
            if (deployment_roll_ <= -90) {
                deployment_roll_ = -90;
                deployed_ = true;
            }
        } else {
            fired = WeaponFireRequest{origin, heading, pitch, launch_speed_};
            cancel();
        }
    } else if (pending_) {
        fired = WeaponFireRequest{origin, heading + (slot_ == WeaponSlot::rear ? 180 : 0), pitch,
                                  launch_speed_};
        cancel();
    }
    return Result::ok;
}
} // namespace roadwars
