#pragma once
#include "game_src/interfaces/host.hpp"
#include <array>
#include <functional>
#include <span>

namespace roadwars {
struct ParticleImpact {
    Vec3 position{}, velocity{};
    float simulation_scale{1.5f};
};
struct TireParticleSource {
    Vec3 position{};
    bool emitting{};
    std::uint32_t color_argb{0xffb2b2b2u};
    bool skid_enabled{};
    std::uint32_t skid_color_argb{0xff191919u};
};
struct ParticleSettings {
    bool burst{};
    std::int32_t atlas_choice{};
    float spread{}, horizontal_inheritance{}, vertical_inheritance{};
    std::uint32_t capacity{}, preload_count{};
    float size{};
};
struct ParticleRenderState {
    Vec3 position{}, velocity{}, cached_step{};
    float remaining_life{}, size{};
    std::uint32_t color_argb{}, atlas_frame{};
    bool active{};
};
// Same numerical CRT generator as 005170b0. Seeded profiles choose a diagnostic
// stream; live callers can borrow the race-owned shared draw callback.
class ParticleRandom {
  public:
    explicit ParticleRandom(std::uint32_t seed = 1) : state_(seed) {}
    // New adapter: borrow the race-owned CRT stream without reseeding it.
    explicit ParticleRandom(std::function<std::uint32_t()> draw)
        : state_(0), draw_(std::move(draw)) {}
    std::uint32_t next();

  private:
    std::uint32_t state_;
    std::function<std::uint32_t()> draw_;
};
ParticleSettings vehicle_spark_settings(float special_effects_scalar);
std::array<Vec3, 3> particle_triangle(float size);
std::array<std::array<float, 2>, 3> particle_atlas_uv(std::uint32_t frame);
std::array<std::array<float, 2>, 3> tire_particle_atlas_uv(std::uint32_t frame);
// Portable original lifecycle. The host displays these slots through native
// Godot particle rendering; simulation state must not be approximated there.
class ParticleEmitter {
  public:
    Result configure(ParticleSettings settings);
    void reset();
    bool begin_burst(const ParticleImpact& impact);
    Result advance(float delta_seconds, bool emitting, Vec3 position, Vec3 velocity,
                   ParticleRandom& random, float simulation_scale = 1.0f);
    std::span<const ParticleRenderState> particles() const { return particles_; }
    std::span<const std::uint32_t> draw_order() const { return draw_order_; }
    const ParticleSettings& settings() const { return settings_; }
    bool active() const { return active_; }
    std::uint32_t active_count() const { return active_count_; }

  private:
    void spawn(Vec3 position, Vec3 velocity, ParticleRandom& random);
    ParticleSettings settings_{};
    std::vector<ParticleRenderState> particles_;
    std::vector<std::uint32_t> draw_order_;
    std::uint32_t active_count_{}, next_slot_{};
    float emission_time_{}, cache_time_{}, cached_delta_{0.03f}, previous_delta_{0.03f};
    float burst_scale_{1.0f};
    Vec3 origin_{}, emitter_velocity_{}, emitter_step_{};
    bool active_{};
};
class TireParticleEmitter {
  public:
    Result configure(float special_effects_scalar, ParticleRandom& random);
    void reset();
    Result advance(float delta_seconds, const TireParticleSource& source);
    std::span<const ParticleRenderState> particles() const { return particles_; }
    std::span<const std::uint32_t> draw_order() const { return draw_order_; }
    std::uint32_t active_count() const { return static_cast<std::uint32_t>(draw_order_.size()); }

  private:
    std::vector<ParticleRenderState> particles_;
    std::vector<std::uint32_t> draw_order_;
    float scalar_{1.0f}, emission_time_{}, cache_time_{}, previous_delta_{0.03f},
        rise_step_{0.0075f};
    std::uint32_t next_slot_{}, next_frame_{};
};
} // namespace roadwars
