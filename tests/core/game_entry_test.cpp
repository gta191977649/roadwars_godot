#include "game_src/unresolved/game_entry.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace roadwars;
void require_loop(bool value) {
    if (!value)
        throw std::runtime_error("Entry loop contract failed");
}
class RecordingHooks : public LoopHooks {
  public:
    std::vector<LoopStage> stages;
    float delta{};
    bool fail{};
    bool quit_in_common{};
    Result invoke(LoopStage stage, float elapsed, const InputState&, LoopState& state) override {
        stages.push_back(stage);
        delta = elapsed;
        if (quit_in_common && stage == LoopStage::common_00408663)
            state.quit_requested = true;
        return fail ? Result::unavailable : Result::ok;
    }
};
int main() {
    RecordingHooks hooks;
    GameEntry entry(hooks);
    require_loop(entry.advance(0.01, {}) == Result::unavailable);
    require_loop(entry.start() == Result::ok);
    require_loop(hooks.stages.back() == LoopStage::initialize_0045c8ab);
    require_loop(entry.start() == Result::invalid_argument);
    hooks.stages.clear();
    require_loop(entry.advance(1, {}) == Result::ok);
    require_loop(hooks.delta == 0.1f);
    require_loop(hooks.stages == std::vector<LoopStage>{LoopStage::keyboard_004dffdf,
                                                        LoopStage::common_00408663,
                                                        LoopStage::shortcuts_0045d8ed,
                                                        LoopStage::single_race_004a4f57});
    entry.state().mode = GameMode::multiplayer;
    entry.state().joystick_enabled = true;
    entry.state().inactivity_enabled = true;
    entry.state().network_transition_pending = true;
    hooks.stages.clear();
    require_loop(entry.advance(0.02, {}) == Result::ok);
    require_loop(hooks.delta == static_cast<float>(0.02));
    require_loop(
        hooks.stages ==
        std::vector<LoopStage>{LoopStage::keyboard_004dffdf, LoopStage::joystick_004e005a,
                               LoopStage::common_00408663, LoopStage::inactivity_block_0045c29c,
                               LoopStage::shortcuts_0045d8ed, LoopStage::multiplayer_0046d7a0,
                               LoopStage::transition_004ab60d});
    entry.state().mode = GameMode::season;
    require_loop(entry.advance(0.03, {}) == Result::ok);
    require_loop(hooks.stages.back() == LoopStage::season_004a16b0);
    require_loop(entry.advance(-1, {}) == Result::invalid_argument);
    require_loop(entry.advance(std::numeric_limits<double>::quiet_NaN(), {}) ==
                 Result::invalid_argument);
    hooks.stages.clear();
    hooks.quit_in_common = true;
    require_loop(entry.advance(0.01, {}) == Result::ok);
    require_loop(hooks.stages.back() == LoopStage::common_00408663 && !entry.running());
    hooks.quit_in_common = false;
    require_loop(entry.start() == Result::ok);
    InputState input;
    input.quit_requested = true;
    hooks.stages.clear();
    require_loop(entry.advance(0, input) == Result::ok && hooks.stages.empty());
    hooks.fail = true;
    require_loop(entry.start() == Result::unavailable && !entry.running());

    // Frontend wall-time policy dispatches one host iteration with the complete
    // elapsed value. It must neither use the legacy .1-second clamp nor enqueue
    // fixed gameplay ticks, including a stalled frame spanning startup pages.
    RecordingHooks frontend_hooks;
    GameEntry frontend(frontend_hooks, GameEntry::Timing::frontend_realtime);
    require_loop(frontend.start() == Result::ok);
    for (double elapsed : {0.0, 0.001, 0.35, 31.0}) {
        frontend_hooks.stages.clear();
        require_loop(frontend.advance(elapsed, {}) == Result::ok);
        require_loop(frontend_hooks.delta == static_cast<float>(elapsed));
        require_loop(
            frontend_hooks.stages ==
            std::vector<LoopStage>{LoopStage::keyboard_004dffdf, LoopStage::common_00408663,
                                   LoopStage::shortcuts_0045d8ed, LoopStage::single_race_004a4f57});
        require_loop(frontend.simulation_ticks() == 0);
    }
    frontend_hooks.stages.clear();
    for (double invalid :
         {-1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN(),
          static_cast<double>(std::numeric_limits<float>::max()) * 2})
        require_loop(frontend.advance(invalid, {}) == Result::invalid_argument);
    require_loop(frontend_hooks.stages.empty() && frontend.running());
    frontend_hooks.quit_in_common = true;
    require_loop(frontend.advance(2, {}) == Result::ok);
    require_loop(frontend_hooks.stages ==
                 std::vector<LoopStage>{LoopStage::keyboard_004dffdf, LoopStage::common_00408663});
    require_loop(!frontend.running());
    frontend_hooks.quit_in_common = false;
    require_loop(frontend.start() == Result::ok);
    frontend_hooks.stages.clear();
    require_loop(frontend.advance(31, input) == Result::ok);
    require_loop(!frontend.running() && frontend_hooks.stages.empty());
    require_loop(frontend.start() == Result::ok);
    frontend_hooks.stages.clear();
    frontend_hooks.fail = true;
    require_loop(frontend.advance(.35, {}) == Result::unavailable);
    require_loop(!frontend.running() && frontend_hooks.stages.size() == 1);

    // Results switch from fixed gameplay to wall-time frontend outside dispatch.
    RecordingHooks transition_hooks;
    GameEntry transition(transition_hooks, GameEntry::Timing::fixed_simulation);
    require_loop(transition.start() == Result::ok);
    require_loop(transition.advance(.35, {}) == Result::ok);
    require_loop(transition.simulation_ticks() == 21);
    transition.state().mode = GameMode::frontend;
    transition.set_timing(GameEntry::Timing::frontend_realtime);
    transition_hooks.stages.clear();
    require_loop(transition.advance(31, {}) == Result::ok);
    require_loop(transition.simulation_ticks() == 0 && transition_hooks.delta == 31);
    require_loop(transition_hooks.stages.back() == LoopStage::frontend_004ab60d);
}
