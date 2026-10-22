#pragma once
#include "game_src/unresolved/particle_effects.hpp"
namespace roadwars {
// Portable owner; native class/source names remain unresolved. One pool belongs
// to each ordered Stack node and advances only while that node is in range.
class ChimneyParticles {
  public:
    Result configure(float native_view_distance);
    void start();
    Result advance(float delta, Vec3 stack, Vec3 camera, ParticleRandom&);
    std::span<const ParticleRenderState> particles() const { return particles_; }
    std::span<const std::uint32_t> draw_order() const { return order_; }
    std::span<const std::array<Vec3, 3>> triangles() const { return triangles_; }
    std::string_view texture_path() const { return "Textures/xxParticleFX.bmp"; }

  private:
    std::array<ParticleRenderState, 10> particles_{};
    std::array<float, 10> animation_{};
    std::array<std::array<Vec3, 3>, 10> triangles_{};
    std::vector<std::uint32_t> order_;
    std::size_t next_{};
    float range_{}, emission_{};
    bool configured_{}, stopped_{true};
};
} // namespace roadwars
