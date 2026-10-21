#include "game_src/unresolved/game_entry.hpp"
#include <cmath>
#include <limits>

namespace roadwars {
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0045c29c
// Partial scheduling scaffold only. Original branch bodies remain in LoopHooks.
Result GameEntry::start() {
    if (running_)
        return Result::invalid_argument;
    state_ = {};
    clock_.reset();
    const auto result = hooks_.invoke(LoopStage::initialize_0045c8ab, 0, {}, state_);
    running_ = result == Result::ok && !state_.quit_requested;
    return result;
}
Result GameEntry::advance(double delta_seconds, const InputState& input) {
    if (!running_)
        return Result::unavailable;
    if (!std::isfinite(delta_seconds) || delta_seconds < 0 ||
        delta_seconds > std::numeric_limits<float>::max())
        return Result::invalid_argument;
    if (input.quit_requested) {
        stop();
        return Result::ok;
    }
    if (timing_ == Timing::fixed_simulation)
        return clock_.advance(delta_seconds, input, [&](float step, const InputState& tick_input) {
            return running_ ? advance_step(step, tick_input) : Result::ok;
        });
    return advance_step(delta_seconds, input);
}
Result GameEntry::advance_step(double delta_seconds, const InputState& input) {
    // Legacy reference clamp only. Modern callers supply a constant simulation step.
    // 005f50b0 = float 0.1, verified against hash-bound PE bytes.
    float delta = static_cast<float>(delta_seconds);
    if (delta > 0.1f)
        delta = 0.1f;
    auto invoke = [&](LoopStage stage) {
        if (state_.quit_requested)
            return Result::ok;
        const auto result = hooks_.invoke(stage, delta, input, state_);
        if (result != Result::ok)
            running_ = false;
        return result;
    };
    for (const auto stage :
         {LoopStage::keyboard_004dffdf, LoopStage::joystick_004e005a, LoopStage::common_00408663,
          LoopStage::inactivity_block_0045c29c, LoopStage::shortcuts_0045d8ed}) {
        if (stage == LoopStage::joystick_004e005a && !state_.joystick_enabled)
            continue;
        if (stage == LoopStage::inactivity_block_0045c29c && !state_.inactivity_enabled)
            continue;
        const auto result = invoke(stage);
        if (result != Result::ok)
            return result;
    }
    Result result = Result::ok;
    switch (state_.mode) {
    case GameMode::single_race:
        result = invoke(LoopStage::single_race_004a4f57);
        break;
    case GameMode::multiplayer:
        result = invoke(LoopStage::multiplayer_0046d7a0);
        break;
    case GameMode::season:
        result = invoke(LoopStage::season_004a16b0);
        break;
    default:
        running_ = false;
        return Result::invalid_argument;
    }
    if (result != Result::ok)
        return result;
    if (!state_.quit_requested && state_.mode == GameMode::multiplayer &&
        state_.network_transition_pending)
        result = invoke(LoopStage::transition_004ab60d);
    if (state_.quit_requested)
        stop();
    return result;
}
void GameEntry::stop() {
    running_ = false;
    state_.quit_requested = true;
}
const char* GameEntry::original_address(LoopStage stage) {
    switch (stage) {
    case LoopStage::initialize_0045c8ab:
        return "0045c8ab";
    case LoopStage::keyboard_004dffdf:
        return "004dffdf";
    case LoopStage::joystick_004e005a:
        return "004e005a";
    case LoopStage::common_00408663:
        return "00408663";
    case LoopStage::inactivity_block_0045c29c:
        return "0045c29c";
    case LoopStage::shortcuts_0045d8ed:
        return "0045d8ed";
    case LoopStage::single_race_004a4f57:
        return "004a4f57";
    case LoopStage::multiplayer_0046d7a0:
        return "0046d7a0";
    case LoopStage::season_004a16b0:
        return "004a16b0";
    case LoopStage::transition_004ab60d:
        return "004ab60d";
    }
    return "unknown";
}
} // namespace roadwars
