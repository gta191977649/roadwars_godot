#pragma once
#include "game_src/core/simulation_clock.hpp"
#include "game_src/interfaces/host.hpp"

namespace roadwars {
// New host adaptation of the observed global loop, not an original class name.
// frontend is a new host mode; values 0..2 retain their recovered meaning.
enum class GameMode { single_race = 0, multiplayer = 1, season = 2, frontend = 3 };
enum class LoopStage {
    initialize_0045c8ab,
    keyboard_004dffdf,
    joystick_004e005a,
    common_00408663,
    inactivity_block_0045c29c,
    shortcuts_0045d8ed,
    single_race_004a4f57,
    multiplayer_0046d7a0,
    season_004a16b0,
    transition_004ab60d,
    frontend_004ab60d
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
    enum class Timing { legacy_reference, fixed_simulation, frontend_realtime };
    explicit GameEntry(LoopHooks& hooks, Timing timing = Timing::legacy_reference)
        : hooks_(hooks), timing_(timing) {}
    Result start();
    Result advance(double delta_seconds, const InputState& input);
    void stop();
    void set_timing(Timing timing) {
        timing_ = timing;
        clock_.reset();
    }
    LoopState& state() { return state_; }
    bool running() const { return running_; }
    double interpolation_alpha() const { return clock_.interpolation_alpha(); }
    std::uint64_t simulation_ticks() const { return clock_.ticks(); }
    static const char* original_address(LoopStage stage);

  private:
    LoopHooks& hooks_;
    LoopState state_{};
    bool running_{};
    Timing timing_;
    SimulationClock clock_;
    Result advance_step(double delta_seconds, const InputState& input);
};
} // namespace roadwars
