#include "game_src/unresolved/chimney_particles.hpp"
#include <cmath>
namespace roadwars {
namespace {
bool finite(Vec3 p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }
} // namespace
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004fb870
Result ChimneyParticles::configure(float view_distance) {
    if (!std::isfinite(view_distance) || view_distance <= 0)
        return Result::invalid_argument;
    particles_ = {};
    animation_ = {};
    triangles_ = {};
    order_.clear();
    next_ = 0;
    emission_ = 0;
    range_ = view_distance * 1.2000000476837158f;
    configured_ = true;
    stopped_ = true;
    return Result::ok;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004fba48
void ChimneyParticles::start() {
    emission_ = 0;
    stopped_ = false;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004fbaad
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004fbc3f
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004fbe36
Result ChimneyParticles::advance(float dt, Vec3 stack, Vec3 camera, ParticleRandom& random) {
    if (!configured_)
        return Result::unavailable;
    if (!std::isfinite(dt) || dt < 0 || !finite(stack) || !finite(camera))
        return Result::invalid_argument;
    // Native square range test freezes both emission and existing particles.
    if (!(std::abs(stack.x - camera.x) < range_ && std::abs(stack.z - camera.z) < range_))
        return Result::ok;
    emission_ += dt;
    if (emission_ > .5f && !stopped_) {
        emission_ = 0;
        if (order_.size() < 10) {
            auto& p = particles_[next_];
            p = {};
            p.position = stack;
            p.remaining_life = 1;
            p.size = 2.5f;
            p.color_argb = 0xff999999u;
            p.atlas_frame = random.next() % 4;
            p.active = true;
            animation_[next_] = 0;
            triangles_[next_] = {Vec3{-.625f, -1.875f, 0}, Vec3{-.625f, .625f, 0},
                                 Vec3{1.875f, .625f, 0}};
            order_.insert(order_.begin(), static_cast<std::uint32_t>(next_));
            next_ = (next_ + 1) % 10;
        }
    }
    // MFC GetNext advances before the update; every node, including after
    // removal, is visited once. This differs from the vehicle health list.
    for (std::size_t i = 0; i < order_.size();) {
        const auto slot = order_[i];
        auto& p = particles_[slot];
        p.remaining_life -= dt / 3;
        if (p.remaining_life <= 0) {
            p.active = false;
            order_.erase(order_.begin() + static_cast<std::ptrdiff_t>(i));
            continue;
        }
        animation_[slot] += dt;
        if (animation_[slot] > .1f) {
            animation_[slot] = 0;
            p.atlas_frame = (p.atlas_frame + 1) % 4;
        }
        p.color_argb = (static_cast<std::uint32_t>(p.remaining_life * 255) << 24) | 0x999999u;
        p.position.y += dt;
        auto& triangle = triangles_[slot];
        triangle[0].x -= .75f * dt;
        triangle[0].y -= 2.25f * dt;
        triangle[1].x -= .75f * dt;
        triangle[1].y += .75f * dt;
        triangle[2].x += 2.25f * dt;
        triangle[2].y += .75f * dt;
        ++i;
    }
    return Result::ok;
}
} // namespace roadwars
