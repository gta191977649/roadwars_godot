#include "game_src/unresolved/skid_marks.hpp"
#include <cmath>

namespace roadwars {
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004e8483
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004e8306
Result SkidMarkEmitter::configure(float scalar, float half_width) {
    if (!std::isfinite(scalar) || scalar <= 0 || !std::isfinite(half_width) || half_width <= 0)
        return Result::invalid_argument;
    interval_ = .125f / scalar;
    half_width_ = half_width;
    reset();
    return Result::ok;
}
void SkidMarkEmitter::reset() {
    for (auto& strip : strips_) {
        strip.vertices.clear();
        strip.committed_count = 0;
        ++strip.revision;
    }
    strip_index_ = point_count_ = 0;
    timer_ = 0;
    enabled_ = false;
}
Vec3 SkidMarkEmitter::offset(Vec3 previous, Vec3 current) const {
    const float x = previous.x - current.x, y = previous.y - current.y, z = previous.z - current.z;
    constexpr float kAngle = 90.0f * .0174555554986f;
    const float c = static_cast<float>(std::cos(static_cast<double>(kAngle)));
    const float s = static_cast<float>(std::sin(static_cast<double>(kAngle)));
    const float rotated_x = x * c + z * s;
    const float rotated_z = z * c - x * s;
    // The imported D3DRMVectorRotate normalizes its RESULT, not only its axis.
    // Its D3DRMVectorNormalize zero-vector fallback is the unit +X vector.
    const float magnitude = static_cast<float>(
        std::sqrt(static_cast<double>(rotated_z) * rotated_z + static_cast<double>(y) * y +
                  static_cast<double>(rotated_x) * rotated_x));
    if (magnitude == 0)
        return {half_width_, 0, 0};
    return {rotated_x / magnitude * half_width_, 0, rotated_z / magnitude * half_width_};
}
void SkidMarkEmitter::pair(std::size_t index, Vec3 p, Vec3 d, float v, std::uint32_t color) {
    auto& vertices = strips_[strip_index_].vertices;
    if (vertices.size() < index + 2)
        vertices.resize(index + 2);
    vertices[index] = {{p.x - d.x, p.y, p.z - d.z}, {0, v}, color};
    vertices[index + 1] = {{p.x + d.x, p.y, p.z + d.z}, {1, v}, color};
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004e86f0
void SkidMarkEmitter::commit(Vec3 point, std::uint32_t color) {
    auto& strip = strips_[strip_index_];
    if (point_count_ > 14)
        point_count_ = 0;
    points_[point_count_] = point;
    if (point_count_ > 0) {
        const auto previous = points_[point_count_ - 1];
        pair(strip.committed_count, previous, offset(previous, point),
             static_cast<float>(point_count_ - 1), color);
        strip.committed_count += 2;
    }
    ++point_count_;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004e8928
void SkidMarkEmitter::endpoint(Vec3 point, std::uint32_t color) {
    auto& strip = strips_[strip_index_];
    const auto previous = points_[point_count_ - 1];
    const auto d = offset(previous, point);
    if (point_count_ == 1) {
        strip.committed_count = 0;
        pair(0, previous, d, 0, color);
        strip.committed_count = 2;
    }
    pair(strip.committed_count, point, d, static_cast<float>(point_count_), color);
    strip.vertices.resize(strip.committed_count + 2);
    ++strip.revision;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004e849c
// Tire-smoke subcall is handled by TireParticleEmitter; this helper owns only strips.
Result SkidMarkEmitter::advance(float dt, const TireParticleSource& source) {
    if (!std::isfinite(dt) || dt < 0 || !std::isfinite(source.position.x) ||
        !std::isfinite(source.position.y) || !std::isfinite(source.position.z))
        return Result::invalid_argument;
    if (!source.skid_enabled) {
        if (enabled_) {
            enabled_ = false;
            strip_index_ = (strip_index_ + 1) % strips_.size();
        }
        return Result::ok;
    }
    if (!enabled_) {
        point_count_ = 0;
        strips_[strip_index_].committed_count = 0;
        strips_[strip_index_].vertices.clear();
        commit(source.position, source.skid_color_argb);
        timer_ = 0;
        enabled_ = true;
    } else {
        timer_ += dt;
        if (timer_ >= interval_) {
            timer_ = 0;
            commit(source.position, source.skid_color_argb);
        }
    }
    endpoint(source.position, source.skid_color_argb);
    return Result::ok;
}
} // namespace roadwars
