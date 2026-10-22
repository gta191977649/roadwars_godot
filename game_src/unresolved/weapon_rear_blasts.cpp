#include "game_src/unresolved/weapon_trails.hpp"
#include <cmath>
namespace roadwars {
namespace {
bool finite(Vec3 p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }
} // namespace
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004f4fdb
// Portable emitter lifecycle; original resource ownership remains separate.
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004f641f
// Eager ten-draw initialization retained; original renderer remains separate.
Result WeaponRearBlastPool::configure(std::string_view weapon, ParticleRandom& random) {
    if (weapon != "Grenades" && weapon != "Land Mines" && weapon != "Reactive Mines" &&
        weapon != "Arachnoids")
        return Result::unavailable;
    reactive_ = weapon == "Reactive Mines";
    horizontal_range_ = weapon == "Grenades" ? 150 : 40;
    primary_.assign(50, {});
    hue_.assign(50, 0);
    secondary_.assign(reactive_ ? 10 : 0, {});
    for (std::size_t index = 0; index < secondary_.size(); ++index) {
        const float angle = static_cast<float>(index) * 36 * .017455555498600006f;
        secondary_[index].velocity = {std::sin(angle), 0, std::cos(angle)};
        secondary_[index].size = 12;
        secondary_[index].atlas_frame = random.next() % 4;
    }
    configured_ = true;
    reset();
    return Result::ok;
}
void WeaponRearBlastPool::reset() {
    for (auto& particle : primary_)
        particle.active = false;
    for (auto& particle : secondary_)
        particle.active = false;
    primary_order_.clear();
    secondary_order_.clear();
    primary_created_ = secondary_created_ = 0;
    primary_timer_ = secondary_timer_ = secondary_animation_ = 0;
    primary_active_ = secondary_active_ = false;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004f5714
// Caller registers primary callback when indicated; no private scheduler invented.
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004f6a4f
// Caller registers secondary callback separately, after primary registration.
RearBlastLaunch WeaponRearBlastPool::begin(Vec3 position) {
    RearBlastLaunch launch;
    if (!configured_ || !finite(position))
        return launch;
    if (!primary_active_) {
        primary_active_ = launch.primary = true;
        primary_created_ = 0;
        primary_timer_ = 0;
        primary_origin_ = position;
    }
    if (reactive_ && !secondary_active_) {
        secondary_active_ = launch.secondary = true;
        secondary_created_ = 0;
        secondary_timer_ = secondary_animation_ = 0;
        secondary_origin_ = position;
    }
    return launch;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004f5151
// Render callback remains external.
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004f52de
// Particle creation and shared RNG sequence only.
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004f55e8
// Particle lifetime/color/motion/UV; native renderer remains separate.
Result WeaponRearBlastPool::advance_primary(float dt, ParticleRandom& random) {
    if (!configured_)
        return Result::unavailable;
    if (!std::isfinite(dt) || dt < 0)
        return Result::invalid_argument;
    if (!primary_active_)
        return Result::ok;
    primary_timer_ += dt;
    if (primary_timer_ >= .012500000186264515f && primary_created_ < 50) {
        while (primary_timer_ >= .012500000186264515f) {
            primary_timer_ -= .012500000186264515f;
            if (primary_created_ < 50) {
                const auto index = primary_created_++;
                auto& particle = primary_[index];
                particle = {};
                particle.position = primary_origin_;
                const float center = static_cast<float>(horizontal_range_ / 20);
                particle.velocity = {
                    static_cast<float>(random.next() % horizontal_range_) / 10 - center,
                    static_cast<float>(random.next() % 50) / 10 + 4,
                    static_cast<float>(random.next() % horizontal_range_) / 10 - center};
                particle.position.x -= particle.velocity.x / 10;
                particle.position.z -= particle.velocity.z / 10;
                particle.remaining_life = 1;
                hue_[index] = static_cast<float>(random.next() % 50) / 100;
                particle.atlas_frame = random.next() % 4;
                particle.active = true;
                particle.size = 1;
                primary_order_.insert(primary_order_.begin(), static_cast<std::uint32_t>(index));
            }
        }
    }
    for (auto entry = primary_order_.begin(); entry != primary_order_.end();) {
        auto& particle = primary_[*entry];
        particle.remaining_life -= dt * 2;
        if (particle.remaining_life <= 0) {
            particle.active = false;
            entry = primary_order_.erase(entry);
            continue;
        }
        const auto tint = static_cast<std::uint32_t>((.5f + hue_[*entry]) * 255);
        particle.color_argb = (static_cast<std::uint32_t>(particle.remaining_life * 255) << 24) |
                              0xff0000 | (tint << 8) | tint;
        particle.position = {particle.position.x + particle.velocity.x * dt,
                             particle.position.y + particle.velocity.y * dt,
                             particle.position.z + particle.velocity.z * dt};
        particle.atlas_frame = random.next() % 4;
        ++entry;
    }
    if (primary_created_ >= 50 && primary_order_.empty())
        primary_active_ = false;
    return Result::ok;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004f66f4
// Caller owns callback lifetime/order and rendering.
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004f687b
// Ten initialized radial directions are preserved without creation RNG.
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004f68f6
// Native renderer remains separate.
Result WeaponRearBlastPool::advance_secondary(float dt, ParticleRandom& random) {
    if (!configured_)
        return Result::unavailable;
    if (!std::isfinite(dt) || dt < 0)
        return Result::invalid_argument;
    if (!secondary_active_)
        return Result::ok;
    secondary_timer_ += dt;
    secondary_animation_ += dt;
    while (secondary_created_ < 10) {
        const auto index = secondary_created_++;
        auto& particle = secondary_[index];
        particle.position = secondary_origin_;
        particle.remaining_life = 1;
        particle.active = true;
        secondary_order_.insert(secondary_order_.begin(), static_cast<std::uint32_t>(index));
    }
    const auto aging = dt / 10;
    for (auto entry = secondary_order_.begin(); entry != secondary_order_.end();) {
        auto& particle = secondary_[*entry];
        particle.remaining_life -= aging;
        if (particle.remaining_life <= 0) {
            particle.active = false;
            entry = secondary_order_.erase(entry);
            continue;
        }
        const auto movement = secondary_timer_ >= 1.5f ? aging : dt;
        particle.position = {particle.position.x + particle.velocity.x * movement,
                             particle.position.y + particle.velocity.y * movement,
                             particle.position.z + particle.velocity.z * movement};
        if (secondary_animation_ > .1f)
            particle.atlas_frame = random.next() % 4;
        particle.color_argb =
            (static_cast<std::uint32_t>(particle.remaining_life * 255) << 24) | 0xffffff;
        ++entry;
    }
    if (secondary_order_.empty() && secondary_created_ >= 10)
        secondary_active_ = false;
    if (secondary_animation_ > .1f)
        secondary_animation_ = 0;
    return Result::ok;
}
} // namespace roadwars
