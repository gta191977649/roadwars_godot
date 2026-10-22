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
    std::array<unsigned, 3> shots{};
    unsigned boosts{};
    auto combat_tick = [&](float, const InputState& controls) {
        for (std::size_t i = 0; i < shots.size(); ++i)
            shots[i] += controls.weapon_requested[i];
        boosts += controls.nitrous_requested;
        return Result::ok;
    };
    input = {};
    input.weapon_down = {true, true, true};
    input.nitrous_down = true;
    require(clock.advance(1.0 / 240, input, combat_tick) == Result::ok);
    require(shots == std::array<unsigned, 3>{} && boosts == 0);
    input = {};
    require(clock.advance(3.0 / 240, input, combat_tick) == Result::ok);
    require(shots == std::array<unsigned, 3>{1, 1, 1} && boosts == 1);
    input.weapon_down = {true, false, true};
    input.nitrous_down = true;
    require(clock.advance(.5, input, combat_tick) == Result::ok);
    require(shots == std::array<unsigned, 3>{2, 1, 2} && boosts == 2);
    require(clock.advance(.5, input, combat_tick) == Result::ok);
    require(shots == std::array<unsigned, 3>{2, 1, 2} && boosts == 2);
    input = {};
    input.weapon_requested[1] = true;
    require(clock.advance(1.0 / 60, input, [](float, const InputState&) {
        return Result::unavailable;
    }) == Result::unavailable);
    require(clock.advance(0, {}, combat_tick) == Result::ok);
    require(shots == std::array<unsigned, 3>{2, 2, 2});
    clock.reset();
    unsigned key_ticks{};
    const auto key_tick = [&](float, const InputState& controls) {
        key_ticks += controls.any_key_down;
        return Result::ok;
    };
    input = {};
    input.any_key_down = true;
    require(clock.advance(1.0 / 240, input, key_tick) == Result::ok && key_ticks == 0);
    require(clock.advance(7.0 / 240, {}, key_tick) == Result::ok && key_ticks == 1);
    require(clock.advance(.1, input, key_tick) == Result::ok && key_ticks == 7);
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
