#include "game_src/unresolved/game_entry.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace roadwars;
void require_loop(bool value) { if (!value) throw std::runtime_error("Entry loop contract failed"); }
class RecordingHooks : public LoopHooks {
  public:
    std::vector<LoopStage> stages;
    float delta{};
    bool fail{};
    bool quit_in_common{};
    Result invoke(LoopStage stage, float elapsed, const InputState&, LoopState& state) override {
        stages.push_back(stage);
        delta = elapsed;
        if (quit_in_common && stage == LoopStage::common_00408663) state.quit_requested = true;
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
        LoopStage::common_00408663, LoopStage::shortcuts_0045d8ed, LoopStage::single_race_004a4f57});
    entry.state().mode = GameMode::multiplayer;
    entry.state().joystick_enabled = true;
    entry.state().inactivity_enabled = true;
    entry.state().network_transition_pending = true;
    hooks.stages.clear();
    require_loop(entry.advance(0.02, {}) == Result::ok);
    require_loop(hooks.delta == static_cast<float>(0.02));
    require_loop(hooks.stages == std::vector<LoopStage>{LoopStage::keyboard_004dffdf,
        LoopStage::joystick_004e005a, LoopStage::common_00408663,
        LoopStage::inactivity_block_0045c29c, LoopStage::shortcuts_0045d8ed,
        LoopStage::multiplayer_0046d7a0, LoopStage::transition_004ab60d});
    entry.state().mode = GameMode::season;
    require_loop(entry.advance(0.03, {}) == Result::ok);
    require_loop(hooks.stages.back() == LoopStage::season_004a16b0);
    require_loop(entry.advance(-1, {}) == Result::invalid_argument);
    require_loop(entry.advance(std::numeric_limits<double>::quiet_NaN(), {}) == Result::invalid_argument);
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
}
