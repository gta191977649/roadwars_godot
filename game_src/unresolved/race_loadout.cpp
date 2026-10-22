#include "game_src/unresolved/race_loadout.hpp"
#include <cmath>

namespace roadwars {
namespace {
Result ordering(ResourceStore& resources, const std::string& base, std::vector<std::string>& output,
                std::string& error) {
    Configuration config;
    float count{};
    if (config.load(resources, base + "/Data.ini") != Result::ok ||
        config.number("ORDERING", "NUMITEMS", count) != Result::ok || count < 1 || count > 256 ||
        count != std::floor(count)) {
        error = "Missing or malformed equipment ordering: " + base;
        return Result::invalid_argument;
    }
    output.clear();
    for (int i = 0; i < static_cast<int>(count); ++i) {
        auto value = config.value("ORDERING", "ITEM" + std::to_string(i));
        if (value.empty() || value.find_first_of("/\\:") != std::string::npos ||
            value.find("..") != std::string::npos) {
            error = "Invalid equipment ordering entry: " + base;
            return Result::invalid_argument;
        }
        output.push_back(std::move(value));
    }
    return Result::ok;
}
} // namespace
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:005170b0
std::uint32_t original_random(std::uint32_t& state) {
    state = state * 0x343fdu + 0x269ec3u;
    return (state >> 16) & 0x7fffu;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004a527a
// Character draw/removal sequence; availability and preserved player are supplied by caller.
Result select_race_characters(std::uint32_t& state, std::span<const std::string> available,
                              std::string_view player, std::size_t player_slot, std::size_t slots,
                              std::vector<std::string>& output, std::string& error) {
    if (slots < 1 || slots > 6 || player_slot >= slots || player.empty() ||
        available.size() < slots - 1) {
        error = "Original character selection inputs are invalid";
        return Result::invalid_argument;
    }
    std::vector<std::string> candidates(available.begin(), available.end());
    for (const auto& candidate : candidates)
        if (candidate.empty() ||
            ResourceStore::canonical(candidate) == ResourceStore::canonical(player)) {
            error = "Opponent table includes an invalid or player character";
            return Result::invalid_argument;
        }
    std::vector<std::string> next(slots);
    next[player_slot] = player;
    for (std::size_t slot = 0; slot < slots; ++slot) {
        if (slot == player_slot)
            continue;
        const auto index = static_cast<std::size_t>(original_random(state) % candidates.size());
        next[slot] = candidates[index];
        candidates.erase(candidates.begin() + static_cast<std::ptrdiff_t>(index));
    }
    output = std::move(next);
    error.clear();
    return Result::ok;
}
bool original_randomizes_loadout(std::size_t slot, std::size_t player_slot, bool force,
                                 bool race_eec) {
    return slot != player_slot || force || race_eec;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00404490
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004a55a9
Result RaceLoadoutTable::load(ResourceStore& resources, std::string_view track,
                              std::string& error) {
    RaceLoadoutTable result;
    const std::array<std::string, 3> slots{"Front", "Side", "Rear"};
    for (std::size_t slot = 0; slot < slots.size(); ++slot) {
        const auto base = "Cars/Shared/Weapons/" + slots[slot];
        std::vector<std::string> candidates;
        if (ordering(resources, base, candidates, error) != Result::ok)
            return Result::invalid_argument;
        for (auto& candidate : candidates) {
            Configuration config;
            if (config.load(resources, base + "/" + candidate + "/Data.ini") != Result::ok) {
                error = "Missing weapon configuration: " + candidate;
                return Result::not_found;
            }
            // 00404490 initializes ISAVAILABLE to one before its optional read.
            float available = 1;
            if (!config.value("PRICE", "ISAVAILABLE").empty() &&
                config.number("PRICE", "ISAVAILABLE", available) != Result::ok) {
                error = "Invalid weapon availability: " + candidate;
                return Result::invalid_argument;
            }
            if (available != 0)
                result.weapons_[slot].push_back(candidate);
        }
        if (result.weapons_[slot].empty()) {
            error = "No original weapons available for slot: " + slots[slot];
            return Result::unavailable;
        }
    }
    auto choices = [&](const std::string& base, std::vector<Choices>& output) {
        std::vector<std::string> names;
        if (ordering(resources, base, names, error) != Result::ok)
            return Result::invalid_argument;
        for (auto& name : names) {
            Choices choice;
            choice.type = name;
            if (ordering(resources, base + "/" + name, choice.quality, error) != Result::ok)
                return Result::invalid_argument;
            output.push_back(std::move(choice));
        }
        return Result::ok;
    };
    if (choices("Cars/Shared/Armor", result.armor_) != Result::ok ||
        choices("Cars/Shared/Drive Train", result.drivetrain_) != Result::ok)
        return Result::invalid_argument;
    Configuration config;
    if (config.load(resources, "Tracks/" + std::string(track) + "/World/Data.txt") != Result::ok ||
        (result.tire_type_ = config.value("AI", "TIRES")).empty()) {
        error = "Original track AI/TIRES is unavailable";
        return Result::not_found;
    }
    std::vector<std::string> tires;
    if (ordering(resources, "Cars/Shared/Tires", tires, error) != Result::ok)
        return Result::invalid_argument;
    bool valid = false;
    for (const auto& tire : tires)
        valid |= ResourceStore::canonical(tire) == ResourceStore::canonical(result.tire_type_);
    if (!valid) {
        error = "Original track selects an unknown tire type: " + result.tire_type_;
        return Result::invalid_argument;
    }
    *this = std::move(result);
    error.clear();
    return Result::ok;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004a55a9
// Selection-only fixture API; live callers use synchronous installers below.
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0041c3d9
Result RaceLoadoutTable::assign(std::uint32_t& state, RaceLoadout& output,
                                std::string& error) const {
    return assign(state, output, RaceLoadoutInstallers{}, error);
}
Result RaceLoadoutTable::assign(std::uint32_t& state, RaceLoadout& output,
                                const RaceLoadoutInstallers& install, std::string& error) const {
    if (tire_type_.empty() || armor_.empty() || drivetrain_.empty()) {
        error = "Original race loadout table is not loaded";
        return Result::unavailable;
    }
    RaceLoadout result;
    result.paint_job = static_cast<int>(original_random(state) % 6) + 1;
    if (install.paint && install.paint(result.paint_job) != Result::ok)
        return Result::unavailable;
    result.tires = {tire_type_, "BEST"};
    if (install.tires && install.tires(result.tires) != Result::ok)
        return Result::unavailable;
    for (std::size_t slot = 0; slot < weapons_.size(); ++slot) {
        result.weapons[slot] = weapons_[slot][original_random(state) % weapons_[slot].size()];
        if (install.weapon && install.weapon(slot, result.weapons[slot]) != Result::ok)
            return Result::unavailable;
    }
    const auto& armor = armor_[original_random(state) % armor_.size()];
    result.armor = {armor.type, armor.quality[original_random(state) % armor.quality.size()]};
    if (install.armor && install.armor(result.armor) != Result::ok)
        return Result::unavailable;
    for (const auto& choice : drivetrain_) {
        result.drivetrain.push_back(
            {choice.type, choice.quality[original_random(state) % choice.quality.size()]});
        if (install.drivetrain && install.drivetrain(result.drivetrain.back()) != Result::ok)
            return Result::unavailable;
    }
    bool nitrous = false;
    for (auto& choice : result.drivetrain)
        if (ResourceStore::canonical(choice.type) == "NITROUS BOOST") {
            choice.quality = "BEST";
            nitrous = true;
            if (install.drivetrain && install.drivetrain(choice) != Result::ok)
                return Result::unavailable;
        }
    if (!nitrous) {
        error = "Original drivetrain ordering has no NITROUS BOOST";
        return Result::unavailable;
    }
    output = std::move(result);
    error.clear();
    return Result::ok;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00412834
Result load_wheel_scale(ResourceStore& resources, std::string_view vehicle,
                        const TireSelection& selection, float& scale, std::string& error) {
    Configuration config;
    const auto path = "Cars/" + std::string(vehicle) + "/Tires/" + selection.type + "/" +
                      selection.quality + "/Data.ini";
    float value{};
    if (config.load(resources, path) != Result::ok ||
        config.number("SCALE", "SCALEX", value) != Result::ok || value <= 0) {
        error = "Original tire scale unavailable: " + path;
        return Result::invalid_argument;
    }
    scale = value;
    error.clear();
    return Result::ok;
}
} // namespace roadwars
