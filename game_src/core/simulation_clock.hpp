#pragma once
#include "game_src/interfaces/host.hpp"
#include <algorithm>
#include <cmath>

namespace roadwars {
// Modern host policy, not an inferred original physics frequency.
// Preserve the established 60 Hz reference feel while decoupling rendering.
class SimulationClock {
  public:
    static constexpr double kTicksPerSecond = 60.0;
    static constexpr float kStepSeconds = 1.0f / 60.0f;
    static constexpr std::uint32_t kMaxCatchUpSteps = 240;
    void reset() {
        pending_ticks_ = 0;
        ticks_ = 0;
        reset_down_ = pending_reset_ = false;
        weapon_down_ = pending_weapons_ = {};
        nitrous_down_ = pending_nitrous_ = false;
        extra_down_ = pending_extra_ = {};
        pending_any_key_ = false;
    }
    double interpolation_alpha() const { return std::clamp(pending_ticks_, 0.0, 1.0); }
    double pending_seconds() const { return pending_ticks_ / kTicksPerSecond; }
    std::uint64_t ticks() const { return ticks_; }

    template <class Tick> Result advance(double seconds, const InputState& input, Tick&& tick) {
        if (!std::isfinite(seconds) || seconds < 0 ||
            !std::isfinite(pending_ticks_ + seconds * kTicksPerSecond))
            return Result::invalid_argument;
        pending_ticks_ += seconds * kTicksPerSecond;
        pending_any_key_ = pending_any_key_ || input.any_key_down;
        pending_reset_ = pending_reset_ || (input.reset_requested && !reset_down_);
        reset_down_ = input.reset_requested;
        for (std::size_t i = 0; i < pending_weapons_.size(); ++i) {
            pending_weapons_[i] = pending_weapons_[i] || input.weapon_requested[i] ||
                                  (input.weapon_down[i] && !weapon_down_[i]);
            weapon_down_[i] = input.weapon_down[i];
        }
        pending_nitrous_ =
            pending_nitrous_ || input.nitrous_requested || (input.nitrous_down && !nitrous_down_);
        nitrous_down_ = input.nitrous_down;
        const std::array<bool, 2> extra_down{input.current_weapon_down, input.cycle_weapon_down};
        const std::array<bool, 2> extra_requested{input.current_weapon_requested,
                                                  input.cycle_weapon_requested};
        for (std::size_t i = 0; i < extra_down.size(); ++i) {
            pending_extra_[i] =
                pending_extra_[i] || extra_requested[i] || (extra_down[i] && !extra_down_[i]);
            extra_down_[i] = extra_down[i];
        }
        for (std::uint32_t count = 0; count < kMaxCatchUpSteps && pending_ticks_ + 1e-9 >= 1;
             ++count) {
            auto step_input = input;
            step_input.any_key_down = input.any_key_down || pending_any_key_;
            step_input.reset_requested = pending_reset_;
            step_input.weapon_requested = pending_weapons_;
            step_input.nitrous_requested = pending_nitrous_;
            step_input.current_weapon_requested = pending_extra_[0];
            step_input.cycle_weapon_requested = pending_extra_[1];
            const auto result = tick(kStepSeconds, step_input);
            if (result != Result::ok)
                return result;
            pending_reset_ = false;
            pending_any_key_ = false;
            pending_weapons_ = {};
            pending_nitrous_ = false;
            pending_extra_ = {};
            pending_ticks_ = std::max(0.0, pending_ticks_ - 1.0);
            ++ticks_;
        }
        // Never discard a stall's elapsed time. Work beyond the per-frame budget
        // remains owed and is consumed on subsequent calls, including seconds=0.
        return Result::ok;
    }

  private:
    double pending_ticks_{};
    std::uint64_t ticks_{};
    bool reset_down_{}, pending_reset_{};
    bool pending_any_key_{};
    std::array<bool, 3> weapon_down_{}, pending_weapons_{};
    bool nitrous_down_{}, pending_nitrous_{};
    std::array<bool, 2> extra_down_{}, pending_extra_{};
};
} // namespace roadwars
