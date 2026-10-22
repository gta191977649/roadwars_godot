#include "game_src/unresolved/weapon_trails.hpp"
#include <algorithm>
#include <cmath>

namespace roadwars {
namespace {
bool finite(Vec3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}
} // namespace
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0047dd34
// Continuous Plasma emitter configuration only; impact emitters remain separate.
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0044e926
// Guided continuous emitter configuration only; impact emitters remain separate.
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00473d30
// Nitrous continuous emitter configuration only; controller/network remain separate.
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00433867
// Electron continuous impact emitter configuration only; beam remains separate.
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004f9037
// Pool lifecycle only; original resource/renderer ownership remains separate.
Result WeaponTrailPool::configure(std::string_view weapon, float scalar) {
    if (!std::isfinite(scalar) || scalar < 0 || scalar > 100)
        return Result::invalid_argument;
    const bool guided = weapon == "Guided Missile" || weapon == "Back Missile" ||
                        weapon == "Swarmers" || weapon == "The Excalibur";
    const bool nitrous = weapon == "Nitrous Boost";
    const bool electron = weapon == "Electron Cannon";
    const bool screen = weapon == "Smoke Screen";
    if (weapon != "Plasma Blaster" && weapon != "Rocket Launcher" && !guided && !nitrous &&
        !electron && !screen)
        return Result::unavailable;
    rocket_ = weapon == "Rocket Launcher";
    screen_ = screen;
    if (rocket_ || screen_) {
        const auto capacity = rocket_ ? 200u : 20u;
        smoke_.assign(capacity, {});
        animation_time_.assign(capacity, 0);
        uv_frame_.assign(capacity, 0);
        uv_valid_.assign(capacity, false);
        triangles_.assign(screen_ ? capacity : 0, {});
    } else {
        const bool excalibur = weapon == "The Excalibur";
        const auto settings = electron  ? ParticleSettings{false,
                                                           0,
                                                           10,
                                                           .75f,
                                                           1,
                                                           static_cast<std::uint32_t>(150 * scalar),
                                                           static_cast<std::uint32_t>(.1f * scalar),
                                                           1}
                              : nitrous ? ParticleSettings{false,
                                                           3,
                                                           3.5f,
                                                           .5f,
                                                           .8f,
                                                           static_cast<std::uint32_t>(150 * scalar),
                                                           static_cast<std::uint32_t>(.1f * scalar),
                                                           3}
                              : guided  ? ParticleSettings{false,
                                                           excalibur              ? -4
                                                           : weapon == "Swarmers" ? 1
                                                                                  : 2,
                                                           2,
                                                           .5f,
                                                           1,
                                                           static_cast<std::uint32_t>(150 * scalar),
                                                           static_cast<std::uint32_t>(.1f * scalar),
                                                           excalibur ? 3.f : 1.f}
                                        : ParticleSettings{false,
                                                           2,
                                                           3,
                                                           .75f,
                                                           .05f,
                                                           static_cast<std::uint32_t>(100 * scalar),
                                                           static_cast<std::uint32_t>(.1f * scalar),
                                                           1};
        const auto result = plasma_.configure(settings);
        if (result != Result::ok)
            return result;
    }
    simulation_scale_ = guided ? .75f : electron ? 1.5f : 1.f;
    configured_ = true;
    reset();
    return Result::ok;
}
void WeaponTrailPool::reset() {
    plasma_.reset();
    for (auto& particle : smoke_)
        particle.active = false;
    order_.clear();
    std::fill(animation_time_.begin(), animation_time_.end(), 0);
    previous_ = {};
    previous_emitting_ = false;
    next_slot_ = 0;
    distance_ = 0;
    emission_time_ = 0;
}
std::size_t WeaponTrailPool::active_count() const {
    return rocket_ || screen_ ? order_.size() : plasma_.active_count();
}
std::span<const ParticleRenderState> WeaponTrailPool::particles() const {
    return rocket_ || screen_ ? std::span<const ParticleRenderState>(smoke_) : plasma_.particles();
}
std::span<const std::uint32_t> WeaponTrailPool::draw_order() const {
    return rocket_ || screen_ ? std::span<const std::uint32_t>(order_) : plasma_.draw_order();
}
Result WeaponTrailPool::atlas_frame(std::size_t slot, std::uint32_t& frame) const {
    if (!configured_ || slot >= particles().size())
        return Result::invalid_argument;
    if (rocket_ || screen_) {
        if (!uv_valid_[slot])
            return Result::unavailable;
        frame = uv_frame_[slot];
    } else
        frame = plasma_.particles()[slot].atlas_frame;
    return Result::ok;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004f928e
// Source transform and rendering are supplied by the caller; lifecycle and emission recovered.
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004f9556
// Raw UV ambiguity is explicit, rather than reading uninitialized original memory.
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004f969b
// Particle lifetime/animation subset; renderer remains separate.
Result WeaponTrailPool::advance(float dt, bool emitting, Vec3 source, Vec3 velocity,
                                ParticleRandom& random, float heading) {
    if (!configured_)
        return Result::unavailable;
    if (!std::isfinite(dt) || dt < 0 || !finite(source) || !finite(velocity) ||
        !std::isfinite(heading))
        return Result::invalid_argument;
    if (screen_)
        return advance_screen(dt, emitting, source, heading, random);
    if (!rocket_)
        return plasma_.advance(dt, emitting, source, velocity, random, simulation_scale_);
    if (emitting != previous_emitting_) {
        previous_emitting_ = emitting;
        previous_ = source;
    }
    if (emitting) {
        distance_ += std::abs(previous_.x - source.x) + std::abs(previous_.z - source.z);
        unsigned count{};
        while (distance_ >= 1.850000023841858f) {
            distance_ -= 1.850000023841858f;
            count = std::min(count + 1, 3u);
        }
        if (count > 0) {
            const Vec3 step{(source.x - previous_.x) / count, (source.y - previous_.y) / count,
                            (source.z - previous_.z) / count};
            auto position = previous_;
            for (unsigned index = 0; index < count; ++index) {
                position = {position.x + step.x, position.y + step.y, position.z + step.z};
                if (order_.size() < 200) {
                    auto& particle = smoke_[next_slot_];
                    particle = {};
                    particle.position = position;
                    particle.remaining_life = 1;
                    particle.size = 2.5f;
                    particle.atlas_frame = random.next() % 10;
                    particle.color_argb = 0xff7f7f7f;
                    particle.active = true;
                    animation_time_[next_slot_] = 0;
                    if (particle.atlas_frame < 4) {
                        uv_valid_[next_slot_] = true;
                        uv_frame_[next_slot_] = particle.atlas_frame;
                    }
                    order_.insert(order_.begin(), static_cast<std::uint32_t>(next_slot_));
                    next_slot_ = (next_slot_ + 1) % 200;
                }
            }
            previous_ = source;
            distance_ = 0;
        }
    }
    const auto aging = dt / 1.75f;
    for (auto entry = order_.begin(); entry != order_.end();) {
        auto& particle = smoke_[*entry];
        particle.remaining_life -= aging;
        if (particle.remaining_life <= 0) {
            particle.active = false;
            entry = order_.erase(entry);
            continue;
        }
        auto& timer = animation_time_[*entry];
        timer += aging;
        if (timer > .1f) {
            timer = 0;
            ++particle.atlas_frame;
            if (particle.atlas_frame > 3)
                particle.atlas_frame = 0;
        }
        if (particle.atlas_frame < 4) {
            uv_valid_[*entry] = true;
            uv_frame_[*entry] = particle.atlas_frame;
        }
        particle.color_argb =
            (static_cast<std::uint32_t>(particle.remaining_life * 255) << 24) | 0x7f7f7f;
        ++entry;
    }
    return Result::ok;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004fa406
// Portable pool configuration/reset is separate from original renderer ownership.
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004fa546
// Camera-facing transforms and draw calls remain external.
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004fa69f
// Undefined initial UV handled explicitly by atlas_frame.
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004fa9ea
// Original evolving triangle geometry exposed; renderer remains external.
Result WeaponTrailPool::advance_screen(float dt, bool emitting, Vec3 source, float heading,
                                       ParticleRandom& random) {
    if (emitting) {
        emission_time_ += dt;
        if (emission_time_ >= .2f) {
            emission_time_ = 0;
            if (order_.size() < 20) {
                auto& particle = smoke_[next_slot_];
                particle = {};
                particle.atlas_frame = random.next() % 10;
                const auto direction =
                    (heading + (next_slot_ % 2 ? 90.f : -90.f)) * .017455555498600006f;
                particle.velocity = {std::sin(direction) / 3.f,
                                     static_cast<float>(random.next() % 15) / 100.f,
                                     std::cos(direction) / 3.f};
                particle.position = {source.x + particle.velocity.x, source.y,
                                     source.z + particle.velocity.z};
                particle.remaining_life = 2;
                particle.size = 1;
                particle.active = true;
                particle.color_argb = 0xff999999;
                animation_time_[next_slot_] = 0;
                triangles_[next_slot_] = {{{-.25f, -.75f, 0}, {-.25f, .25f, 0}, {.75f, .25f, 0}}};
                if (particle.atlas_frame < 4) {
                    uv_valid_[next_slot_] = true;
                    uv_frame_[next_slot_] = particle.atlas_frame;
                }
                order_.insert(order_.begin(), static_cast<std::uint32_t>(next_slot_));
                next_slot_ = (next_slot_ + 1) % 20;
            }
        }
    }
    for (auto entry = order_.begin(); entry != order_.end();) {
        auto& particle = smoke_[*entry];
        particle.remaining_life -= dt / 5;
        if (particle.remaining_life <= 0) {
            particle.active = false;
            entry = order_.erase(entry);
            continue;
        }
        auto& timer = animation_time_[*entry];
        timer += dt;
        if (timer > .1f) {
            timer = 0;
            ++particle.atlas_frame;
            if (particle.atlas_frame > 3)
                particle.atlas_frame = 0;
        }
        if (particle.atlas_frame < 4) {
            uv_valid_[*entry] = true;
            uv_frame_[*entry] = particle.atlas_frame;
        }
        auto& triangle = triangles_[*entry];
        if (triangle[2].x < 12) {
            triangle[0].x -= 6 * dt;
            triangle[0].y -= 18 * dt;
            triangle[1].x -= 6 * dt;
            triangle[1].y += 6 * dt;
            triangle[2].x += 18 * dt;
            triangle[2].y += 6 * dt;
        }
        particle.position = {particle.position.x + particle.velocity.x * dt,
                             particle.position.y + particle.velocity.y * dt,
                             particle.position.z + particle.velocity.z * dt};
        particle.color_argb =
            particle.remaining_life > 1
                ? 0xff999999u
                : (static_cast<std::uint32_t>(particle.remaining_life * 255) << 24) | 0x999999;
        ++entry;
    }
    return Result::ok;
}
Result WeaponImpactPool::configure(std::string_view weapon, float scalar, float missile_count) {
    if (!std::isfinite(scalar) || scalar < 0 || scalar > 100 || !std::isfinite(missile_count) ||
        missile_count <= 0)
        return Result::invalid_argument;
    const bool small =
        weapon == "Machine Gun" || weapon == "20mm Cannon" || weapon == "Gatling Gun";
    const bool plasma = weapon == "Plasma Blaster";
    const bool vulcan = weapon == "Vulcanized Gun";
    const bool rocket = weapon == "Rocket Launcher";
    const bool guided = weapon == "Guided Missile" || weapon == "Back Missile" ||
                        weapon == "Swarmers" || weapon == "The Excalibur";
    if (!small && !plasma && !vulcan && !rocket && !guided)
        return Result::unavailable;
    float base_capacity = small ? 15.f : plasma || vulcan ? 50.f : 150.f;
    if (guided)
        base_capacity = static_cast<float>(static_cast<std::uint32_t>(150.f / missile_count));
    const auto atlas = vulcan || weapon == "The Excalibur" ? -4
                       : guided                            ? weapon == "Swarmers" ? 2 : -2
                       : small || rocket                   ? 1
                                                           : 2;
    const auto result = emitter_.configure(
        {true, atlas, small || plasma || vulcan ? 5.f : 15.f,
         small || plasma || vulcan ? .3f : .35f, .05f,
         static_cast<std::uint32_t>(base_capacity * scalar),
         static_cast<std::uint32_t>((small || plasma || vulcan ? .5f : .1f) * scalar),
         small                       ? .25f
         : weapon == "The Excalibur" ? 8.f
                                     : 1.f});
    if (result == Result::ok)
        secondary_sprite_ = vulcan;
    return result;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004f8e84
// Caller registers independent callback when begin succeeds.
bool VulcanImpactSprite::begin(Vec3 position, Vec3 velocity) {
    if (particle_.active || !finite(position) || !finite(velocity))
        return false;
    particle_ = {};
    particle_.position = position;
    particle_.velocity = velocity;
    particle_.remaining_life = 1;
    particle_.active = true;
    particle_.atlas_frame = 1;
    particle_.size = 2;
    particle_.color_argb = 0xffffffff;
    scale_ = 1;
    return true;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004f8b7d
// Original sprite lifetime/motion/scale only; camera transform/render external.
Result VulcanImpactSprite::advance(float dt) {
    if (!std::isfinite(dt) || dt < 0)
        return Result::invalid_argument;
    if (!particle_.active)
        return Result::ok;
    particle_.remaining_life -= dt * 4;
    scale_ += dt * 80;
    particle_.size = 2 * scale_;
    if (particle_.remaining_life < 0) {
        particle_.active = false;
        return Result::ok;
    }
    particle_.color_argb =
        (static_cast<std::uint32_t>(particle_.remaining_life * 255) << 24) | 0xffffff;
    const auto movement = std::max(particle_.remaining_life, .2f) * dt;
    particle_.position = {particle_.position.x + particle_.velocity.x * movement,
                          particle_.position.y + particle_.velocity.y * movement,
                          particle_.position.z + particle_.velocity.z * movement};
    return Result::ok;
}
} // namespace roadwars
