#include "game_src/core/simulation_clock.hpp"
#include "game_src/unresolved/game_entry.hpp"
#include <limits>
#include <stdexcept>
using namespace roadwars;
void require(bool condition) {
    if (!condition)
        throw std::runtime_error("Fixed simulation clock contract failed");
}
int main() {
    SimulationClock clock;
    unsigned resets = 0, calls = 0;
    auto tick = [&](float dt, const InputState& input) {
        require(dt == 1.0f / 60);
        ++calls;
        resets += input.reset_requested;
        return Result::ok;
    };
    InputState input;
    input.reset_requested = true;
    require(clock.advance(1.0 / 240, input, tick) == Result::ok && calls == 0);
    input.reset_requested = false;
    clock.advance(3.0 / 240, input, tick);
    require(calls == 1 &&
            resets == 1); // A short reset press survives a frame with no simulation tick.
    input.reset_requested = true;
    clock.advance(.5, input, tick);
    require(calls == 31 && resets == 2);
    clock.advance(.5, input, tick);
    require(calls == 61 && resets == 2); // A held reset does not reset every catch-up step.
    clock.reset();
    calls = 0;
    clock.advance(10, {}, tick);
    require(calls == 240 && clock.pending_seconds() > 5.99 && clock.interpolation_alpha() == 1);
    clock.advance(0, {}, tick);
    clock.advance(0, {}, tick);
    require(calls == 600 && clock.pending_seconds() < 1e-10); // No stall time was discarded.
    require(clock.advance(-1, {}, tick) == Result::invalid_argument);
    require(clock.advance(std::numeric_limits<double>::infinity(), {}, tick) ==
            Result::invalid_argument);
    const auto before = clock.ticks();
    require(clock.advance(1, {}, [](float, const InputState&) { return Result::unavailable; }) ==
            Result::unavailable);
    require(clock.ticks() == before);
    struct Hooks : LoopHooks {
        unsigned ticks{};
        Result invoke(LoopStage stage, float dt, const InputState&, LoopState&) override {
            if (stage == LoopStage::single_race_004a4f57) {
                require(dt == SimulationClock::kStepSeconds);
                ++ticks;
            }
            return Result::ok;
        }
    } hooks;
    GameEntry entry(hooks, GameEntry::Timing::fixed_simulation);
    require(entry.start() == Result::ok);
    require(entry.advance(.5, {}) == Result::ok && hooks.ticks == 30);
    InputState quit;
    quit.quit_requested = true;
    require(entry.advance(0, quit) == Result::ok && !entry.running());
}
