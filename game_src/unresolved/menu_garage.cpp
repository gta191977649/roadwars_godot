#include "game_src/unresolved/frontend_dialog.hpp"
#include "game_src/unresolved/frontend_scene.hpp"
#include "game_src/unresolved/menu_frontend.hpp"
#include <algorithm>
#include <cmath>

namespace roadwars {
namespace {
constexpr std::array<std::string_view, 6> kVariants{"tires", "drive_train", "weapons",
                                                    "armor", "repair",      "paint"};
constexpr std::array<std::string_view, 6> kStoreDirectories{"Tires", "Drive Train", "Weapons",
                                                            "Armor", "Repair",      "Paint Job"};
// 0045fee8 groups display prices in threes; trade-in footer uses a separate itoa path.
std::string display_price(std::int32_t value) {
    auto text = std::to_string(value);
    const auto start = !text.empty() && text.front() == '-' ? 1u : 0u;
    for (std::size_t i = text.size(); i > start + 3;) {
        i -= 3;
        text.insert(i, ",");
    }
    return text;
}
} // namespace

Result MenuFrontend::refresh_garage(bool select_initial, std::string& error) {
    if (!garage_)
        return Result::unavailable;
    const auto& state = garage_->state();
    custom_.preview_data = false;
    custom_.fields["CHARACTER"] = state.character;
    custom_.racers = {{state.character, state.character, 0, state.cash, true, {}}};
    custom_.fields["TIRES"] = translate("Garage", state.loadout.tires.type);
    custom_.fields["TIRES_TYPE"] = state.loadout.tires.type;
    custom_.fields["TIRES_QUALITY"] = state.loadout.tires.quality;
    custom_.fields["ARMOR"] =
        translate("Garage", state.loadout.armor.type.empty() ? "NONE" : state.loadout.armor.type);
    custom_.fields["ARMOR_TYPE"] = state.loadout.armor.type;
    custom_.fields["ARMOR_QUALITY"] = state.loadout.armor.quality;
    custom_.fields["DRIVE TRAIN"] = translate("Garage", "DRIVE TRAIN");
    custom_.fields["DRIVE_LABEL"] = translate("Garage", "DRIVE TRAIN");
    custom_.fields["BOOST_LABEL"] = translate("Garage", "BOOST");
    custom_.fields["INTERNAL_LABEL"] = translate("Garage", "INTERNAL");
    custom_.fields["CURRENT_TRACK_LABEL"] = translate("Garage", "CURRENT TRACK");
    custom_.fields["TIRES_LABEL"] = translate("Garage", "TIRES");
    float drive_ratio{};
    for (const auto& entry : state.loadout.drivetrain) {
        const auto quality = ResourceStore::canonical(entry.quality);
        drive_ratio += quality == "BEST"   ? .1428571492433548f
                       : quality == "GOOD" ? .09571428596973419f
                       : quality == "POOR" ? .04714285954833031f
                                           : 0;
    }
    custom_.fields["DRIVE_RATIO"] = std::to_string(drive_ratio);
    custom_.fields["NITROUS_AMMO"] = std::to_string(state.condition.nitrous_ammo);
    int maximum_nitrous{};
    // 00438393 caches Best/DEFAULTAMMO for every installed quality's display bar.
    for (const auto& item : garage_->items(GarageCategory::drivetrain))
        if (ResourceStore::canonical(item.name) == "NITROUS BOOST")
            for (const auto& product : item.products)
                if (ResourceStore::canonical(product.name) == "BEST")
                    maximum_nitrous = product.ammunition;
    custom_.fields["NITROUS_RATIO"] = std::to_string(
        maximum_nitrous > 0 ? static_cast<float>(state.condition.nitrous_ammo) / maximum_nitrous
                            : 0);
    custom_.fields["INTERNAL_HP"] = std::to_string(state.condition.internal_hp);
    custom_.fields["MAX_INTERNAL_HP"] = std::to_string(state.condition.maximum_internal_hp);
    constexpr std::array<std::string_view, 4> sides{"FRONT", "RIGHT", "REAR", "LEFT"};
    for (std::size_t i = 0; i < sides.size(); ++i) {
        const auto side = std::string(sides[i]);
        custom_.fields[side + "_LABEL"] = translate("Garage", side);
        custom_.fields[side + "_HP"] = std::to_string(state.condition.armor_hp[i]);
        custom_.fields["MAX_" + side + "_HP"] = std::to_string(state.condition.maximum_armor_hp[i]);
    }
    custom_.fields["NITROUS_LABEL"] = translate("Garage", "NITROUS");
    constexpr std::array<std::string_view, 3> slots{"FRONT", "SIDE", "REAR"};
    for (std::size_t i = 0; i < 3; ++i) {
        custom_.fields[std::string(slots[i])] = translate(
            "Garage", state.loadout.weapons[i].empty() ? "NONE" : state.loadout.weapons[i]);
        custom_.fields[std::string(slots[i]) + "_TYPE"] = state.loadout.weapons[i];
        bool uses_ammo{};
        for (const auto& item : garage_->items(GarageCategory::weapons))
            if (ResourceStore::canonical(item.name) == slots[i])
                for (const auto& product : item.products)
                    if (ResourceStore::canonical(product.name) ==
                        ResourceStore::canonical(state.loadout.weapons[i]))
                        uses_ammo = product.ammunition > 0;
        if (uses_ammo)
            custom_.fields[std::string(slots[i]) + "_AMMO"] =
                std::to_string(state.weapon_ammunition[i]);
        else
            custom_.fields.erase(std::string(slots[i]) + "_AMMO");
    }
    if (variant_.empty()) {
        if (scene_ && scene_->loaded())
            return scene_->set_vehicle_loadout(state.loadout, error,
                                               state.visuals ? &*state.visuals : nullptr);
        return Result::ok;
    }
    const auto category = std::find(kVariants.begin(), kVariants.end(), variant_);
    if (category == kVariants.end())
        return Result::invalid_argument;
    const auto type = static_cast<GarageCategory>(category - kVariants.begin());
    if (select_initial)
        garage_selection_ = garage_->initial_selection(type);
    garage_selection_.category = type;
    const auto list = garage_->items(type);
    if (garage_selection_.item >= list.size())
        return Result::invalid_argument;
    const auto& item = list[garage_selection_.item];
    if (garage_selection_.product >= item.products.size())
        return Result::invalid_argument;
    std::vector<std::string> names;
    for (const auto& entry : list)
        names.push_back(translate("Garage", entry.name));
    custom_.rows = {{"ITEM", std::move(names), garage_selection_.item}};
    if (!item.products.front().name.empty()) {
        std::vector<std::string> qualities;
        for (const auto& entry : item.products)
            qualities.push_back(translate("Garage", entry.name));
        custom_.rows.push_back({"QUALITY", std::move(qualities), garage_selection_.product});
    }
    GarageQuote quote;
    if (garage_->quote(garage_selection_, quote, error) != Result::ok)
        return Result::unavailable;
    std::string action;
    switch (quote.action) {
    case GarageAction::unavailable:
        action = translate("Garage", "NOT AVAILABLE");
        break;
    case GarageAction::installed:
        action = translate("Garage", type == GarageCategory::repair  ? "NO REPAIR NECESSARY"
                                     : type == GarageCategory::paint ? "CURRENT PAINT JOB"
                                                                     : "CURRENT TIRES");
        break;
    case GarageAction::sell:
        action = translate("Garage", "SELL BACK") + " $" + display_price(quote.trade_in);
        break;
    default:
        if (type == GarageCategory::repair && quote.price == 0)
            action = translate("Garage", "NO REPAIR NECESSARY");
        else
            action = translate("Garage", type == GarageCategory::repair ? "REPAIR" : "BUY") + " $" +
                     display_price(quote.price);
        break;
    }
    custom_.rows.push_back({"ACTION", {action}});
    custom_.fields["PRODUCT_AVAILABLE"] =
        item.products[garage_selection_.product].available ? "1" : "0";
    custom_.fields["TRADE_LABEL"] = translate("Garage", "TRADE IN");
    custom_.fields["AMMO_LABEL"] = translate("Garage", "AMMO");
    bool trade_visible = type == GarageCategory::tires;
    if (type == GarageCategory::armor)
        trade_visible = !state.loadout.armor.type.empty() &&
                        ResourceStore::canonical(state.loadout.armor.type) != "NONE";
    else if (type == GarageCategory::drivetrain)
        for (const auto& component : state.loadout.drivetrain)
            trade_visible |=
                ResourceStore::canonical(component.type) == ResourceStore::canonical(item.name);
    else if (type == GarageCategory::weapons)
        for (std::size_t i = 0; i < slots.size(); ++i)
            if (ResourceStore::canonical(item.name) == slots[i])
                trade_visible = !state.loadout.weapons[i].empty() &&
                                ResourceStore::canonical(state.loadout.weapons[i]) != "NONE";
    custom_.fields["TRADE IN VISIBLE"] = trade_visible ? "1" : "0";
    custom_.fields["TRADE IN"] = std::to_string(quote.trade_in);
    custom_.fields["AMMO"] = std::to_string(quote.ammunition);
    auto product_path = "Cars/Shared/" +
                        std::string(kStoreDirectories[static_cast<std::size_t>(type)]) + "/" +
                        item.name;
    const auto& product = item.products[garage_selection_.product];
    if (!product.name.empty())
        product_path += "/" + product.name;
    custom_.fields["PRODUCT_RESOURCE"] = product_path;
    Configuration product_config;
    if (product_config.load(resources_, product_path + "/Data.ini") != Result::ok)
        return Result::not_found;
    for (const auto section : {"DESCRIPTION", "ADVICE"}) {
        auto value = translate("GarageDescriptions", product_config.value(section, "TEXT"));
        std::size_t position{};
        while ((position = value.find("#ammo", position)) != std::string::npos) {
            const auto replacement = std::to_string(quote.ammunition);
            value.replace(position, 5, replacement);
            position += replacement.size();
        }
        custom_.fields[section] = std::move(value);
    }
    selected_ = std::clamp(selected_, 0, static_cast<int>(custom_.rows.size()) - 1);
    return Result::ok;
}

Result MenuFrontend::commit_garage(GarageSelection selection, bool confirmed, std::string& error) {
    bool changed{};
    const auto result = garage_->transact(selection, confirmed, changed, error);
    auto sounds = garage_->take_sound_events();
    sounds_.insert(sounds_.end(), sounds.begin(), sounds.end());
    if (result != Result::ok)
        return result;
    if (!error.empty()) {
        garage_message_ = translate("Misc", error);
        garage_modal_backdrop_.assign(canvas_.commands().begin(), canvas_.commands().end());
        error.clear();
    }
    if (changed)
        race_request_.player_loadout = garage_->race_loadout();
    return refresh_garage(false, error);
}

Result MenuFrontend::advance_garage(double elapsed, const FrontendInput& input,
                                    std::string& error) {
    const auto edge = [](bool now, bool before) { return now && !before; };
    const bool accept = edge(input.accept, previous_.accept);
    const bool back = edge(input.back, previous_.back);
    const bool click = edge(input.pointer_accept, previous_.pointer_accept);
    const int dx = edge(input.right, previous_.right) - edge(input.left, previous_.left);
    const int dy = edge(input.down, previous_.down) - edge(input.up, previous_.up);
    // Original nested confirmation dialogs suspend the garage scene clock.
    if (!garage_message_.empty()) {
        FrontendDialogLayout layout;
        const auto result = frontend_dialog_layout(canvas_, true, layout);
        if (result != Result::ok)
            return result;
        if (accept || (click && layout.hit(input.pointer_x, input.pointer_y) == 0)) {
            garage_message_.clear();
            garage_modal_backdrop_.clear();
        }
        previous_ = input;
        return draw(error);
    }
    if (garage_pending_selection_) {
        if (dx)
            garage_confirm_yes_ = dx < 0;
        bool dialog_hit{};
        if (input.pointer_moved || click) {
            FrontendDialogLayout layout;
            const auto result = frontend_dialog_layout(canvas_, false, layout);
            if (result != Result::ok)
                return result;
            const int hit = layout.hit(input.pointer_x, input.pointer_y);
            if (hit >= 0) {
                garage_confirm_yes_ = hit == 0;
                dialog_hit = true;
            }
        }
        const bool choose = accept || (click && dialog_hit);
        if (choose && !garage_confirm_yes_) {
            garage_modal_backdrop_.clear();
            garage_pending_selection_.reset();
            garage_confirmations_.clear();
        } else if (choose && garage_confirm_yes_) {
            if (++garage_confirmation_index_ == garage_confirmations_.size()) {
                const auto selection = *garage_pending_selection_;
                garage_modal_backdrop_.clear();
                garage_pending_selection_.reset();
                garage_confirmations_.clear();
                const auto result = commit_garage(selection, true, error);
                if (result != Result::ok)
                    return result;
            }
            garage_confirm_yes_ = false;
        }
        previous_ = input;
        return draw(error);
    }
    bool pointer_hit{};
    if (input.pointer_moved || click) {
        Configuration config;
        if (config.load(resources_, "ModifyVehicle/Data.ini") != Result::ok)
            return Result::unavailable;
        float start_y{}, spacing{}, left{}, width{};
        if (config.number("SELECTIONS", "SELECTIONSTARTY", start_y) != Result::ok ||
            config.number(variant_.empty() ? "SELECTIONS" : "STORE", "SELECTIONSPACING", spacing) !=
                Result::ok)
            return Result::unavailable;
        if (variant_.empty()) {
            config.number("SELECTIONS", "SELECTIONSTARTX", left);
            width = 210;
        } else {
            config.number("STORE", "SELECTIONRECTX", left);
            config.number("STORE", "SELECTIONRECTSIZEX", width);
        }
        float scale = 1;
        if (!variant_.empty() && config.number("STORE", "SELECTIONFONTSCALE", scale) != Result::ok)
            return Result::unavailable;
        const auto height = canvas_.font_height(
            variant_.empty() ? FrontendFont::menu : FrontendFont::nebula, scale);
        for (std::size_t row = 0; row < custom_.rows.size(); ++row)
            if (input.pointer_x >= left && input.pointer_x < left + width &&
                input.pointer_y >= start_y + row * (height + spacing) &&
                input.pointer_y < start_y + row * (height + spacing) + height) {
                selected_ = static_cast<int>(row);
                pointer_hit = true;
                break;
            }
    }
    if (back && exit_time_ < 0) {
        if (!variant_.empty()) {
            variant_.clear();
            // Keep the Summary entry at the top of history when leaving a nested store.
            if (!history_.empty() && history_.back() == "modify_vehicle")
                history_.pop_back();
            const auto result = enter("modify_vehicle", error);
            previous_ = input;
            return result;
        }
        const auto target = history_.empty() ? std::string("start_race_summary") : history_.back();
        if (!history_.empty())
            history_.pop_back();
        previous_ = input;
        const auto result = navigate(target, error);
        return result == Result::ok ? advance(elapsed, input, error) : result;
    }
    if (exit_time_ < 0 && !custom_.rows.empty()) {
        const auto old = selected_;
        selected_ = std::clamp(selected_ + dy, 0, static_cast<int>(custom_.rows.size()) - 1);
        if (old != selected_)
            sounds_.push_back("ModifyVehicle/select.wav");
        if (!variant_.empty() && dx && selected_ + 1 < static_cast<int>(custom_.rows.size())) {
            if (selected_ == 0) {
                const auto list = garage_->items(garage_selection_.category);
                const auto index = static_cast<std::size_t>(
                    (garage_selection_.item + list.size() + dx) % list.size());
                GarageSelection next{garage_selection_.category, index, 0};
                const auto& item = list[index];
                const auto result = scene_->transition_store_product(
                    item.name, item.products.front().name, error,
                    [this](std::string& name, std::string& quality, std::string&) {
                        if (!garage_pending_item_)
                            return Result::unavailable;
                        auto& pending = *garage_pending_item_;
                        const auto& destination = garage_->items(pending.category)[pending.item];
                        const auto& state = garage_->state();
                        pending.product = 0;
                        // Native resolves installed quality when the doors meet, after any
                        // purchases made on the still-visible old item during closure.
                        for (std::size_t i = 0; i < destination.products.size(); ++i) {
                            const auto candidate =
                                ResourceStore::canonical(destination.products[i].name);
                            bool installed{};
                            if (pending.category == GarageCategory::weapons)
                                for (const auto& weapon : state.loadout.weapons)
                                    installed |= !weapon.empty() &&
                                                 ResourceStore::canonical(weapon) == candidate;
                            else if (pending.category == GarageCategory::drivetrain)
                                for (const auto& upgrade : state.loadout.drivetrain)
                                    installed |=
                                        ResourceStore::canonical(upgrade.type) ==
                                            ResourceStore::canonical(destination.name) &&
                                        ResourceStore::canonical(upgrade.quality) == candidate;
                            if (installed) {
                                pending.product = i;
                                break;
                            }
                        }
                        name = destination.name;
                        quality = destination.products[pending.product].name;
                        return Result::ok;
                    });
                if (result != Result::ok)
                    return result;
                // The current item remains visible and actionable until the threshold.
                // Further direction edges retarget relative to that same current item.
                garage_pending_item_ = next;
            } else {
                const auto& products =
                    garage_->items(garage_selection_.category)[garage_selection_.item].products;
                garage_selection_.product =
                    (garage_selection_.product + products.size() + dx) % products.size();
                const auto& item =
                    garage_->items(garage_selection_.category)[garage_selection_.item];
                const auto result = scene_->select_store_product(
                    item.name, item.products[garage_selection_.product].name, error);
                if (result != Result::ok)
                    return result;
                sounds_.push_back("ModifyVehicle/select.wav");
                if (refresh_garage(false, error) != Result::ok)
                    return Result::unavailable;
            }
        }
        if (accept || (pointer_hit && click)) {
            if (variant_.empty()) {
                variant_ = kVariants[static_cast<std::size_t>(selected_)];
                history_.push_back("modify_vehicle");
                const auto result = enter("modify_vehicle", error);
                previous_ = input;
                return result;
            }
            if (selected_ + 1 == static_cast<int>(custom_.rows.size())) {
                GarageQuote quote;
                if (garage_->quote(garage_selection_, quote, error) != Result::ok)
                    return Result::unavailable;
                if (quote.affordable && !quote.confirmations.empty() &&
                    quote.action != GarageAction::unavailable &&
                    quote.action != GarageAction::installed) {
                    garage_modal_backdrop_.assign(canvas_.commands().begin(),
                                                  canvas_.commands().end());
                    garage_pending_selection_ = garage_selection_;
                    garage_confirmations_ = std::move(quote.confirmations);
                    garage_confirmation_index_ = 0;
                    garage_confirm_yes_ = false;
                } else if (commit_garage(garage_selection_, false, error) != Result::ok)
                    return Result::unavailable;
            }
        }
    }
    if (!garage_message_.empty() || garage_pending_selection_) {
        previous_ = input;
        return draw(error);
    }
    time_ += elapsed;
    if (scene_ && scene_->loaded()) {
        const auto result = scene_->advance(elapsed);
        if (result != Result::ok)
            return result;
        if (scene_->drain_store_door_events()) {
            sounds_.push_back("ModifyVehicle/door.wav");
            if (garage_pending_item_) {
                garage_selection_ = *garage_pending_item_;
                garage_pending_item_.reset();
                if (refresh_garage(false, error) != Result::ok)
                    return Result::unavailable;
            }
        }
    }
    previous_ = input;
    return draw(error);
}

Result MenuFrontend::draw_garage_modal(std::string& error) {
    if (garage_message_.empty() && !garage_pending_selection_)
        return Result::ok;
    // Native modal loops restore the captured framebuffer before each overlay draw.
    canvas_.begin();
    canvas_.append(garage_modal_backdrop_);
    FrontendCustomState dialog;
    dialog.screen = "dialog";
    dialog.preview_data = false;
    if (!garage_message_.empty()) {
        dialog.variant = "notice";
        dialog.message = garage_message_;
    } else {
        dialog.message = translate("Misc", garage_confirmations_[garage_confirmation_index_]);
        dialog.selected = garage_confirm_yes_ ? 0 : 1;
    }
    return FrontendCustom::draw(canvas_, dialog, error);
}
} // namespace roadwars
