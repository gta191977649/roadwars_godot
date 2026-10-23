#pragma once
#include "game_src/unresolved/race_loadout.hpp"
#include "game_src/unresolved/vehicle_equipment_effects.hpp"

namespace roadwars {
enum class GarageCategory { tires, drivetrain, weapons, armor, repair, paint };
enum class GarageAction { unavailable, buy, sell, installed, repair };

struct GarageProduct {
    std::string name, description, advice;
    std::int32_t base_price{}, ammunition{};
    bool available{true};
};
struct GarageItem {
    std::string name;
    // An unqualified item (paint) has one product with an empty name.
    std::vector<GarageProduct> products;
};
struct GarageVehicleState {
    std::string character, vehicle;
    std::int32_t cash{25000};
    RaceLoadout loadout;
    VehicleEquipmentEffects condition;
    std::array<std::int32_t, 3> weapon_ammunition{};
};
struct GarageSelection {
    GarageCategory category{};
    std::size_t item{}, product{};
};
struct GarageQuote {
    GarageAction action{GarageAction::unavailable};
    std::int32_t price{}, trade_in{}, ammunition{};
    std::int64_t balance_change{};
    bool affordable{};
    std::vector<std::string> confirmations;
    std::string description, advice;
};

// New shared coordinator; recovered rules retain address-linked evidence.
// Season can restore a vehicle state without depending on menu or rendering owners.
class GarageSession {
  public:
    explicit GarageSession(ResourceStore& resources) : resources_(resources) {}
    Result start_single(std::string_view character, std::string_view track, std::string& error);
    Result restore(GarageVehicleState, std::string_view track, std::string& error);
    const GarageVehicleState& state() const { return state_; }
    std::span<const GarageItem> items(GarageCategory category) const;
    GarageSelection initial_selection(GarageCategory) const;
    Result quote(GarageSelection, GarageQuote&, std::string& error) const;
    // The caller presents every quote confirmation before passing confirmed=true.
    Result transact(GarageSelection, bool confirmed, bool& changed, std::string& error);
    RaceLoadout race_loadout() const { return state_.loadout; }
    std::vector<std::string> take_sound_events();

  private:
    Result load_catalog(std::string& error);
    Result price(GarageSelection, bool ignore_installed, std::int32_t&, std::string&) const;
    Result resale(GarageSelection, std::int32_t&, std::string&) const;
    Result repair_price(GarageSelection, std::int32_t&, std::string&) const;
    std::optional<GarageSelection> installed(GarageSelection) const;
    const GarageProduct* product(GarageSelection) const;
    float repair_fraction(GarageSelection) const;
    std::vector<std::string> tire_confirmations(std::int64_t expense,
                                                GarageSelection) const;
    ResourceStore& resources_;
    GarageVehicleState state_;
    std::string track_, track_tires_;
    std::array<std::vector<GarageItem>, 6> catalog_;
    std::array<float, 6> price_scalars_{};
    float spikes_scalar_{1};
    std::vector<std::string> sounds_;
};
} // namespace roadwars
