#include "game_src/unresolved/garage_session.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace roadwars {
namespace {
constexpr std::array<std::string_view, 6> kDirectories{"Tires", "Drive Train", "Weapons",
                                                       "Armor", "Repair",      "Paint Job"};
constexpr std::array<std::string_view, 6> kScalars{"TIRES", "DRIVETRAIN", "WEAPONS",
                                                   "ARMOR", "REPAIR",     ""};
std::string key(std::string_view text) { return ResourceStore::canonical(text); }
bool equal(std::string_view a, std::string_view b) { return key(a) == key(b); }
bool safe_name(std::string_view text) {
    return !text.empty() && text.find_first_of("/\\:") == std::string_view::npos &&
           text.find("..") == std::string_view::npos;
}
bool integer(const Configuration& config, std::string_view section, std::string_view name,
             std::int32_t& value) {
    float number{};
    if (config.number(section, name, number) != Result::ok || !std::isfinite(number) ||
        number < 0 || static_cast<double>(number) > std::numeric_limits<std::int32_t>::max() ||
        std::floor(number) != number)
        return false;
    value = static_cast<std::int32_t>(number);
    return true;
}
bool truncate(double value, std::int32_t& output) {
    if (!std::isfinite(value) || value < std::numeric_limits<std::int32_t>::min() ||
        value > std::numeric_limits<std::int32_t>::max())
        return false;
    output = static_cast<std::int32_t>(value);
    return true;
}
int weapon_slot(std::string_view name) {
    return equal(name, "FRONT") ? 0 : equal(name, "SIDE") ? 1 : equal(name, "REAR") ? 2 : -1;
}
void substitute_ammo(std::string& value, int ammunition) {
    std::size_t offset{};
    while ((offset = value.find("#ammo", offset)) != std::string::npos) {
        const auto replacement = std::to_string(ammunition);
        value.replace(offset, 5, replacement);
        offset += replacement.size();
    }
}
} // namespace

// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0044a0f0
Result GarageSession::load_catalog(std::string& error) {
    Configuration config;
    if (config.load(resources_, "Characters/" + state_.character + "/Data.ini") != Result::ok) {
        error = "Original garage character configuration unavailable";
        return Result::not_found;
    }
    for (std::size_t i = 0; i < price_scalars_.size(); ++i) {
        price_scalars_[i] = 1;
        if (!kScalars[i].empty() &&
            (config.number("PRICE_SCALARS", kScalars[i], price_scalars_[i]) != Result::ok ||
             !std::isfinite(price_scalars_[i]) || price_scalars_[i] < 0)) {
            error = "Invalid garage character price scalar";
            return Result::invalid_argument;
        }
    }
    if (config.number("PRICE_SCALARS", "SPIKES", spikes_scalar_) != Result::ok ||
        !std::isfinite(spikes_scalar_) || spikes_scalar_ < 0) {
        error = "Invalid garage contact weapon price scalar";
        return Result::invalid_argument;
    }
    if (config.load(resources_, "Tracks/" + track_ + "/World/Data.txt") != Result::ok ||
        (track_tires_ = config.value("AI", "TIRES")).empty()) {
        error = "Original garage track tire recommendation unavailable";
        return Result::not_found;
    }
    std::array<std::vector<GarageItem>, 6> catalog;
    for (std::size_t category = 0; category < catalog.size(); ++category) {
        const auto base = "Cars/Shared/" + std::string(kDirectories[category]);
        if (config.load(resources_, base + "/Data.ini") != Result::ok) {
            error = "Original garage category unavailable: " + base;
            return Result::not_found;
        }
        std::int32_t count{};
        if (!integer(config, "ORDERING", "NUMITEMS", count) || count < 1 || count > 256) {
            error = "Invalid garage item count";
            return Result::invalid_argument;
        }
        for (int i = 0; i < count; ++i) {
            GarageItem item;
            item.name = config.value("ORDERING", "ITEM" + std::to_string(i));
            if (!safe_name(item.name)) {
                error = "Invalid original garage item name";
                return Result::invalid_argument;
            }
            Configuration item_config;
            const auto item_path = base + "/" + item.name;
            if (item_config.load(resources_, item_path + "/Data.ini") != Result::ok) {
                error = "Original garage item unavailable: " + item_path;
                return Result::not_found;
            }
            std::int32_t qualities{};
            if (!item_config.value("ORDERING", "NUMITEMS").empty() &&
                (!integer(item_config, "ORDERING", "NUMITEMS", qualities) || qualities > 256)) {
                error = "Invalid garage quality count";
                return Result::invalid_argument;
            }
            for (int quality = 0; quality < std::max(qualities, 1); ++quality) {
                GarageProduct product;
                Configuration product_config = item_config;
                if (qualities) {
                    product.name = item_config.value("ORDERING", "ITEM" + std::to_string(quality));
                    if (!safe_name(product.name) ||
                        product_config.load(resources_, item_path + "/" + product.name +
                                                            "/Data.ini") != Result::ok) {
                        error = "Original garage quality unavailable";
                        return Result::not_found;
                    }
                }
                if (!integer(product_config, "PRICE", "PRICE($)", product.base_price)) {
                    error = "Original garage price unavailable: " + item_path;
                    return Result::invalid_argument;
                }
                std::int32_t available{1};
                if (!product_config.value("PRICE", "ISAVAILABLE").empty() &&
                    !integer(product_config, "PRICE", "ISAVAILABLE", available)) {
                    error = "Invalid garage product availability";
                    return Result::invalid_argument;
                }
                product.available = available != 0;
                if (!product_config.value("PRICE", "DEFAULTAMMO").empty() &&
                    !integer(product_config, "PRICE", "DEFAULTAMMO", product.ammunition)) {
                    error = "Invalid garage ammunition count";
                    return Result::invalid_argument;
                }
                product.description = product_config.value("DESCRIPTION", "TEXT");
                product.advice = product_config.value("ADVICE", "TEXT");
                substitute_ammo(product.description, product.ammunition);
                substitute_ammo(product.advice, product.ammunition);
                item.products.push_back(std::move(product));
            }
            catalog[category].push_back(std::move(item));
        }
    }
    catalog_ = std::move(catalog);
    return Result::ok;
}

