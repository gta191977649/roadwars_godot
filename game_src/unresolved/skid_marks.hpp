#pragma once
#include "game_src/unresolved/particle_effects.hpp"

namespace roadwars {
struct SkidVertex {
    Vec3 position{};
    std::array<float, 2> uv{};
    std::uint32_t color_argb{};
};
struct SkidStrip {
    std::vector<SkidVertex> vertices;
    std::size_t committed_count{};
    std::uint64_t revision{};
};
// Original source/class ownership remains unresolved. Five rolling strips per tire.
class SkidMarkEmitter {
  public:
    Result configure(float skidmark_scalar, float half_tire_width);
    void reset();
    Result advance(float delta_seconds, const TireParticleSource& source);
    const std::array<SkidStrip, 5>& strips() const { return strips_; }

  private:
    void commit(Vec3 point, std::uint32_t color);
    void endpoint(Vec3 point, std::uint32_t color);
    void pair(std::size_t index, Vec3 center, Vec3 offset, float v, std::uint32_t color);
    Vec3 offset(Vec3 previous, Vec3 current) const;
    std::array<SkidStrip, 5> strips_;
    std::array<Vec3, 15> points_{};
    std::size_t strip_index_{}, point_count_{};
    float interval_{.125f}, half_width_{}, timer_{};
    bool enabled_{};
};
} // namespace roadwars
