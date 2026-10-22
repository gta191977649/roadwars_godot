#include "game_src/unresolved/vehicle_damage_particles.hpp"
#include <algorithm>
#include <cmath>
namespace roadwars {
namespace {
bool finite(Vec3 p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }
std::uint32_t color(float life, Vec3 rgb) {
    const auto brightness = life > .7f ? 1.f : life * .5f;
    return (static_cast<std::uint32_t>(life * 255) << 24) |
           (static_cast<std::uint32_t>(rgb.x * brightness * 255) << 16) |
           (static_cast<std::uint32_t>(rgb.y * brightness * 255) << 8) |
           static_cast<std::uint32_t>(rgb.z * brightness * 255);
}
} // namespace
void VehicleDamageParticles::reset() {
    particles_ = {};
    animation_ = {};
    animation_known_ = {};
    base_rgb_ = {};
    order_.clear();
    next_slot_ = 0;
    emission_time_ = 0;
    enabled_ = true;
    draining_ = false;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004e7fa0
void VehicleDamageParticles::start() {
    if (!enabled_) {
        enabled_ = true;
        draining_ = false;
        next_slot_ = 0;
    }
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004e7fdb
void VehicleDamageParticles::stop(bool clear) {
    draining_ = true;
    if (clear) {
        for (auto& particle : particles_)
            particle.active = false;
        order_.clear();
        enabled_ = false;
        next_slot_ = 0;
    }
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004e7dde
// Explicit source/velocity preserve the caller's original race phase and source offsets.
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004e79c6
// Native resource and draw submission remain external.
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004e7770
// Custom list iteration/removal and changing live-count loop bound are preserved.
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004e791a
Result VehicleDamageParticles::advance(float dt, float damage, Vec3 source, Vec3 velocity,
                                       ParticleRandom& random) {
    if (!std::isfinite(dt) || dt < 0 || !std::isfinite(damage) || damage < 0 || !finite(source) ||
        !finite(velocity))
        return Result::invalid_argument;
    const auto intensity = std::min(damage, 1.f);
    emission_time_ += dt;
    if (emission_time_ >= .1f && enabled_ && damage > .05f) {
        emission_time_ = 0;
        if (order_.size() < 40) {
            auto& particle = particles_[next_slot_];
            particle = {};
            particle.position = {source.x - velocity.x * dt, source.y - velocity.y * dt,
                                 source.z - velocity.z * dt};
            particle.velocity = {velocity.x + static_cast<float>(random.next() % 100) / 100 - .5f,
                                 velocity.y + 4,
                                 velocity.z + static_cast<float>(random.next() % 100) / 100 - .5f};
            particle.remaining_life = intensity;
            auto& rgb = base_rgb_[next_slot_];
            rgb = intensity < .7f ? Vec3{intensity / 2, intensity / 2, .2f} : Vec3{1, 1, 1};
            particle.color_argb = color(intensity, rgb);
            particle.atlas_frame = random.next() % 4;
            particle.size = 2.5f;
            particle.active = true;
            // Native004e7c35 resets slot0 regardless of selected slot. The slot
            // constructor leaves all other animation timers uninitialized.
            animation_[0] = 0;
            animation_known_[0] = true;
            order_.insert(order_.begin(), static_cast<std::uint32_t>(next_slot_));
            next_slot_ = (next_slot_ + 1) % 40;
        }
    }
    std::size_t cursor{};
    // Original index increases unconditionally, while Count shrinks during removals.
    for (std::size_t index = 0; index < order_.size() && cursor < order_.size(); ++index) {
        const auto slot = order_[cursor];
        auto& particle = particles_[slot];
        if (animation_known_[slot])
            animation_[slot] += dt;
        particle.remaining_life -= dt;
        if (particle.remaining_life >= 0) {
            particle.color_argb = color(particle.remaining_life, base_rgb_[slot]);
            const auto movement = std::max(particle.remaining_life, .3f) * dt;
            particle.position = {particle.position.x + particle.velocity.x * movement,
                                 particle.position.y + particle.velocity.y * movement,
                                 particle.position.z + particle.velocity.z * movement};
            if (animation_known_[slot] && animation_[slot] > .07500000298023224f) {
                animation_[slot] = 0;
                particle.atlas_frame = (particle.atlas_frame + 1) % 4;
            }
            if (cursor + 1 < order_.size())
                ++cursor;
        } else {
            particle.active = false;
            const bool was_head = cursor == 0;
            order_.erase(order_.begin() + static_cast<std::ptrdiff_t>(cursor));
            if (was_head && order_.size() > 1)
                cursor = 1;
        }
    }
    if (draining_ && enabled_ && order_.empty())
        enabled_ = false;
    return Result::ok;
}
Result VehicleDamageParticles::atlas_frame(std::size_t slot, std::uint32_t& frame) const {
    if (slot >= particles_.size())
        return Result::invalid_argument;
    if (!animation_known_[slot])
        return Result::unavailable;
    frame = particles_[slot].atlas_frame;
    return Result::ok;
}
} // namespace roadwars
