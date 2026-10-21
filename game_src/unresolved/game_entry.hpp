#pragma once
#include "game_src/interfaces/host.hpp"

namespace roadwars {
// New host adaptation of the observed global loop, not an original class name.
enum class GameMode { single_race = 0, multiplayer = 1, season = 2 };
enum class LoopStage {
    initialize_0045c8ab, keyboard_004dffdf, joystick_004e005a,
    common_00408663, inactivity_block_0045c29c, shortcuts_0045d8ed,
    single_race_004a4f57, multiplayer_0046d7a0, season_004a16b0,
    transition_004ab60d
};
struct LoopState {
    GameMode mode{GameMode::single_race};
    bool joystick_enabled{};
    bool inactivity_enabled{};
    bool network_transition_pending{};
    bool quit_requested{};
};
class LoopHooks {
  public:
    virtual ~LoopHooks() = default;
    virtual Result invoke(LoopStage stage, float delta_seconds, const InputState& input,
                          LoopState& state) = 0;
};
class GameEntry {
  public:
    explicit GameEntry(LoopHooks& hooks) : hooks_(hooks) {}
    Result start();
    Result advance(double delta_seconds, const InputState& input);
    void stop();
    LoopState& state() { return state_; }
    bool running() const { return running_; }
    static const char* original_address(LoopStage stage);
  private:
    LoopHooks& hooks_;
    LoopState state_{};
    bool running_{};
};
} // namespace roadwars
