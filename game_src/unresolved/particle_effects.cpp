#include "game_src/unresolved/particle_effects.hpp"
#include <cmath>
#include <stdexcept>

namespace roadwars {
namespace {
Vec3 add(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 scale(Vec3 v, float scalar) { return {v.x * scalar, v.y * scalar, v.z * scalar}; }
} // namespace
std::uint32_t ParticleRandom::next() {
    if (draw_)
        return draw_();
    state_ = state_ * 0x343fdu + 0x269ec3u;
    return (state_ >> 16) & 0x7fffu;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004e5e99
ParticleSettings vehicle_spark_settings(float scalar) {
    if (!std::isfinite(scalar) || scalar < 0 || scalar > 100)
        throw std::invalid_argument("Invalid particle quality scalar");
    return {true,
            1,
            15.0f,
            0.1f,
            0.05f,
            static_cast<std::uint32_t>(15.0f * scalar),
            static_cast<std::uint32_t>(0.1f * scalar),
            0.25f};
}
std::array<Vec3, 3> particle_triangle(float size) {
    return {{{-0.25f * size, -0.75f * size, 0},
             {-0.25f * size, 0.25f * size, 0},
             {0.75f * size, 0.25f * size, 0}}};
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:005008cc
std::array<std::array<float, 2>, 3> particle_atlas_uv(std::uint32_t frame) {
    constexpr float a = 0.21799999475479126f, b = 0.609000027179718f;
    constexpr float c = 0.39100000262260437f, d = 0.7820000052452087f;
    switch (frame) {
    case 0:
        return {{{0, a}, {0, b}, {c, b}}};
    case 1:
        return {{{c, b}, {c, a}, {0, a}}};
    case 2:
        return {{{c, a}, {c, b}, {d, b}}};
    case 3:
        return {{{d, b}, {d, a}, {c, a}}};
    default:
        throw std::invalid_argument("Unknown original particle atlas frame");
    }
}
Result ParticleEmitter::configure(ParticleSettings settings) {
    if (settings.capacity > 100000 || settings.atlas_choice < -4 || settings.atlas_choice > 3 ||
        !std::isfinite(settings.spread) || settings.spread < 0 || !std::isfinite(settings.size) ||
        settings.size <= 0 || !std::isfinite(settings.horizontal_inheritance) ||
        !std::isfinite(settings.vertical_inheritance))
        return Result::invalid_argument;
    settings_ = settings;
    particles_.assign(settings.capacity, {});
    reset();
    return Result::ok;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004e6201
void ParticleEmitter::reset() {
    for (auto& particle : particles_)
        particle = {};
    active_count_ = 0;
    draw_order_.clear();
    next_slot_ = 0;
    emission_time_ = 0;
    cache_time_ = 0;
    cached_delta_ = previous_delta_ = 0.03f;
    origin_ = emitter_velocity_ = emitter_step_ = {};
    active_ = false;
    burst_scale_ = 1.0f;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004e69a8
bool ParticleEmitter::begin_burst(const ParticleImpact& impact) {
    if (active_ || !settings_.burst || !settings_.capacity)
        return false;
    active_ = true;
    origin_ = impact.position;
    emitter_velocity_ = impact.velocity;
    emitter_step_ = scale(emitter_velocity_, cached_delta_);
    burst_scale_ = impact.simulation_scale;
    emission_time_ = static_cast<float>(settings_.capacity) * 0.03f *
                     static_cast<float>(settings_.preload_count);
    next_slot_ = 0;
    return true;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004e655e
void ParticleEmitter::spawn(Vec3 position, Vec3 velocity, ParticleRandom& random) {
    auto& p = particles_[next_slot_];
    p = {};
    p.position = position;
    const auto jitter = [&](float inherit, float base) {
        const float scaled_base = base * inherit;
        return (static_cast<float>(random.next() % 100) * 0.009999999776482582f * settings_.spread +
                scaled_base) -
               settings_.spread / 2.0f;
    };
    p.velocity = {jitter(settings_.horizontal_inheritance, velocity.x),
                  jitter(settings_.vertical_inheritance, velocity.y),
                  jitter(settings_.horizontal_inheritance, velocity.z)};
    p.position.x -= p.velocity.x * 0.10000000149011612f;
    p.position.z -= p.velocity.z * 0.10000000149011612f;
    p.remaining_life = 1.0f;
    p.cached_step = scale(p.velocity, cached_delta_);
    p.atlas_frame = settings_.atlas_choice < 0
                        ? random.next() % static_cast<std::uint32_t>(-settings_.atlas_choice)
                        : static_cast<std::uint32_t>(settings_.atlas_choice);
    p.color_argb = 0xffffffffu;
    p.size = settings_.size;
    p.active = true;
    draw_order_.insert(draw_order_.begin(), next_slot_);
    ++next_slot_;
    ++active_count_;
    if (next_slot_ >= settings_.capacity && !settings_.burst)
        next_slot_ = 0;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004e6271
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004e67ea
Result ParticleEmitter::advance(float dt, bool emitting, Vec3 position, Vec3 velocity,
                                ParticleRandom& random, float simulation_scale) {
    if (!std::isfinite(dt) || dt < 0 || !std::isfinite(simulation_scale) || simulation_scale < 0)
        return Result::invalid_argument;
    if (settings_.burst && !active_)
        return Result::ok;
    cache_time_ += dt;
    if (cache_time_ > 1.0f) {
        cache_time_ = 0;
        cached_delta_ = (dt + previous_delta_) * 0.5f;
        emitter_step_ = scale(emitter_velocity_, cached_delta_);
    }
    previous_delta_ = dt;
    emission_time_ += dt;
    const float interval = settings_.burst ? 0.006666666362434626f : 0.029999999329447746f;
    while (emission_time_ >= interval && emitting && active_count_ < settings_.capacity) {
        emission_time_ -= interval;
        if (settings_.burst) {
            if (next_slot_ < settings_.capacity)
                spawn(origin_, emitter_velocity_, random);
        } else
            spawn(position, velocity, random);
    }
    if (!emitting)
        emission_time_ = 0;
    if (settings_.burst)
        origin_ = add(origin_, emitter_step_);
    for (auto item = draw_order_.begin(); item != draw_order_.end();) {
        auto& p = particles_[*item];
        p.remaining_life -= dt * (settings_.burst ? burst_scale_ : simulation_scale);
        if (p.remaining_life > 0) {
            p.position = add(p.position, p.cached_step);
            const auto alpha = static_cast<std::uint32_t>(p.remaining_life * 255.0f);
            p.color_argb = (alpha << 24) | 0x00ffffffu;
            ++item;
        } else {
            p.active = false;
            --active_count_;
            item = draw_order_.erase(item);
        }
    }
    active_ = active_count_ != 0;
    return Result::ok;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:005007a1
std::array<std::array<float, 2>, 3> tire_particle_atlas_uv(std::uint32_t frame) {
    constexpr float a = 0.609000027179718f, b = 0.39100000262260437f, c = 0.7820000052452087f;
    switch (frame) {
    case 0:
        return {{{0, a}, {0, 1}, {b, 1}}};
    case 1:
        return {{{b, 1}, {b, a}, {0, a}}};
    case 2:
        return {{{b, a}, {b, 1}, {c, 1}}};
    case 3:
        return {{{c, 1}, {c, a}, {b, a}}};
    default:
        throw std::invalid_argument("Unknown original tire particle atlas frame");
    }
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00506485
Result TireParticleEmitter::configure(float scalar, ParticleRandom& random) {
    if (!std::isfinite(scalar) || scalar < 0 || scalar > 100)
        return Result::invalid_argument;
    scalar_ = scalar;
    particles_.assign(static_cast<std::uint32_t>(20.0f * scalar), {});
    for (auto& p : particles_) {
        p.atlas_frame = random.next() % 4;
        p.size = 2.5f;
    }
    reset();
    return Result::ok;
}
void TireParticleEmitter::reset() {
    for (auto& p : particles_)
        p.active = false;
    draw_order_.clear();
    next_slot_ = next_frame_ = 0;
    emission_time_ = cache_time_ = 0;
    previous_delta_ = 0.03f;
    rise_step_ = 0.0075f;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:005066f2
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:005068a2
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:005069ca
Result TireParticleEmitter::advance(float dt, const TireParticleSource& source) {
    if (!std::isfinite(dt) || dt < 0)
        return Result::invalid_argument;
    cache_time_ += dt;
    if (cache_time_ > 1.0f) {
        cache_time_ = 0;
        rise_step_ = 0.25f * ((dt + previous_delta_) * 0.5f);
    }
    previous_delta_ = dt;
    if (draw_order_.empty() && !source.emitting)
        return Result::ok;
    emission_time_ += dt;
    if (source.emitting && emission_time_ >= 0.10000000149011612f &&
        draw_order_.size() < particles_.size()) {
        auto& p = particles_[next_slot_];
        p.position = source.position;
        p.position.y += 0.20000000298023224f;
        p.velocity = {};
        p.remaining_life = 1;
        p.size = 2.5f;
        p.active = true;
        p.color_argb = source.color_argb;
        draw_order_.insert(draw_order_.begin(), next_slot_);
        ++next_slot_;
        if (next_slot_ >= particles_.size())
            next_slot_ = 0;
        emission_time_ = 0;
    }
    for (auto item = draw_order_.begin(); item != draw_order_.end();) {
        auto& p = particles_[*item];
        p.remaining_life -= dt / scalar_;
        if (p.remaining_life > 0) {
            p.atlas_frame = next_frame_;
            next_frame_ = (next_frame_ + 1) % 4;
            p.color_argb = (static_cast<std::uint32_t>(p.remaining_life * 255) << 24) |
                           (p.color_argb & 0x00ffffffu);
            p.cached_step = {0, rise_step_, 0};
            p.position.y += rise_step_;
            ++item;
        } else {
            p.active = false;
            item = draw_order_.erase(item);
        }
    }
    return Result::ok;
}
} // namespace roadwars