Result GarageSession::start_single(std::string_view character, std::string_view track,
                                   std::string& error) {
    if (!safe_name(character) || !safe_name(track))
        return Result::invalid_argument;
    GarageVehicleState state;
    state.character = character;
    Configuration config;
    if (config.load(resources_, "Characters/" + state.character + "/Data.ini") != Result::ok ||
        !safe_name(state.vehicle = config.value("INFO", "VEHICLE"))) {
        error = "Original garage vehicle identity unavailable";
        return Result::not_found;
    }
    state.loadout.paint_job = 0;
    state.loadout.armor = {"NONE", "NONE"};
    if (load_vehicle_equipment_effects(resources_, state.character, state.vehicle, "NONE", "NONE",
                                       {}, {}, state.condition, error) != Result::ok)
        return Result::unavailable;
    return restore(std::move(state), track, error);
}

Result GarageSession::restore(GarageVehicleState state, std::string_view track,
                              std::string& error) {
    if (!safe_name(state.character) || !safe_name(state.vehicle) || !safe_name(track) ||
        state.cash < 0 || !std::isfinite(state.condition.internal_hp) ||
        state.condition.internal_hp < 0 || !std::isfinite(state.condition.maximum_internal_hp) ||
        state.condition.maximum_internal_hp <= 0 ||
        state.condition.internal_hp > state.condition.maximum_internal_hp)
        return Result::invalid_argument;
    Configuration character;
    if (character.load(resources_, "Characters/" + state.character + "/Data.ini") != Result::ok ||
        !equal(character.value("INFO", "VEHICLE"), state.vehicle)) {
        error = "Garage vehicle identity does not match the selected character";
        return Result::invalid_argument;
    }
    for (std::size_t i = 0; i < 4; ++i)
        if (!std::isfinite(state.condition.armor_hp[i]) ||
            !std::isfinite(state.condition.maximum_armor_hp[i]) ||
            state.condition.maximum_armor_hp[i] <= 0 ||
            state.condition.armor_hp[i] > state.condition.maximum_armor_hp[i] ||
            !std::isfinite(state.condition.base_armor_hp[i]) ||
            state.condition.base_armor_hp[i] <= 0)
            return Result::invalid_argument;
    GarageSession next(resources_);
    next.state_ = std::move(state);
    next.track_ = track;
    if (next.load_catalog(error) != Result::ok)
        return Result::unavailable;
    // Every installed name must resolve to this game's catalog; restore is transactional.
    const auto known = [&](GarageCategory category, std::string_view type,
                           std::string_view quality) {
        const auto list = next.items(category);
        for (const auto& item : list)
            if (equal(item.name, type))
                for (const auto& value : item.products)
                    if (equal(value.name, quality))
                        return true;
        return false;
    };
    if (!known(GarageCategory::tires, next.state_.loadout.tires.type,
               next.state_.loadout.tires.quality) ||
        (!next.state_.loadout.armor.type.empty() &&
         !equal(next.state_.loadout.armor.type, "NONE") &&
         !known(GarageCategory::armor, next.state_.loadout.armor.type,
                next.state_.loadout.armor.quality)) ||
        next.state_.loadout.paint_job < 0 ||
        next.state_.loadout.paint_job >
            static_cast<int>(next.items(GarageCategory::paint).size())) {
        error = "Garage state contains unknown equipment";
        return Result::invalid_argument;
    }
    std::vector<std::string> categories;
    for (const auto& entry : next.state_.loadout.drivetrain) {
        if (!known(GarageCategory::drivetrain, entry.type, entry.quality) ||
            std::find(categories.begin(), categories.end(), key(entry.type)) != categories.end()) {
            error = "Garage state contains an unknown or duplicate drivetrain component";
            return Result::invalid_argument;
        }
        categories.push_back(key(entry.type));
    }
    constexpr std::array<std::string_view, 3> slots{"FRONT", "SIDE", "REAR"};
    for (std::size_t i = 0; i < slots.size(); ++i)
        if ((!next.state_.loadout.weapons[i].empty() &&
             !known(GarageCategory::weapons, slots[i], next.state_.loadout.weapons[i])) ||
            next.state_.weapon_ammunition[i] < 0) {
            error = "Garage state contains an invalid weapon or ammunition count";
            return Result::invalid_argument;
        }
    if (next.state_.visuals) {
        VehicleDamageVisuals visuals;
        if (!std::isfinite(next.state_.body_width) || next.state_.body_width <= 0 ||
            !std::isfinite(next.state_.body_length) || next.state_.body_length <= 0 ||
            visuals.load(resources_, next.state_.vehicle, next.state_.loadout.paint_job, error) !=
                Result::ok ||
            visuals.restore_state(*next.state_.visuals) != Result::ok) {
            error = "Garage visual snapshot does not match the selected vehicle and paint";
            return Result::invalid_argument;
        }
    }
    state_ = std::move(next.state_);
    track_ = std::move(next.track_);
    track_tires_ = std::move(next.track_tires_);
    catalog_ = std::move(next.catalog_);
    price_scalars_ = next.price_scalars_;
    spikes_scalar_ = next.spikes_scalar_;
    sounds_.clear();
    error.clear();
    return Result::ok;
}

