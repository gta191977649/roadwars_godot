#include "game_src/unresolved/contact_weapon_runtime.hpp"
#include <cmath>

namespace roadwars {
namespace {
bool finite(Vec3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
Vec3 add_strength(Vec3 force) {
    const float length = std::sqrt(force.x * force.x + force.y * force.y + force.z * force.z);
    if (length != 0) {
        const Vec3 unit{force.x / length, force.y / length, force.z / length};
        force.x += unit.x * 1000;
        force.y += unit.y * 1000;
        force.z += unit.z * 1000;
    }
    return force;
}
} // namespace
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0041dc29
// The caller has already selected an equipped contact weapon and an actual pair-collision force.
bool contact_weapon_front_hit(Vec3 forward, Vec3 position, Vec3 contact) {
    if (!finite(forward) || !finite(position) || !finite(contact))
        return false;
    const Vec3 delta{contact.x - position.x, contact.y - position.y, contact.z - position.z};
    const float a =
        std::sqrt(forward.x * forward.x + forward.y * forward.y + forward.z * forward.z);
    const float b = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
    float cosine = (forward.x * delta.x + forward.y * delta.y + forward.z * delta.z) / (a * b);
    if (cosine <= 1) {
        if (cosine < -1)
            cosine = -1;
    } else
        cosine = 1;
    const float angle =
        static_cast<float>(std::acos(static_cast<double>(cosine)) * 57.288352966308594f);
    return angle < 60;
}
Result ContactWeaponRuntime::initialize(std::string_view name, float length, std::string& error) {
    const auto* descriptor = find_weapon_descriptor(name);
    if (!descriptor ||
        (descriptor->name != "Battering Spikes" && descriptor->name != "Hydraulic Ram")) {
        error = "Unsupported original contact weapon identity";
        return Result::unavailable;
    }
    if (!std::isfinite(length) || length <= 0) {
        error = "Invalid original contact weapon model length";
        return Result::invalid_argument;
    }
    hydraulic_ = descriptor->name == "Hydraulic Ram";
    weapon_length_ = length;
    local_z_ = 0;
    extending_ = active_ = callback_running_ = false;
    initialized_ = true;
    return Result::ok;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004081e3
// The historical CDialog::CreateIndirect library match at this address is false.
// Actual weapon base activation is004cf5ed; native callback registration is separate.
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00455c26
bool ContactWeaponRuntime::begin(const WeaponFireRequest& request) {
    if (!initialized_ || !finite(request.origin) || !std::isfinite(request.heading) ||
        !std::isfinite(request.pitch) || !std::isfinite(request.speed))
        return false;
    if (hydraulic_) {
        // This reset precedes the common active-state gate in00455c26.
        extending_ = true;
        local_z_ = 0;
    }
    if (active_)
        return false;
    active_ = callback_running_ = true;
    return true;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00455cd2
// Animation/state subset; presentation applies localZ to the actual attached weapon root.
Result ContactWeaponRuntime::advance(float dt) {
    if (!initialized_)
        return Result::unavailable;
    if (!std::isfinite(dt) || dt < 0)
        return Result::invalid_argument;
    if (!hydraulic_ || !callback_running_)
        return Result::ok;
    const float maximum = weapon_length_ * .5f;
    if (!extending_) {
        local_z_ -= 2 * dt;
        if (local_z_ < 0)
            local_z_ = 0;
    } else {
        local_z_ += 2 * dt;
        // Strict overshoot: reaching maximum exactly retains the extending latch.
        if (maximum < local_z_) {
            local_z_ = maximum;
            extending_ = false;
        }
    }
    if (local_z_ == 0) {
        active_ = callback_running_ = false;
        local_z_ = 0;
    }
    return Result::ok;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00408213
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00455c63
// Force-vector operation only; the original common call004cf69c records replay damage state.
Result ContactWeaponRuntime::modify_contact_force(Vec3 input, Vec3& output) const {
    if (!initialized_)
        return Result::unavailable;
    if (!finite(input))
        return Result::invalid_argument;
    auto modified = add_strength(input);
    if (hydraulic_ && extending_)
        modified = add_strength(modified);
    if (!finite(modified))
        return Result::invalid_argument;
    output = modified;
    return Result::ok;
}
} // namespace roadwars
