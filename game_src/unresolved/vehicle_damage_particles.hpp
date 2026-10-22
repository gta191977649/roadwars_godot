#pragma once
#include "game_src/unresolved/particle_effects.hpp"
namespace roadwars {
// Portable owner of the observed vehicle emitter, original source/class unresolved.
class VehicleDamageParticles {
  public:
    VehicleDamageParticles() { reset(); }
    void reset();
    void start();
    void stop(bool clear);
    Result advance(float delta, float damage_fraction, Vec3 source, Vec3 vehicle_velocity,
                   ParticleRandom&);
    std::span<const ParticleRenderState> particles() const { return particles_; }
    std::span<const std::uint32_t> draw_order() const { return order_; }
    bool fire_material(std::size_t slot) const { return particles_.at(slot).remaining_life >= .7f; }
    bool enabled() const { return enabled_; }
    Result atlas_frame(std::size_t slot, std::uint32_t& frame) const;
    static std::array<Vec3, 3> triangle() {
        return {Vec3{-.625f, -1.875f, 0}, Vec3{-.625f, .625f, 0}, Vec3{1.875f, .625f, 0}};
    }

  private:
    std::array<ParticleRenderState, 40> particles_{};
    std::array<float, 40> animation_{};
    std::array<bool, 40> animation_known_{};
    std::array<Vec3, 40> base_rgb_{};
    std::vector<std::uint32_t> order_;
    std::size_t next_slot_{};
    float emission_time_{};
    bool enabled_{}, draining_{};
};
} // namespace roadwars
