#pragma once
#include "game_src/unresolved/resource_store.hpp"
#include <functional>

namespace roadwars {
// Portable value representation of vehicle-owned equipment. Original class names unresolved.
struct TireSelection {
    std::string type{"Street"}, quality{"Poor"};
};
struct EquipmentSelection {
    std::string type, quality;
};
struct RaceLoadout {
    TireSelection tires;
    std::array<std::string, 3> weapons;
    EquipmentSelection armor;
    std::vector<EquipmentSelection> drivetrain;
    int paint_job{1};
};
// Original 005170b0 recurrence; the caller owns the startup seed and shared stream.
std::uint32_t original_random(std::uint32_t& state);
// Original character selection completes before any new vehicle initializes.
Result select_race_characters(std::uint32_t& state,
                              std::span<const std::string> available_opponents,
                              std::string_view player, std::size_t player_slot,
                              std::size_t slot_count, std::vector<std::string>& output,
                              std::string& error);
bool original_randomizes_loadout(std::size_t slot, std::size_t player_slot,
                                 bool force_player_configuration, bool race_eec);
struct RaceLoadoutInstallers {
    std::function<Result(int paint_job)> paint;
    std::function<Result(const TireSelection&)> tires;
    std::function<Result(std::size_t slot, std::string_view weapon)> weapon;
    std::function<Result(const EquipmentSelection&)> armor, drivetrain;
};
class RaceLoadoutTable {
  public:
    Result load(ResourceStore&, std::string_view track, std::string& error);
    Result assign(std::uint32_t& random_state, RaceLoadout&, std::string& error) const;
    // Synchronous side effects preserve factory-driven shared RNG between selection draws.
    Result assign(std::uint32_t& random_state, RaceLoadout&, const RaceLoadoutInstallers&,
                  std::string& error) const;
    const std::array<std::vector<std::string>, 3>& weapons() const { return weapons_; }
    TireSelection tires() const { return {tire_type_, "BEST"}; }

  private:
    std::array<std::vector<std::string>, 3> weapons_;
    struct Choices {
        std::string type;
        std::vector<std::string> quality;
    };
    std::vector<Choices> armor_, drivetrain_;
    std::string tire_type_;
};
Result load_wheel_scale(ResourceStore&, std::string_view vehicle, const TireSelection&,
                        float& scale, std::string& error);
} // namespace roadwars
