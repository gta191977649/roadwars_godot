#include "game_src/unresolved/race_frontend.hpp"
#include "game_src/unresolved/weapon_runtime.hpp"
#include <algorithm>
#include <cmath>

namespace roadwars {
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00487fbe
Result RaceFrontend::configure_equipment(ResourceStore& resources, std::string_view car,
                                         const std::array<std::string, 3>& weapons,
                                         std::string& error) {
    equipment_loaded_ = false;
    weapon_icons_ = {};
    for (std::size_t i = 0; i < weapons.size(); ++i) {
        if (weapons[i].empty())
            continue;
        const auto* descriptor = find_weapon_descriptor(weapons[i]);
        if (!descriptor || static_cast<std::size_t>(descriptor->slot) != i)
            return Result::invalid_argument;
        const auto base = "Cars/Shared/Weapons/" + std::string(weapon_slot_name(descriptor->slot)) +
                          "/" + weapons[i];
        if (load_sprite(resources, base + "/16InterfaceIcon.bmp", weapon_icons_[i]) != Result::ok) {
            error = "Original weapon HUD icon is unavailable: " + weapons[i];
            return Result::unavailable;
        }
    }
    const auto damage = "Cars/" + std::string(car) + "/Damage";
    if (load_sprite(resources, "Cars/Shared/Drive Train/Nitrous Boost/16InterfaceIcon.bmp",
                    nitrous_icon_) != Result::ok ||
        load_sprite(resources, "Cars/Shared/Damage/16DamageBackground.bmp", damage_background_) !=
            Result::ok ||
        load_sprite(resources, damage + "/16DamageShellInternal.bmp", damage_shell_) !=
            Result::ok) {
        error = "Original equipment HUD textures are unavailable";
        return Result::unavailable;
    }
    Configuration config;
    if (config.load(resources, damage + "/Data.ini") != Result::ok)
        return Result::unavailable;
    const auto integer = [&](std::string_view section, std::string_view key, int& output) {
        float value{};
        if (config.number(section, key, value) != Result::ok || !std::isfinite(value) ||
            value < -4096 || value > 4096 || std::floor(value) != value)
            return false;
        output = static_cast<int>(value);
        return true;
    };
    if (!integer("DAMAGEINTERNAL", "OFFSETX", internal_offset_) ||
        !integer("DAMAGESHELL", "OFFSETX", shell_offset_))
        return Result::invalid_argument;
    const std::array<std::string_view, 5> sections{"DAMAGEINTERNAL", "SHELLFRONT", "SHELLRIGHT",
                                                   "SHELLREAR", "SHELLLEFT"};
    for (std::size_t i = 0; i < sections.size(); ++i) {
        auto& region = damage_regions_[i];
        int right{}, bottom{};
        if (!integer(sections[i], "LEFT", region.x) || !integer(sections[i], "TOP", region.y) ||
            !integer(sections[i], "RIGHT", right) || !integer(sections[i], "BOTTOM", bottom) ||
            region.x < 0 || region.y < 0 || right <= region.x || bottom <= region.y ||
            right > damage_shell_.width || bottom > damage_shell_.height)
            return Result::invalid_argument;
        region.width = right - region.x;
        region.height = bottom - region.y;
    }
    equipment_loaded_ = true;
    return Result::ok;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0048c313
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0048c90c
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0048cb23
Result RaceFrontend::draw_equipment(const FrontendEquipmentFrame& frame) {
    if (!equipment_loaded_)
        return Result::unavailable;
    if (frame.selected_item < 0 || frame.selected_item > 6)
        return Result::invalid_argument;
    if (frame.selected_item == 0 || frame.selected_item == 6)
        return Result::ok;
    if (frame.selected_item == 1) {
        sprite(damage_background_, {0, 352, 128, 128}, .7f);
        for (std::size_t i = 0; i < damage_regions_.size(); ++i) {
            const float hp = i == 0 ? frame.internal_hp : frame.armor_hp[i - 1];
            const float maximum =
                i == 0 ? frame.maximum_internal_hp : frame.maximum_armor_hp[i - 1];
            if (!std::isfinite(hp) || !std::isfinite(maximum) || maximum <= 0)
                return Result::invalid_argument;
            const auto ratio = std::clamp(hp / maximum, 0.f, 1.f);
            const auto red = ratio > .5f ? static_cast<int>(((1.0 - ratio) / .5) * 255.0) : 255;
            const auto green =
                ratio > .5f ? 255 : static_cast<int>(static_cast<double>(ratio) * 2.0 * 255.0);
            const auto& source = damage_regions_[i];
            const int offset = i == 0 ? internal_offset_ : shell_offset_;
            sprite(damage_shell_, {source.x + offset, 352 + source.y, source.width, source.height},
                   .7f);
            auto& quad = commands_.back();
            const auto left = static_cast<float>(source.x) / damage_shell_.width;
            const auto right = static_cast<float>(source.x + source.width) / damage_shell_.width;
            const auto top = 1 - static_cast<float>(source.y) / damage_shell_.height;
            const auto bottom =
                1 - static_cast<float>(source.y + source.height) / damage_shell_.height;
            quad.uv = {{{left, top}, {right, top}, {right, bottom}, {left, bottom}}};
            for (auto& color : quad.colors) {
                color[0] = red / 255.f;
                color[1] = green / 255.f;
                color[2] = 0;
            }
        }
        return Result::ok;
    }
    int ammunition{};
    bool bright{};
    if (frame.selected_item == 2) {
        ammunition = frame.nitrous_ammunition;
        bright = ammunition != 0;
        sprite(nitrous_icon_, {0, 352, 128, 128}, .7f, bright ? 1 : 100.f / 255);
    } else {
        const auto slot = static_cast<std::size_t>(frame.selected_item - 3);
        if (!frame.present[slot] || !weapon_icons_[slot].texture.value)
            return Result::invalid_argument;
        ammunition = frame.ammunition[slot];
        bright = frame.weapons_unlocked;
        sprite(weapon_icons_[slot], {0, 352, 128, 128}, .7f, bright ? 1 : 100.f / 255);
    }
    const auto label = std::to_string(ammunition);
    int width = -2;
    for (const auto character : label)
        width += font_.glyph(static_cast<unsigned char>(character)).width + 2;
    const auto first = commands_.size();
    text(64 - width / 2, 416 - font_.height() / 2, label);
    for (auto i = first; i < commands_.size(); ++i)
        for (auto& color : commands_[i].colors)
            color = {bright ? 1.f : 100.f / 255, bright ? 1.f : 100.f / 255,
                     bright ? 1.f : 100.f / 255, 178.f / 255};
    return Result::ok;
}
} // namespace roadwars