std::span<const GarageItem> GarageSession::items(GarageCategory category) const {
    const auto index = static_cast<std::size_t>(category);
    return index < catalog_.size() ? std::span<const GarageItem>(catalog_[index])
                                   : std::span<const GarageItem>{};
}
const GarageProduct* GarageSession::product(GarageSelection selection) const {
    const auto list = items(selection.category);
    return selection.item < list.size() && selection.product < list[selection.item].products.size()
               ? &list[selection.item].products[selection.product]
               : nullptr;
}
std::optional<GarageSelection> GarageSession::installed(GarageSelection selection) const {
    const auto list = items(selection.category);
    if (selection.item >= list.size())
        return {};
    std::string type, quality;
    switch (selection.category) {
    case GarageCategory::tires:
        type = state_.loadout.tires.type;
        quality = state_.loadout.tires.quality;
        break;
    case GarageCategory::armor:
        type = state_.loadout.armor.type;
        quality = state_.loadout.armor.quality;
        break;
    case GarageCategory::drivetrain:
        for (const auto& entry : state_.loadout.drivetrain)
            if (equal(entry.type, list[selection.item].name)) {
                type = entry.type;
                quality = entry.quality;
                break;
            }
        break;
    case GarageCategory::weapons: {
        const auto slot = weapon_slot(list[selection.item].name);
        if (slot >= 0) {
            type = list[selection.item].name;
            quality = state_.loadout.weapons[static_cast<std::size_t>(slot)];
        }
        break;
    }
    case GarageCategory::paint:
        if (state_.loadout.paint_job > 0)
            type = "PAINT JOB " + std::to_string(state_.loadout.paint_job);
        break;
    case GarageCategory::repair:
        return {};
    }
    if (type.empty() || equal(type, "NONE"))
        return {};
    for (std::size_t i = 0; i < list.size(); ++i)
        if (equal(list[i].name, type))
            for (std::size_t j = 0; j < list[i].products.size(); ++j)
                if (equal(list[i].products[j].name, quality))
                    return GarageSelection{selection.category, i, j};
    return {};
}
GarageSelection GarageSession::initial_selection(GarageCategory category) const {
    GarageSelection selection{category};
    if (const auto found = installed(selection))
        selection = *found;
    return selection;
}

// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0040fff0
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0041050c
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00410c25
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004124f4
Result GarageSession::price(GarageSelection selection, bool ignore_installed, std::int32_t& output,
                            std::string& error) const {
    const auto* value = product(selection);
    if (!value)
        return Result::invalid_argument;
    const auto current = installed(selection);
    if (!ignore_installed && current && current->item == selection.item &&
        current->product == selection.product) {
        output = -1;
        return Result::ok;
    }
    if (selection.category == GarageCategory::repair)
        return repair_price(selection, output, error);
    double amount =
        value->base_price *
        static_cast<double>(price_scalars_[static_cast<std::size_t>(selection.category)]);
    if (selection.category == GarageCategory::weapons &&
        (equal(value->name, "BATTERING SPIKES") || equal(value->name, "HYDRAULIC RAM")))
        amount *= spikes_scalar_;
    if (!truncate(amount, output)) {
        error = "Original garage price exceeds the portable integer boundary";
        return Result::invalid_argument;
    }
    return Result::ok;
}

// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0041004b
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004105e3
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00410568
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00410cf4
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00412550
Result GarageSession::resale(GarageSelection selection, std::int32_t& output,
                             std::string& error) const {
    output = 0;
    const auto current = installed(selection);
    if (!current)
        return Result::ok;
    std::int32_t original{};
    if (price(*current, true, original, error) != Result::ok)
        return Result::invalid_argument;
    original = static_cast<std::int32_t>(original * .5);
    if (selection.category == GarageCategory::armor) {
        float total{}, remaining{};
        for (std::size_t i = 0; i < 4; ++i) {
            total += state_.condition.maximum_armor_hp[i];
            remaining += state_.condition.armor_hp[i];
        }
        const float fraction = remaining / total;
        if (!truncate(original * static_cast<double>(fraction) + .5, output))
            return Result::invalid_argument;
    } else
        output = original;
    return Result::ok;
}

float GarageSession::repair_fraction(GarageSelection selection) const {
    const auto* value = product(selection);
    if (!value)
        return 0;
    return equal(value->name, "MINOR")      ? .25f
           : equal(value->name, "MEDIUM")   ? .5f
           : equal(value->name, "MAJOR")    ? .75f
           : equal(value->name, "COMPLETE") ? 1.0f
                                            : 0.0f;
}

// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00415008
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00421e70
Result GarageSession::repair_price(GarageSelection selection, std::int32_t& output,
                                   std::string& error) const {
    const auto list = items(selection.category);
    const float fraction = repair_fraction(selection);
    if (selection.item >= list.size() || fraction == 0)
        return Result::invalid_argument;
    const auto name = key(list[selection.item].name);
    constexpr std::array<std::string_view, 6> regions{"ALL DAMAGE",   "INTERNAL DAMAGE",
                                                      "FRONT DAMAGE", "RIGHT DAMAGE",
                                                      "REAR DAMAGE",  "LEFT DAMAGE"};
    if (std::find(regions.begin(), regions.end(), name) == regions.end()) {
        error = "Unsupported original repair region";
        return Result::unavailable;
    }
    float health{};
    if (name == "ALL DAMAGE" || name == "INTERNAL DAMAGE")
        health = (state_.condition.maximum_internal_hp - state_.condition.internal_hp) * fraction;
    constexpr std::array<std::string_view, 4> sides{"FRONT DAMAGE", "RIGHT DAMAGE", "REAR DAMAGE",
                                                    "LEFT DAMAGE"};
    for (std::size_t i = 0; i < sides.size(); ++i)
        if (name == "ALL DAMAGE" || name == sides[i])
            health +=
                (state_.condition.maximum_armor_hp[i] - state_.condition.armor_hp[i]) * fraction;
    std::int32_t unit{};
    if (!truncate(price_scalars_[4] * 40.0, unit) ||
        !truncate(unit * static_cast<double>(health) + .5, output)) {
        error = "Original repair price is outside the supported boundary";
        return Result::invalid_argument;
    }
    return Result::ok;
}

Result GarageSession::quote(GarageSelection selection, GarageQuote& output,
                            std::string& error) const {
    error.clear();
    const auto* value = product(selection);
    if (!value)
        return Result::invalid_argument;
    GarageQuote quote;
    quote.description = value->description;
    quote.advice = value->advice;
    quote.ammunition = value->ammunition;
    if (price(selection, false, quote.price, error) != Result::ok ||
        resale(selection, quote.trade_in, error) != Result::ok)
        return Result::invalid_argument;
    const auto current = installed(selection);
    const bool same =
        current && current->item == selection.item && current->product == selection.product;
    quote.affordable = static_cast<std::int64_t>(state_.cash) + quote.trade_in >= quote.price;
    if (!value->available)
        quote.action = GarageAction::unavailable;
    else if (same) {
        if (selection.category == GarageCategory::tires ||
            selection.category == GarageCategory::paint)
            quote.action = GarageAction::installed;
        else {
            quote.action = GarageAction::sell;
            const std::string item = selection.category == GarageCategory::weapons ? "WEAPON"
                                     : selection.category == GarageCategory::armor ? "ARMOR"
                                                                                   : "UPGRADE";
            quote.confirmations.push_back("ARE YOU SURE YOU WANT TO SELL BACK YOUR " + item + "?");
        }
    } else if (selection.category == GarageCategory::repair)
        quote.action = GarageAction::repair;
    else {
        quote.action = GarageAction::buy;
        if (current) {
            std::int32_t previous{};
            if (price(*current, true, previous, error) != Result::ok)
                return Result::invalid_argument;
            if (selection.category == GarageCategory::weapons)
                quote.confirmations.push_back("ARE YOU SURE YOU WANT TO CHANGE YOUR " +
                                              key(items(selection.category)[selection.item].name) +
                                              " WEAPON?");
            else if (selection.category != GarageCategory::paint && quote.price < previous &&
                     (selection.category != GarageCategory::tires ||
                      current->item == selection.item))
                quote.confirmations.push_back(
                    "ARE YOU SURE YOU WANT TO DOWNGRADE YOUR " +
                    std::string(selection.category == GarageCategory::tires   ? "TIRES"
                                : selection.category == GarageCategory::armor ? "ARMOR"
                                                                              : "UPGRADE") +
                    "?");
        }
    }
    quote.balance_change =
        quote.action == GarageAction::sell ? quote.trade_in
        : quote.action == GarageAction::buy || quote.action == GarageAction::repair
            ? static_cast<std::int64_t>(quote.trade_in) - quote.price
            : 0;
    const auto warnings = tire_confirmations(-quote.balance_change, selection);
    quote.confirmations.insert(quote.confirmations.begin(), warnings.begin(), warnings.end());
    output = std::move(quote);
    return Result::ok;
}

// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00437bf6
std::vector<std::string> GarageSession::tire_confirmations(std::int64_t expense,
                                                           GarageSelection selection) const {
    if (expense <= 0)
        return {};
    const auto list = items(selection.category);
    if (selection.category == GarageCategory::tires) {
        if (!equal(list[selection.item].name, track_tires_))
            return {"THESE TIRES DON'T MATCH THE TIRE TYPE FOR THE CURRENT TRACK. CONTINUE WITH "
                    "PURCHASE?"};
        return {};
    }
    if (equal(list[selection.item].name, track_tires_))
        return {};
    const auto tires = items(GarageCategory::tires);
    for (std::size_t i = 0; i < tires.size(); ++i) {
        if (!equal(tires[i].name, track_tires_))
            continue;
        const auto& products = tires[i].products;
        if (products.empty())
            break;
        // 00437972 caches raw asset prices through 00404490, without character discounts.
        const auto best = products.back().base_price;
        const auto cheapest = products.front().base_price;
        if (!equal(state_.loadout.tires.type, track_tires_) && state_.cash >= cheapest &&
            state_.cash - expense < best)
            return {"YOU HAVN'T PURCHASED THE PROPER TIRES FOR THIS TRACK. ARE YOU SURE?"};
        if (state_.cash >= best && state_.cash - expense < best &&
            !equal(state_.loadout.tires.quality, products.back().name))
            return {"YOU HAVN'T PURCHASED THE BEST TIRES FOR THIS TRACK. ARE YOU SURE?"};
        break;
    }
    return {};
}

// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00441a99
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0041018e
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00410757
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00410e37
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0041522c
Result GarageSession::transact(GarageSelection selection, bool confirmed, bool& changed,
                               std::string& error) {
    error.clear();
    changed = false;
    GarageQuote current;
    if (quote(selection, current, error) != Result::ok)
        return Result::invalid_argument;
    if (current.action == GarageAction::unavailable || current.action == GarageAction::installed) {
        sounds_.push_back("Sfx/notavail.wav");
        return Result::ok;
    }
    if (!current.affordable) {
        error = "YOU DO NOT HAVE ENOUGH MONEY!";
        return Result::ok;
    }
    if (!current.confirmations.empty() && !confirmed)
        return Result::ok;
    const auto cash = state_.cash + current.balance_change;
    if (cash < 0 || cash > std::numeric_limits<std::int32_t>::max())
        return Result::invalid_argument;
    auto next = state_;
    bool repaired_visuals{};
    const auto& item = items(selection.category)[selection.item];
    const auto* value = product(selection);
    const bool sell = current.action == GarageAction::sell;
    switch (selection.category) {
    case GarageCategory::tires:
        next.loadout.tires = {item.name, value->name};
        break;
    case GarageCategory::drivetrain: {
        auto& drive = next.loadout.drivetrain;
        drive.erase(std::remove_if(drive.begin(), drive.end(),
                                   [&](const auto& entry) { return equal(entry.type, item.name); }),
                    drive.end());
        if (!sell)
            drive.push_back({item.name, value->name});
        std::int32_t bonus{};
        if (drivetrain_upgrade(item.name, value->name, bonus) != Result::ok)
            return Result::unavailable;
        constexpr std::array<std::string_view, 7> categories{
            "AIR FILTER",      "CAM SHAFT",     "CYLINDER HEADS", "EXHAUST",
            "INTAKE MANIFOLD", "SUPER CHARGER", "NITROUS BOOST"};
        const auto found = std::find(categories.begin(), categories.end(), key(item.name));
        if (found == categories.end())
            return Result::invalid_argument;
        next.condition.engine_upgrades[static_cast<std::size_t>(found - categories.begin())] =
            sell ? 0 : bonus;
        if (equal(item.name, "NITROUS BOOST"))
            next.condition.nitrous_ammo = sell ? 0 : value->ammunition;
        break;
    }
    case GarageCategory::weapons: {
        const auto slot = weapon_slot(item.name);
        if (slot < 0)
            return Result::invalid_argument;
        next.loadout.weapons[slot] = sell ? "" : value->name;
        next.weapon_ammunition[slot] = sell ? 0 : value->ammunition;
        next.condition.front_weapon = next.loadout.weapons[0];
        break;
    }
    case GarageCategory::armor:
        next.loadout.armor =
            sell ? EquipmentSelection{"NONE", "NONE"} : EquipmentSelection{item.name, value->name};
        // 00410853 first sells the old armor; 00410757 restores the stock HP.
        next.condition.maximum_armor_hp = next.condition.armor_hp = next.condition.base_armor_hp;
        if (!sell) {
            Configuration config;
            float scale{};
            if (config.load(resources_, "Cars/Shared/Armor/" + item.name + "/" + value->name +
                                            "/Data.ini") != Result::ok ||
                config.number("HP", "HPSCALE", scale) != Result::ok ||
                rescale_vehicle_armor(next.condition, scale) != Result::ok) {
                error = "Original purchased armor configuration unavailable";
                return Result::unavailable;
            }
        }
        break;
    case GarageCategory::repair: {
        const auto name = key(item.name);
        const float fraction = repair_fraction(selection);
        if (name == "ALL DAMAGE" || name == "INTERNAL DAMAGE")
            next.condition.internal_hp +=
                (next.condition.maximum_internal_hp - next.condition.internal_hp) * fraction;
        constexpr std::array<std::string_view, 4> sides{"FRONT DAMAGE", "RIGHT DAMAGE",
                                                        "REAR DAMAGE", "LEFT DAMAGE"};
        for (std::size_t i = 0; i < 4; ++i)
            if (name == "ALL DAMAGE" || name == sides[i])
                next.condition.armor_hp[i] +=
                    (next.condition.maximum_armor_hp[i] - next.condition.armor_hp[i]) * fraction;
        if (next.visuals) {
            VehicleDamageVisuals visual;
            constexpr std::array<std::string_view, 6> regions{"ALL DAMAGE",   "INTERNAL DAMAGE",
                                                              "FRONT DAMAGE", "RIGHT DAMAGE",
                                                              "REAR DAMAGE",  "LEFT DAMAGE"};
            const auto region = std::find(regions.begin(), regions.end(), name);
            if (region == regions.end() ||
                visual.load(resources_, next.vehicle, next.loadout.paint_job, error) !=
                    Result::ok ||
                visual.restore_state(*next.visuals) != Result::ok) {
                error = "Original vehicle visual repair failed";
                return Result::unavailable;
            }
            const auto revision = visual.revision();
            if (visual.repair(static_cast<VehicleRepairRegion>(region - regions.begin()), fraction,
                              next.body_width, next.body_length) != Result::ok) {
                error = "Original vehicle visual repair failed";
                return Result::unavailable;
            }
            repaired_visuals = visual.revision() != revision;
            next.visuals = visual.capture_state();
        }
        break;
    }
    case GarageCategory::paint:
        // The original paint operation changes resources but does not debit cash.
        next.loadout.paint_job = static_cast<int>(selection.item + 1);
        if (next.visuals) {
            VehicleDamageVisuals visual;
            if (visual.load(resources_, next.vehicle, next.loadout.paint_job, error) !=
                    Result::ok ||
                visual.repaint_damage_from(*next.visuals) != Result::ok) {
                error = "Original vehicle repaint failed";
                return Result::unavailable;
            }
            next.visuals = visual.capture_state();
        }
        break;
    }
    next.cash = static_cast<std::int32_t>(cash);
    const bool repaired_health = next.condition.internal_hp != state_.condition.internal_hp ||
                                 next.condition.armor_hp != state_.condition.armor_hp;
    const bool repair = selection.category == GarageCategory::repair;
    changed = !repair || repaired_health || repaired_visuals;
    state_ = std::move(next);
    // 0041522c always repairs visuals, but its return value only reports restored HP.
    if (!repair || repaired_health)
        sounds_.push_back("ModifyVehicle/buy.wav");
    error.clear();
    return Result::ok;
}
std::vector<std::string> GarageSession::take_sound_events() {
    std::vector<std::string> result;
    result.swap(sounds_);
    return result;
}
} // namespace roadwars
