#include "game_src/core/simulation_clock.hpp"
#include "game_src/unresolved/race_session.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
using namespace roadwars;
namespace {
void require(bool value, std::string_view message) {
    if (!value)
        throw std::runtime_error(std::string(message));
}
class Files final : public ResourceReader {
  public:
    explicit Files(std::filesystem::path root) : root_(std::move(root)) {}
    Result read(std::string_view name, std::vector<std::byte>& output) override {
        std::ifstream file(root_ / std::filesystem::path(name), std::ios::binary | std::ios::ate);
        if (!file)
            return Result::not_found;
        const auto size = file.tellg();
        if (size < 0)
            return Result::invalid_argument;
        output.resize(static_cast<std::size_t>(size));
        file.seekg(0);
        file.read(reinterpret_cast<char*>(output.data()), size);
        return file ? Result::ok : Result::invalid_argument;
    }

  private:
    std::filesystem::path root_;
};
} // namespace
int main(int argc, char** argv) {
    try {
        if (argc != 2) {
            std::cout << "SKIP original-resource RaceSession combat lifecycle profile\n";
            return 0;
        }
        std::cout.setf(std::ios::unitbuf);
        Files files(argv[1]);
        ResourceStore resources(files);
        require(resources.open() == Result::ok, "Cannot open original resources");
        RaceOptions options;
        options.ai_count = 0; // Explicit diagnostic participant-count extension.
        options.laps = 10;
        RaceLoadoutTable table;
        std::string error;
        require(table.load(resources, options.track, error) == Result::ok, error);
        std::set<std::string> expected, covered;
        for (const auto& slot : table.weapons())
            for (const auto& name : slot)
                expected.insert(ResourceStore::canonical(name));
        std::vector<std::byte> bytes;
        MeshObject world;
        TrackCollision collision;
        VehicleParameters parameters;
        require(resources.read("Tracks/" + options.track + "/World/SpatialObject.msh", bytes) ==
                    Result::ok,
                "Missing world");
        require(decode_msh(bytes, world, error) == Result::ok, error);
        require(collision.load(resources, options.track, world, error) == Result::ok, error);
        require(load_vehicle_metrics(resources, options.car, parameters, error) == Result::ok,
                error);
        require(load_vehicle_geometry(resources, options.car, parameters, error) == Result::ok,
                error);
        for (options.seed = 1; options.seed <= 512 && covered.size() < expected.size();
             ++options.seed) {
            VehicleDynamics player;
            require(player.initialize(parameters, collision, error) == Result::ok, error);
            RaceSession session;
            require(session.load(resources, collision, options, player, error) == Result::ok,
                    error);
            bool novel{};
            for (const auto& name : session.racers().back().loadout.weapons)
                novel |= !covered.contains(ResourceStore::canonical(name));
            if (!novel)
                continue;
            std::array<int, 3> initial;
            for (std::size_t slot = 0; slot < 3; ++slot)
                initial[slot] = player.combat()->slot(static_cast<WeaponSlot>(slot)).ammunition;
            SimulationClock clock;
            auto advance = [&](double delta, const InputState& input) {
                require(clock.advance(delta, input,
                                      [&](float dt, const InputState& command) {
                                          return session.advance(dt, command, error);
                                      }) == Result::ok,
                        error);
            };
            // Real default lock gate: commands during the first20 race seconds must not reserve ammo.
            std::vector<FrontendRacer> hud_racers;
            require(!session.frontend_frame(hud_racers).weapon_lock_warning,
                    "Weapon lock warning appeared during the opening countdown");
            InputState held;
            held.weapon_down = {true, true, true};
            advance(1.0 / 60, held);
            bool saw_warning{};
            while (session.time() <= 20.1) {
                const bool countdown_was_zero = session.countdown() == 0;
                advance(1.0 / 60, {});
                const bool expected_warning = countdown_was_zero &&
                                              static_cast<float>(session.time()) < 20 &&
                                              player.equipment().dying_timer == 0;
                const auto warning = session.frontend_frame(hud_racers).weapon_lock_warning;
                require(warning == expected_warning,
                        "Weapon lock warning differs from native countdown/time/death predicate");
                saw_warning |= warning;
            }
            require(saw_warning && !session.frontend_frame(hud_racers).weapon_lock_warning,
                    "Weapon lock warning did not clear at the original20-second threshold");
            for (std::size_t slot = 0; slot < 3; ++slot)
                require(player.combat()->slot(static_cast<WeaponSlot>(slot)).ammunition ==
                            initial[slot],
                        "Locked player command consumed ammunition");
            // A press/release wholly between simulation steps must survive until the next fixed tick.
            InputState pulse;
            pulse.weapon_requested = {true, true, true};
            const auto ticks = session.ticks();
            advance(0, pulse);
            advance(0, {});
            require(session.ticks() == ticks, "Zero-step input advanced the race");
            advance(1.0 / 60, {});
            session.set_paused(true);
            const auto deployment_tick = session.ticks();
            const auto deployment_random = session.random_state();
            require(session.advance(.25f, pulse, error) == Result::ok, error);
            require(session.ticks() == deployment_tick &&
                        session.random_state() == deployment_random,
                    "Pause changed freshly triggered/deploying weapon state");
            session.set_paused(false);
            for (int i = 1; i < 60; ++i)
                advance(1.0 / 60, {});
            std::array<int, 3> fired;
            for (std::size_t slot = 0; slot < 3; ++slot) {
                const auto weapon = player.combat()->slot(static_cast<WeaponSlot>(slot));
                fired[slot] = weapon.ammunition;
                require(weapon.descriptor != nullptr, "Assigned weapon is missing");
                const auto name = std::string(weapon.descriptor->name);
                require(name == "Battering Spikes" || fired[slot] < initial[slot],
                        "Accepted player activation did not consume actual mount ammunition: " +
                            name);
                covered.insert(ResourceStore::canonical(name));
                std::cout << "SESSION_WEAPON " << name << " seed=" << options.seed << '\n';
            }
            session.set_paused(true);
            const auto paused_tick = session.ticks();
            const auto paused_random = session.random_state();
            require(session.advance(.25f, pulse, error) == Result::ok, error);
            require(session.ticks() == paused_tick && session.random_state() == paused_random,
                    "Paused in-flight weapon advanced simulation/randomness");
            for (std::size_t slot = 0; slot < 3; ++slot)
                require(player.combat()->slot(static_cast<WeaponSlot>(slot)).ammunition ==
                            fired[slot],
                        "Paused in-flight weapon consumed ammunition");
            session.set_paused(false);
            // Exercise scheduler debt with active effects, then restart before all mines/trails drain.
            advance(.4, held);
            while (clock.pending_seconds() >= SimulationClock::kStepSeconds)
                advance(0, held);
            require(session.restart(error) == Result::ok, error);
            require(session.ticks() == 0 && session.time() == 0 &&
                        session.ending() == RaceEndReason::none,
                    "Restart retained in-flight session state");
            for (std::size_t slot = 0; slot < 3; ++slot) {
                const auto weapon = player.combat()->slot(static_cast<WeaponSlot>(slot));
                require(weapon.ammunition == initial[slot] && !weapon.active && !weapon.pending,
                        "Restart retained weapon ammunition/activity/deployment");
            }
            clock.reset();
            while (session.time() <= 20.1)
                advance(1.0 / 60, {});
            advance(0, pulse);
            for (int i = 0; i < 60; ++i)
                advance(1.0 / 60, {});
            // Exit by scope destruction with live callbacks/effects; the player outlives its session.
        }
        require(covered == expected,
                "Diagnostic seed search did not cover every original available weapon");
        std::cout << "RACE_SESSION_COMBAT_LIFECYCLE_OK branches=" << covered.size() << '\n';
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
