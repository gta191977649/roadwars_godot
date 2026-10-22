#pragma once
#include "game_src/unresolved/particle_effects.hpp"

namespace roadwars {
// Portable emitter ownership; no recovered original class-name claim.
class WeaponTrailPool {
  public:
    Result configure(std::string_view weapon, float special_effects_scalar);
    void reset();
    Result advance(float delta, bool emitting, Vec3 source, Vec3 velocity, ParticleRandom&,
                   float heading = 0);
    std::size_t active_count() const;
    std::span<const ParticleRenderState> particles() const;
    std::span<const std::uint32_t> draw_order() const;
    // Rocket raw animation frames4..9 leave the original slot UV untouched.
    // An untouched initial UV is undefined, and is explicitly unavailable here.
    Result atlas_frame(std::size_t slot, std::uint32_t& render_frame) const;
    bool rocket_smoke() const { return rocket_; }
    bool smoke_screen() const { return screen_; }
    std::string_view texture_path() const {
        return rocket_ || screen_ ? "Textures/xxParticleFX.bmp" : "Textures/16ParticleFX.bmp";
    }
    bool uses_tire_atlas() const { return rocket_ || screen_; }
    std::span<const std::array<Vec3, 3>> screen_triangles() const { return triangles_; }

  private:
    Result advance_screen(float, bool, Vec3, float heading, ParticleRandom&);
    ParticleEmitter plasma_;
    std::vector<ParticleRenderState> smoke_;
    std::vector<float> animation_time_;
    std::vector<std::uint32_t> order_, uv_frame_;
    std::vector<bool> uv_valid_;
    std::vector<std::array<Vec3, 3>> triangles_;
    Vec3 previous_{};
    float distance_{}, emission_time_{}, simulation_scale_{1};
    std::size_t next_slot_{};
    bool configured_{}, rocket_{}, screen_{}, previous_emitting_{};
};
// One instance per original projectile/weapon impact emitter; updates belong to
// the shared tail-registered callback stream, independently of the moving object.
class WeaponImpactPool {
  public:
    Result configure(std::string_view weapon, float special_effects_scalar,
                     float guided_missile_count = 1);
    bool begin(const ParticleImpact& impact) { return emitter_.begin_burst(impact); }
    Result advance(float delta, ParticleRandom& random) {
        return emitter_.advance(delta, true, {}, {}, random);
    }
    void reset() { emitter_.reset(); }
    bool active() const { return emitter_.active(); }
    std::span<const ParticleRenderState> particles() const { return emitter_.particles(); }
    std::span<const std::uint32_t> draw_order() const { return emitter_.draw_order(); }
    const ParticleSettings& settings() const { return emitter_.settings(); }
    bool requires_secondary_sprite() const { return secondary_sprite_; }
    std::string_view texture_path() const { return "Textures/16ParticleFX.bmp"; }

  private:
    ParticleEmitter emitter_;
    bool secondary_sprite_{};
};
class VulcanImpactSprite {
  public:
    bool begin(Vec3 position, Vec3 velocity);
    Result advance(float delta);
    void reset() {
        particle_ = {};
        scale_ = 1;
    }
    const ParticleRenderState& particle() const { return particle_; }
    std::string_view texture_path() const { return "Textures/16ParticleFX2.bmp"; }

  private:
    ParticleRenderState particle_;
    float scale_{1};
};
struct RearBlastLaunch {
    bool primary{}, secondary{};
};
// Distinct specialized rear explosion systems004f4fdb and004f641f.
class WeaponRearBlastPool {
  public:
    Result configure(std::string_view weapon, ParticleRandom&);
    RearBlastLaunch begin(Vec3);
    Result advance_primary(float delta, ParticleRandom&);
    Result advance_secondary(float delta, ParticleRandom&);
    void reset();
    bool primary_active() const { return primary_active_; }
    bool secondary_active() const { return secondary_active_; }
    std::span<const ParticleRenderState> primary_particles() const { return primary_; }
    std::span<const ParticleRenderState> secondary_particles() const { return secondary_; }
    std::span<const std::uint32_t> primary_order() const { return primary_order_; }
    std::span<const std::uint32_t> secondary_order() const { return secondary_order_; }

  private:
    std::vector<ParticleRenderState> primary_, secondary_;
    std::vector<float> hue_;
    std::vector<std::uint32_t> primary_order_, secondary_order_;
    Vec3 primary_origin_{}, secondary_origin_{};
    std::size_t primary_created_{}, secondary_created_{};
    std::uint32_t horizontal_range_{};
    float primary_timer_{}, secondary_timer_{}, secondary_animation_{};
    bool configured_{}, reactive_{}, primary_active_{}, secondary_active_{};
};
} // namespace roadwars
