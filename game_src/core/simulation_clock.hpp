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
    }
    double interpolation_alpha() const { return std::clamp(pending_ticks_, 0.0, 1.0); }
    double pending_seconds() const { return pending_ticks_ / kTicksPerSecond; }
    std::uint64_t ticks() const { return ticks_; }

    template <class Tick> Result advance(double seconds, const InputState& input, Tick&& tick) {
        if (!std::isfinite(seconds) || seconds < 0 ||
            !std::isfinite(pending_ticks_ + seconds * kTicksPerSecond))
            return Result::invalid_argument;
        pending_ticks_ += seconds * kTicksPerSecond;
        pending_reset_ = pending_reset_ || (input.reset_requested && !reset_down_);
        reset_down_ = input.reset_requested;
        for (std::uint32_t count = 0; count < kMaxCatchUpSteps && pending_ticks_ + 1e-9 >= 1;
             ++count) {
            auto step_input = input;
            step_input.reset_requested = pending_reset_;
            const auto result = tick(kStepSeconds, step_input);
            if (result != Result::ok)
                return result;
            pending_reset_ = false;
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
};
} // namespace roadwars
