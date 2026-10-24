#include "game_src/unresolved/frontend_custom.hpp"
#include "game_src/unresolved/frontend_dialog.hpp"
#include "game_src/unresolved/frontend_input_names.hpp"
#include "game_src/unresolved/frontend_localization.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <map>

namespace roadwars {
namespace {
constexpr std::uint32_t kRed = 0xffff0000, kYellow = 0xffffff00, kWhite = 0xffffffff;
std::string uppercase(std::string value) { return ResourceStore::canonical(value); }
std::string grouped_number(std::string value) {
    const auto first = !value.empty() && value.front() == '-' ? 1u : 0u;
    for (auto pos = static_cast<int>(value.size()) - 3; pos > static_cast<int>(first); pos -= 3)
        value.insert(static_cast<std::size_t>(pos), ",");
    return value;
}
void sprite_region(FrontendCanvas& canvas, const FrontendSprite& sprite, HudRect source,
                   HudRect destination, std::array<float, 4> color) {
    HudQuad quad;
    quad.texture = sprite.texture;
    const auto x = static_cast<float>(destination.x), y = static_cast<float>(destination.y);
    quad.positions = {{{x, y},
                       {x + destination.width, y},
                       {x + destination.width, y + destination.height},
                       {x, y + destination.height}}};
    const float left = static_cast<float>(source.x) / sprite.width,
                right = static_cast<float>(source.x + source.width) / sprite.width,
                top = 1 - static_cast<float>(source.y) / sprite.height,
                bottom = 1 - static_cast<float>(source.y + source.height) / sprite.height;
    quad.uv = {{{left, top}, {right, top}, {right, bottom}, {left, bottom}}};
    quad.colors.fill(color);
    canvas.append(std::span<const HudQuad>(&quad, 1));
}
int integer(const Configuration& config, std::string_view section, std::string_view key,
            int fallback = 0) {
    float value{};
    return config.number(section, key, value) == Result::ok && std::abs(value) < 1000000
               ? static_cast<int>(value)
               : fallback;
}
void add(std::vector<FrontendOptionRow>& rows, std::string label, std::vector<std::string> values,
         int selected = 0) {
    rows.push_back({std::move(label), std::move(values), 0, true});
    auto& row = rows.back();
    if (!row.values.empty())
        row.selected = static_cast<std::size_t>(
            std::clamp(selected, 0, static_cast<int>(row.values.size()) - 1));
}
std::vector<std::string> percentages() {
    std::vector<std::string> values;
    for (int i = 0; i <= 100; i += 5)
        values.push_back(std::to_string(i) + "%");
    return values;
}
std::string keyboard_name(int code) { return std::string(frontend_keyboard_name(code)); }
std::string joystick_name(int code) { return std::string(frontend_joystick_name(code)); }
void title(FrontendCanvas& canvas, std::string_view value) {
    canvas.text(FrontendFont::title, 320, 10, value, true);
}
void footer(FrontendCanvas& canvas) {
    canvas.text(FrontendFont::small, 320, 455, "HIT ESC TO GO BACK", true, 1, 0x64ffffff,
                0x64ffffff);
}
Result background(FrontendCanvas& canvas) {
    return canvas.sprite("Screens/Background.bmp", {0, 0, 640, 480});
}
Result original_size(FrontendCanvas& canvas, std::string_view path, int x, int y,
                     FrontendSprite& sprite, float alpha = 1) {
    auto result = canvas.asset(path, sprite);
    if (result == Result::ok)
        canvas.sprite(sprite, {x, y, sprite.width, sprite.height}, alpha);
    return result;
}
void preview_label(FrontendCanvas& canvas, const FrontendCustomState& state) {
    if (state.preview_data)
        canvas.text(FrontendFont::small, 320, 435, "PREVIEW DATA", true, 1, 0xff80ffff, 0xff80ffff);
}
// Original selected icon overlay moves linearly between alpha 0 and .7.
float pulse(double time) {
    const double phase = std::fmod(std::max(time, 0.0), 1.4);
    return static_cast<float>(phase <= .7 ? phase : 1.4 - phase);
}
Result action_icon(FrontendCanvas& canvas, std::string_view root, std::string_view label, int x,
                   int y, bool selected, double elapsed) {
    const int radius = selected ? 72 : 36;
    const HudRect rectangle{x - radius, y - radius, radius * 2, radius * 2};
    FrontendSprite icon;
    auto result =
        canvas.asset(std::string(root) + "/" + std::string(label) + "/16SelectionIcon.bmp", icon);
    if (result != Result::ok)
        return result;
    canvas.sprite(icon, rectangle);
    // 004927c0/004a91f0 bake centered Arial15 at (70,18), scale .7, into the
    // source surface before the selection animation scales that whole surface.
    const float icon_scale = static_cast<float>(radius * 2) / icon.width;
    const float label_scale = .7f * icon_scale;
    canvas.text(FrontendFont::arial, rectangle.x + 70 * icon_scale,
                rectangle.y + 18 * icon_scale -
                    canvas.font_height(FrontendFont::arial) * label_scale / 2,
                label, true, label_scale);
    return canvas.sprite("Standings/16AlphaIcon.bmp", rectangle, selected ? pulse(elapsed) : .7f);
}
Result draw_options(FrontendCanvas& canvas, const FrontendCustomState& state) {
    if (state.rows.empty() || state.rows.size() > 20 || state.selected >= state.rows.size())
        return Result::invalid_argument;
    if (background(canvas) != Result::ok)
        return Result::not_found;
    const auto name = state.screen == "graphics" ? "GRAPHIC OPTIONS"
                      : state.screen == "sound"  ? "SOUND OPTIONS"
                      : state.screen == "game"   ? "GAME OPTIONS"
                                                 : uppercase(state.screen);
    title(canvas, name);
    footer(canvas);
    if (state.screen == "sound" && !state.message.empty())
        canvas.text(FrontendFont::small, 320, 420, state.message, true, .8f);
    const bool modern = state.screen == "graphics" && state.fields.contains("MODERN_GRAPHICS");
    const float scale = 1.f;
    const int step = canvas.font_height(FrontendFont::body) + 5;
    const int top = 240 - static_cast<int>(state.rows.size()) * step / 2;
    if (modern) {
        canvas.text(FrontendFont::small, 320, 430, state.message, true, .8f);
    }
    const int right = 610 + canvas.text_width(FrontendFont::body, ">");
    auto result = canvas.sprite(
        "Game Options/Options/16SelectionBackground.bmp",
        {20, top - 10, right - 20, static_cast<int>(state.rows.size()) * step + 20}, .15f);
    if (result != Result::ok)
        return result;
    for (std::size_t i = 0; i < state.rows.size(); ++i) {
        const auto& row = state.rows[i];
        const auto selected = i == state.selected;
        const auto color = !row.available ? 0xff808080 : selected ? kRed : kWhite;
        const auto bottom = selected && row.available ? kYellow : color;
        const auto y = static_cast<float>(top + static_cast<int>(i) * step);
        canvas.text(FrontendFont::body, 30, y, row.label, false, scale, color, bottom);
        if (!row.values.empty()) {
            if (row.selected >= row.values.size())
                return Result::invalid_argument;
            canvas.text(FrontendFont::body, 430, y, row.values[row.selected], true, scale, color,
                        bottom);
            const bool editable = modern && row.available && row.id != "apply" &&
                                  row.id != "defaults" && row.id != "restart" && row.id != "back" &&
                                  row.id != "confirm" && row.id != "revert";
            if (row.values.size() > 1 || editable) {
                canvas.text(FrontendFont::body, 310, y, "<", false, scale, color, bottom);
                canvas.text(FrontendFont::body, 610, y, ">", false, scale, color, bottom);
            }
        }
    }
    if (state.screen == "keyboard" || state.screen == "joystick")
        canvas.text(FrontendFont::small, 320, top + static_cast<int>(state.rows.size()) * step + 20,
                    state.screen == "keyboard" ? "PRESS ENTER TO CHANGE"
                                               : "PRESS BUTTON 1 TO CHANGE",
                    true);
    return Result::ok;
}
Result draw_results(FrontendCanvas& canvas, const FrontendCustomState& state) {
    if (state.racers.size() > 6)
        return Result::invalid_argument;
    if (background(canvas) != Result::ok)
        return Result::not_found;
    const bool race = state.screen == "race_results";
    title(canvas, race                          ? "RACE RESULTS"
                  : state.screen == "standings" ? "STANDINGS"
                                                : "SEASON RESULTS");
    const int x = race ? 370 : 250;
    const auto points = race ? "POINTS" : "TOTAL PTS.";
    canvas.text(FrontendFont::body, static_cast<float>(x), 70, points, false, 1, kRed, kYellow);
    const int cash_x = race ? 470 : 280 + canvas.text_width(FrontendFont::body, points);
    canvas.text(FrontendFont::body, static_cast<float>(cash_x), 70, race ? "CASH" : "TOTAL CASH",
                false, 1, kRed, kYellow);
    const auto fh = canvas.font_height(FrontendFont::body);
    for (std::size_t i = 0; i < state.racers.size(); ++i) {
        const auto& racer = state.racers[i];
        FrontendSprite portrait;
        auto result =
            canvas.asset("Characters/" + racer.character + "/16StandingsPortrait.bmp", portrait);
        if (result != Result::ok)
            return result;
        const int y = 90 + static_cast<int>(i) * (race ? 64 : portrait.height + 4);
        // 0048fd6d admits the next row only after the preceding row reaches x=140.
        const double row_time = std::max(0.0, state.elapsed - static_cast<double>(i) * .501);
        const int left =
            race ? std::max(140, 641 - static_cast<int>(std::min(row_time, 1.0) * 1000)) : 32;
        result = canvas.sprite("RaceInterface/16GlassBackground.bmp",
                               {left - 1, y - 1, 590 - left + 1, portrait.height + 2}, .6f);
        if (result != Result::ok)
            return result;
        canvas.sprite(portrait, {left, y, portrait.width, portrait.height});
        if (race && left != 140)
            continue;
        const auto rank = race && !racer.finished ? "DNF" : std::to_string(i + 1) + ".";
        const int rank_x = left - canvas.text_width(FrontendFont::body, rank) - (race ? 2 : 6);
        canvas.text(FrontendFont::body, static_cast<float>(rank_x),
                    static_cast<float>(y + portrait.height - fh), rank, false, 1, kRed, kYellow);
        canvas.text(FrontendFont::body, static_cast<float>(left + portrait.width + 4),
                    static_cast<float>(y + portrait.height - fh), uppercase(racer.name));
        if (!racer.total_time.empty())
            canvas.text(FrontendFont::small, static_cast<float>(left + portrait.width + 4),
                        static_cast<float>(y + 4), racer.total_time);
        canvas.text(FrontendFont::body, static_cast<float>(x),
                    static_cast<float>(y + portrait.height - fh), std::to_string(racer.points));
        canvas.text(FrontendFont::body, static_cast<float>(cash_x),
                    static_cast<float>(y + portrait.height - fh), std::to_string(racer.cash));
    }
    if (state.screen == "standings") {
        canvas.text(FrontendFont::body, 67, 360, "NEXT TRACK: ", false, 1, 0xff0000ff, kYellow);
        const auto width = canvas.text_width(FrontendFont::body, "NEXT TRACK: ");
        canvas.text(FrontendFont::body, static_cast<float>(67 + width), 360,
                    uppercase(state.track));
        if (!state.message.empty())
            canvas.text(FrontendFont::body, 67, 400, state.message);
        footer(canvas);
    }
    preview_label(canvas, state);
    if (state.fields.contains("LIVE_RACE"))
        canvas.text(FrontendFont::small, 320, 450, "DEBUG RACE - REWARDS NOT APPLIED", true);
    return action_icon(canvas,
                       state.screen == "race_results" ? "Race Results"
                       : state.screen == "standings"  ? "Standings"
                                                      : "Season Results",
                       "CONTINUE", 540, 392, true, state.elapsed);
}
Result draw_summary(FrontendCanvas& canvas, const FrontendCustomState& state) {
    if (state.racers.size() > 6)
        return Result::invalid_argument;
    auto result =
        canvas.sprite("Tracks/" + state.track + "/16RaceSummaryBackground.bmp", {0, 0, 640, 480});
    if (result != Result::ok)
        return result;
    canvas.text(FrontendFont::title, 320, 20, uppercase(state.track), true);
    const auto record_height = canvas.font_height(FrontendFont::body) * .75f;
    canvas.text(FrontendFont::body, 113, 67, "Best Time", true, .75f, 0xffe05a20, 0xffe05a20);
    canvas.text(FrontendFont::body, 527, 67, "Best Lap", true, .75f, 0xffe05a20, 0xffe05a20);
    canvas.text(FrontendFont::body, 113, 67 + record_height, state.best_time, true, .75f,
                0xb2ffffff, 0xb2ffffff);
    canvas.text(FrontendFont::body, 527, 67 + record_height, state.best_lap, true, .75f, 0xb2ffffff,
                0xb2ffffff);
    // 00494a1d: paired columns, 55-pixel rows starting at (71,148).
    for (std::size_t i = 0; i < state.racers.size(); ++i) {
        const int x = 71 + static_cast<int>(i % 2) * 290, y = 148 + static_cast<int>(i / 2) * 55;
        FrontendSprite portrait;
        result = original_size(
            canvas, "Characters/" + state.racers[i].character + "/16RaceSummaryPortrait.bmp", x, y,
            portrait);
        if (result != Result::ok)
            return result;
        canvas.text(FrontendFont::small, static_cast<float>(x + 156), static_cast<float>(y + 2),
                    uppercase(state.racers[i].name), true);
        FrontendSprite points;
        result = original_size(canvas, "Start Race Summary/16PointsBox.bmp", x + 46, y + 1, points);
        if (result != Result::ok)
            return result;
        canvas.text(FrontendFont::score, static_cast<float>(x + 46 + points.width / 2),
                    static_cast<float>(y + 1), std::to_string(state.racers[i].points), true);
    }
    // Race/lap/weapon state is supplied as rows, never inferred from preview racers.
    for (std::size_t i = 0; i < std::min<std::size_t>(3, state.rows.size()); ++i) {
        const auto& row = state.rows[i];
        const auto y = static_cast<float>(344 + static_cast<int>(i) * 32);
        canvas.text(FrontendFont::cyber16, 210, y, row.label);
        if (!row.values.empty() && row.selected < row.values.size())
            canvas.text(FrontendFont::body, 349, y, row.values[row.selected], true, 1, 0xb2ffffff,
                        0xb2ffffff);
    }
    preview_label(canvas, state);
    result = action_icon(canvas, "Start Race Summary", "GARAGE", 98, 392, state.selected == 0,
                         state.elapsed);
    if (result != Result::ok)
        return result;
    if (state.variant != "join")
        result = action_icon(canvas, "Start Race Summary", "START RACE", 540, 392,
                             state.selected == 1, state.elapsed);
    canvas.text(FrontendFont::small, 320, 466, "HIT ESC TO GO BACK", true, 1, 0x64ffffff,
                0x64ffffff);
    return result;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004b090a
Result draw_dialog(FrontendCanvas& canvas, const FrontendCustomState& state) {
    const bool notice = state.variant == "notice";
    FrontendDialogLayout layout;
    auto result = frontend_dialog_layout(canvas, notice, layout);
    if (result != Result::ok)
        return result;
    Configuration config;
    FrontendLocalization localization;
    std::string error;
    float language{};
    if (config.load(canvas.resources(), "Ini/RoadWars.ini") != Result::ok ||
        config.number("ROADWARS", "LANGUAGE", language) != Result::ok)
        return Result::not_found;
    if (language != 0 && localization.load(canvas.resources(), "Misc", error) != Result::ok)
        return Result::not_found;
    const auto label = [&](std::string_view key) {
        std::string text(key);
        if (language != 0)
            localization.lookup(key, static_cast<int>(language), text);
        return text;
    };
    canvas.sprite(layout.panel_sprite, layout.panel, layout.panel_alpha);
    canvas.text(FrontendFont::small, static_cast<float>(layout.message_x),
                static_cast<float>(layout.message_y), state.message, true, 1, kWhite, kWhite,
                FrontendVerticalAlign::center);
    for (std::size_t i = 0; i < layout.button_count; ++i) {
        canvas.sprite(layout.button_sprite, layout.buttons[i]);
        const auto top = notice || state.selected == i ? 0xff00ff00u : kWhite;
        const auto bottom = notice ? 0xff00ff14u : top;
        canvas.text(FrontendFont::small, static_cast<float>(layout.button_centers[i][0]),
                    static_cast<float>(layout.button_centers[i][1]),
                    label(notice   ? "OK"
                          : i == 0 ? "YES"
                                   : "NO"),
                    true, 1, top, bottom, FrontendVerticalAlign::center);
    }
    return Result::ok;
}
Result draw_pause(FrontendCanvas& canvas, const FrontendCustomState& state) {
    if (state.rows.empty())
        return Result::invalid_argument;
    Configuration config;
    auto result = config.load(canvas.resources(), "Game Menu/Screen Setup.ini");
    if (result != Result::ok)
        return result;
    float opacity{};
    if (config.number("INFO", "FADEOPACITY", opacity) != Result::ok)
        return Result::invalid_argument;
    result = canvas.sprite("Game Menu/16Fade.bmp", {0, 0, 640, 480}, opacity);
    if (result != Result::ok)
        return result;
    FrontendSprite panel;
    result = canvas.asset("Game Menu/16SelectionBackground.bmp", panel);
    if (result != Result::ok)
        return result;
    float background_opacity{};
    if (config.number("INFO", "BACKGROUNDOPACITY", background_opacity) != Result::ok)
        return Result::invalid_argument;
    canvas.sprite(panel, {320 - panel.width / 2, 240 - panel.height / 2, panel.width, panel.height},
                  background_opacity);
    const int step =
        canvas.font_height(FrontendFont::menu) + integer(config, "INFO", "SELECTIONSPACING", 5);
    const int height =
        canvas.font_height(FrontendFont::menu) * static_cast<int>(state.rows.size()) +
        integer(config, "INFO", "SELECTIONSPACING", 5) * (static_cast<int>(state.rows.size()) - 1);
    const int top = 240 - height / 2;
    for (std::size_t i = 0; i < state.rows.size(); ++i) {
        const auto& row = state.rows[i];
        const bool selected = i == state.selected && row.available;
        const auto color = !row.available ? 0xff808080 : selected ? kRed : kWhite;
        canvas.text(FrontendFont::menu,
                    static_cast<float>(320 - panel.width / 2 +
                                       integer(config, "INFO", "BORDERINDENTX", 38)),
                    static_cast<float>(top + static_cast<int>(i) * step), row.label, false, 1,
                    color, selected ? kYellow : color);
    }
    return Result::ok;
}
Result draw_list(FrontendCanvas& canvas, const FrontendCustomState& state) {
    if (state.rows.size() > 10 || (!state.rows.empty() && state.selected >= state.rows.size()))
        return Result::invalid_argument;
    if (background(canvas) != Result::ok)
        return Result::not_found;
    title(canvas, state.screen == "create_season" ? "CREATE SEASON"
                  : state.screen == "load_season" ? "LOAD SEASON"
                                                  : "SELECT SESSION");
    if (state.rows.empty())
        canvas.text(FrontendFont::body, 320, 220,
                    state.screen == "select_session" ? "NO SESSIONS ARE AVAILABLE"
                                                     : "NO SEASONS HAVE BEEN SAVED",
                    true);
    const int step = canvas.font_height(FrontendFont::body) + 8;
    for (std::size_t i = 0; i < state.rows.size(); ++i) {
        const auto& row = state.rows[i];
        const bool selected = i == state.selected;
        const auto y = static_cast<float>(90 + static_cast<int>(i) * step);
        canvas.text(FrontendFont::body, 200, y, std::to_string(i + 1) + ".", false, 1,
                    selected ? kRed : kWhite, selected ? kYellow : kWhite);
        canvas.text(FrontendFont::body, 230, y, row.label, false, 1, selected ? kRed : kWhite,
                    selected ? kYellow : kWhite);
    }
    if (state.screen == "create_season" && !state.rows.empty()) {
        canvas.text(FrontendFont::small, 320, 434, "PRESS TAB KEY OR BUTTON 3 CHANGE CASE", true, 1,
                    0x64ffffff, 0x64ffffff);
        canvas.text(FrontendFont::small, 320, 450, "PRESS ENTER KEY OR BUTTON 1 TO CONTINUE", true,
                    1, 0x64ffffff, 0x64ffffff);
    }
    canvas.text(FrontendFont::small, 320, 466, "HIT ESC TO GO BACK", true, 1, 0x64ffffff,
                0x64ffffff);
    if (state.preview_data)
        canvas.text(FrontendFont::small, 320, 410, "PREVIEW DATA", true, 1, 0xff80ffff, 0xff80ffff);
    return Result::ok;
}
std::string field(const FrontendCustomState& state, const std::string& key,
                  const std::string& fallback = {}) {
    const auto it = state.fields.find(key);
    return it == state.fields.end() ? fallback : it->second;
}
std::string store_directory(std::string_view variant) {
    if (variant == "tires")
        return "Cars/Shared/Tires";
    if (variant == "drive_train")
        return "Cars/Shared/Drive Train";
    if (variant == "weapons")
        return "Cars/Shared/Weapons";
    if (variant == "armor")
        return "Cars/Shared/Armor";
    if (variant == "repair")
        return "Cars/Shared/Repair";
    if (variant == "paint")
        return "Cars/Shared/Paint Job";
    return {};
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00460d57
void wrapped_text(FrontendCanvas& canvas, const std::string& value, int x, int y, int width,
                  int spacing, float scale, std::uint32_t top, std::uint32_t bottom) {
    // 00460d57 appends until the current width reaches the right edge, then
    // rewinds the unfinished word. Its supplied bottom edge is never consulted.
    const int line_height = canvas.font_height(FrontendFont::nebula, scale) + spacing;
    std::size_t cursor = 0;
    int row = 0;
    while (cursor < value.size()) {
        const auto start = cursor;
        std::string line;
        while (canvas.text_width(FrontendFont::nebula, line, scale) < width &&
               cursor < value.size()) {
            const char c = value[cursor++];
            if (c != ' ' || !line.empty())
                line += c;
        }
        if (cursor < value.size() && value[cursor] != ' ' && !line.empty()) {
            auto last = line.size() - 1;
            bool found_space = false;
            for (auto i = line.size(); i > 0; --i) {
                if (line[i - 1] == ' ') {
                    last = i - 1;
                    found_space = true;
                } else if (found_space)
                    break;
            }
            cursor -= line.size() - last - 1;
            line.resize(last);
        }
        if (cursor == start)
            break; // Malformed external bounds must not stall the frontend.
        canvas.text(FrontendFont::nebula, static_cast<float>(x),
                    static_cast<float>(y + row++ * line_height), line, false, scale, top, bottom);
    }
}
Result draw_host_or_join(FrontendCanvas& canvas, const FrontendCustomState& state) {
    auto result = background(canvas);
    if (result != Result::ok)
        return result;
    title(canvas, "HOST OR JOIN");
    const std::array<std::pair<const char*, int>, 2> icons{{{"HOST", 118}, {"JOIN", 520}}};
    for (std::size_t i = 0; i < icons.size(); ++i) {
        const bool selected = i == state.selected;
        const int radius = selected ? 100 : 50;
        const HudRect box{icons[i].second - radius, 322 - radius, radius * 2, radius * 2};
        result = canvas.sprite(
            std::string("Host Or Join/") + icons[i].first + "/16SelectionIcon.bmp", box);
        if (result != Result::ok)
            return result;
        canvas.text(FrontendFont::menu, static_cast<float>(icons[i].second),
                    static_cast<float>(322 + radius * .15f), icons[i].first, true,
                    selected ? 1 : .5f);
        result =
            canvas.sprite("Screens/16IconTexture.bmp", box, selected ? pulse(state.elapsed) : .7f);
        if (result != Result::ok)
            return result;
    }
    const int step = canvas.font_height(FrontendFont::body) + 5;
    const auto connection = uppercase(state.variant);
    if (connection == "TCP-IP" || connection == "TCP/IP" || connection == "TCPIP") {
        canvas.text(FrontendFont::body, 10, static_cast<float>(190 - step),
                    "IP ADDRESS: " + field(state, "IP ADDRESS"));
    } else if (connection == "MODEM") {
        canvas.text(FrontendFont::body, 10, static_cast<float>(190 - step * 2),
                    "MODEM: " + field(state, "MODEM", "UNAVAILABLE"));
        canvas.text(FrontendFont::body, 10, static_cast<float>(190 - step),
                    "PHONE NUMBER: " + field(state, "PHONE NUMBER"));
    } else if (connection == "SERIAL") {
        canvas.text(FrontendFont::body, 10, static_cast<float>(190 - step * 2),
                    "PORT: " + field(state, "PORT", "UNAVAILABLE"));
        canvas.text(FrontendFont::body, 10, static_cast<float>(190 - step),
                    "BAUD: " + field(state, "BAUD", "UNAVAILABLE"));
    }
    canvas.text(FrontendFont::body, 10, 190, "STATUS: " + field(state, "STATUS", "UNAVAILABLE"));
    canvas.text(FrontendFont::small, 320, 466, "HIT ESC TO GO BACK", true, 1, 0x64ffffff,
                0x64ffffff);
    return Result::ok;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0043d2d0
Result draw_garage(FrontendCanvas& canvas, const FrontendCustomState& state) {
    Configuration setup, character;
    auto result = setup.load(canvas.resources(), "ModifyVehicle/Data.ini");
    if (result != Result::ok)
        return result;
    const auto name =
        field(state, "CHARACTER", state.racers.empty() ? "Animal" : state.racers.front().character);
    result = character.load(canvas.resources(), "Characters/" + name + "/Data.ini");
    if (result != Result::ok)
        return result;
    float opacity{};
    if (setup.number("GRAPHICS", "INTERFACEOPACITY", opacity) != Result::ok)
        return Result::invalid_argument;
    const int portrait_x = integer(setup, "GRAPHICS", "PANEL1LEFT"),
              portrait_y = integer(setup, "GRAPHICS", "PANEL1TOP"),
              panel_x = integer(setup, "GRAPHICS", "PANEL2LEFT"),
              panel_y = integer(setup, "GRAPHICS", "PANEL2TOP"),
              weapon_x = integer(setup, "GRAPHICS", "PANEL3LEFT"),
              weapon_y = integer(setup, "GRAPHICS", "PANEL3TOP");
    FrontendSprite sprite;
    for (int i = 0; i < 2; ++i) {
        result = original_size(
            canvas, "Characters/" + name + "/16GaragePortrait" + std::to_string(i + 1) + ".bmp",
            portrait_x + i * 128, portrait_y, sprite, opacity);
        if (result != Result::ok)
            return result;
    }
    for (int i = 0; i < 3; ++i) {
        result = original_size(canvas, "ModifyVehicle/16Panel" + std::to_string(i + 1) + ".bmp",
                               i < 2 ? panel_x + i * 128 : weapon_x, i < 2 ? panel_y : weapon_y,
                               sprite, opacity);
        if (result != Result::ok)
            return result;
    }
    const auto vehicle = character.value("INFO", "VEHICLE");
    if (vehicle.empty())
        return Result::invalid_argument;
    FrontendSprite body_icon;
    result = canvas.asset("Cars/" + vehicle + "/Garage/16GarIcon1.bmp", body_icon);
    if (result != Result::ok)
        return result;
    canvas.sprite(body_icon, {panel_x + 128 + 89, panel_y + 23, 64, 66}, opacity);
    result = original_size(canvas, "Cars/" + vehicle + "/Garage/16GarIcon2.bmp", panel_x + 128 + 89,
                           panel_y + 89, body_icon, opacity);
    if (result != Result::ok)
        return result;
    const auto font = FrontendFont::nebula;
    constexpr std::uint32_t kGarageText = 0xcc36c41a;
    const auto text = [&](int x, int y, const std::string& value, bool center = true,
                          bool vertical_center = false) {
        canvas.text(font, static_cast<float>(x), static_cast<float>(y), value, center, 1,
                    kGarageText, kGarageText,
                    vertical_center ? FrontendVerticalAlign::center : FrontendVerticalAlign::top);
    };
    const auto numeric = [&](std::string_view key, float fallback = 0.f) {
        const auto value = field(state, std::string(key));
        float parsed{};
        const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
        return result.ec == std::errc{} && result.ptr == value.data() + value.size() &&
                       std::isfinite(parsed) && std::abs(parsed) < 1000000000.f
                   ? parsed
                   : fallback;
    };
    FrontendSprite meter;
    result = canvas.asset("ModifyVehicle/16Meter.bmp", meter);
    if (result != Result::ok)
        return result;
    const auto bar = [&](int x, int y, int width, float ratio) {
        sprite_region(canvas, meter, {0, 1, 25, 5}, {x, y, width, 5},
                      {1, 49.f / 255, 8.f / 255, opacity});
        const auto filled = static_cast<int>(std::clamp(ratio, 0.f, 1.f) * width);
        if (filled)
            sprite_region(canvas, meter, {0, 1, 25, 5}, {x, y, filled, 5},
                          {70.f / 255, 240.f / 255, 23.f / 255, opacity});
    };
    const auto quality_ratio = [&](std::string_view key) {
        const auto value = uppercase(field(state, std::string(key)));
        return value == "POOR" ? .33f : value == "GOOD" ? .67f : value == "BEST" ? 1.f : 0.f;
    };
    text(portrait_x + 168, portrait_y + 23,
         uppercase(state.racers.empty() ? name : state.racers.front().name), true, true);
    const auto cash = state.racers.empty() ? field(state, "CASH", "PREVIEW")
                                           : std::to_string(state.racers.front().cash);
    text(portrait_x + 168, portrait_y + 40, "$" + grouped_number(cash), true, true);
    text(panel_x + 99, panel_y + 27, field(state, "DRIVE_LABEL", "DRIVE TRAIN"));
    text(panel_x + 61, panel_y + 60, field(state, "TIRES", "PREVIEW"));
    text(panel_x + 61, panel_y + 94, field(state, "ARMOR", "PREVIEW"));
    bar(panel_x + 61, panel_y + 39, 74, numeric("DRIVE_RATIO"));
    bar(panel_x + 24, panel_y + 74, 74, quality_ratio("TIRES_QUALITY"));
    bar(panel_x + 24, panel_y + 109, 74, quality_ratio("ARMOR_QUALITY"));
    text(panel_x + 128 + 37, panel_y + 62, field(state, "NITROUS_LABEL", "NITROUS"));
    text(panel_x + 128 + 37, panel_y + 74, field(state, "BOOST_LABEL", "BOOST"));
    text(panel_x + 128 + 37, panel_y + 93, field(state, "NITROUS_AMMO", "0"));
    bar(panel_x + 128 + 15, panel_y + 109, 43, numeric("NITROUS_RATIO"));
    // Native health is displayed as integer current / maximum, without an HP suffix.
    const auto integer_text = [&](std::string_view key) {
        return std::to_string(static_cast<int>(numeric(key)));
    };
    if (state.fields.contains("INTERNAL_HP") && state.fields.contains("MAX_INTERNAL_HP")) {
        text(portrait_x + 89, portrait_y + 63, field(state, "INTERNAL_LABEL", "INTERNAL") + " ",
             false);
        const auto current = integer_text("INTERNAL_HP"), maximum = integer_text("MAX_INTERNAL_HP");
        canvas.text(font, static_cast<float>(portrait_x + 141), static_cast<float>(portrait_y + 63),
                    current + " / " + maximum, false, 1, 0xcc00ff00, 0xcc00ff00);
    }
    constexpr std::array<std::string_view, 4> sides{"FRONT", "RIGHT", "REAR", "LEFT"};
    for (std::size_t side = 0; side < sides.size(); ++side) {
        const auto key = std::string(sides[side]);
        if (!state.fields.contains(key + "_HP") || !state.fields.contains("MAX_" + key + "_HP"))
            continue;
        const auto y =
            portrait_y + 63 + static_cast<int>(side + 1) * (canvas.font_height(font) + 2);
        text(portrait_x + 89, y, field(state, key + "_LABEL", key), false);
        const auto current = integer_text(key + "_HP"),
                   maximum = integer_text("MAX_" + key + "_HP");
        const float maximum_health = numeric("MAX_" + key + "_HP");
        const auto ratio =
            maximum_health > 0 ? std::clamp(numeric(key + "_HP") / maximum_health, 0.f, 1.f) : 0.f;
        const auto red = ratio > .5f ? static_cast<unsigned>((1 - ratio) / .5f * 255) : 255u;
        const auto green = ratio > .5f ? 255u : static_cast<unsigned>(ratio * 2 * 255);
        const auto color = 0xcc000000u | (red << 16) | (green << 8);
        const auto number_x = portrait_x + 139 + canvas.text_width(font, "8") * 4;
        canvas.text(font, static_cast<float>(number_x - canvas.text_width(font, current)),
                    static_cast<float>(y), current, false, 1, color, color);
        canvas.text(font, static_cast<float>(number_x), static_cast<float>(y), " / " + maximum,
                    false, 1, 0xcc00ff00, 0xcc00ff00);
    }
    for (const auto& item :
         std::array<std::pair<const char*, int>, 3>{{{"FRONT", 31}, {"SIDE", 65}, {"REAR", 100}}})
        text(weapon_x + 52, weapon_y + item.second, field(state, item.first, "NONE"));
    const auto icon = [&](const std::string& root, int x, int y) {
        return original_size(canvas, root + "/16GarIcon.bmp", x, y, sprite, opacity);
    };
    if (state.fields.contains("TIRES_TYPE") &&
        icon("Cars/Shared/Tires/" + field(state, "TIRES_TYPE"), panel_x + 104, panel_y + 57) !=
            Result::ok)
        return Result::not_found;
    if (state.fields.contains("ARMOR_TYPE") && !field(state, "ARMOR_TYPE").empty() &&
        uppercase(field(state, "ARMOR_TYPE")) != "NONE" &&
        icon("Cars/Shared/Armor/" + field(state, "ARMOR_TYPE"), panel_x + 104, panel_y + 92) !=
            Result::ok)
        return Result::not_found;
    constexpr std::array<std::string_view, 3> slots{"FRONT", "SIDE", "REAR"};
    constexpr std::array<int, 3> icon_x{93, 113, 93}, icon_y{26, 53, 96}, ammo_x{94, 114, 94},
        ammo_y{31, 77, 98};
    for (std::size_t slot = 0; slot < slots.size(); ++slot) {
        const auto key = std::string(slots[slot]);
        const auto type = field(state, key + "_TYPE");
        if (!type.empty() && uppercase(type) != "NONE" &&
            icon("Cars/Shared/Weapons/" + key + "/" + type, panel_x + 128 + icon_x[slot],
                 panel_y + icon_y[slot]) != Result::ok)
            return Result::not_found;
        if (state.fields.contains(key + "_AMMO")) {
            const auto alpha = static_cast<std::uint32_t>(opacity * 255);
            const auto color = (alpha << 24) | 0xffffff;
            canvas.text(font, static_cast<float>(panel_x + 128 + ammo_x[slot]),
                        static_cast<float>(panel_y + ammo_y[slot]), integer_text(key + "_AMMO"),
                        false, 1, color, color);
        }
    }
    Configuration damage;
    FrontendSprite damage_atlas;
    result = damage.load(canvas.resources(), "Cars/" + vehicle + "/Damage/Data.ini");
    if (result == Result::ok)
        result =
            canvas.asset("Cars/" + vehicle + "/Damage/16DamageShellInternal.bmp", damage_atlas);
    if (result != Result::ok)
        return result;
    constexpr std::array<std::string_view, 5> damage_sections{
        "DAMAGEINTERNAL", "SHELLFRONT", "SHELLRIGHT", "SHELLREAR", "SHELLLEFT"};
    constexpr std::array<std::string_view, 5> health_names{"INTERNAL", "FRONT", "RIGHT", "REAR",
                                                           "LEFT"};
    for (std::size_t region = 0; region < damage_sections.size(); ++region) {
        const auto section = damage_sections[region];
        const int left = integer(damage, section, "LEFT"), top = integer(damage, section, "TOP"),
                  right = integer(damage, section, "RIGHT"),
                  bottom = integer(damage, section, "BOTTOM"),
                  offset_x =
                      integer(damage, region == 0 ? "DAMAGEINTERNAL" : "DAMAGESHELL", "OFFSETX");
        const auto maximum = numeric("MAX_" + std::string(health_names[region]) + "_HP");
        const auto ratio =
            maximum > 0
                ? std::clamp(numeric(std::string(health_names[region]) + "_HP") / maximum, 0.f, 1.f)
                : 0.f;
        const auto red = ratio > .5f ? static_cast<int>((1 - ratio) * 510) : 255,
                   green = ratio > .5f ? 255 : static_cast<int>(ratio * 510);
        const int dx = portrait_x + 172 + static_cast<int>((left + offset_x) * .84f),
                  dy = portrait_y + 38 + static_cast<int>(top * .84f),
                  dr = portrait_x + 172 + static_cast<int>((right + offset_x) * .84f),
                  db = portrait_y + 38 + static_cast<int>(bottom * .84f);
        if (left < 0 || top < 0 || right > damage_atlas.width || bottom > damage_atlas.height ||
            right <= left || bottom <= top)
            return Result::invalid_argument;
        sprite_region(canvas, damage_atlas, {left, top, right - left, bottom - top},
                      {dx, dy, dr - dx, db - dy}, {red / 255.f, green / 255.f, 0, opacity});
    }
    text(530, 10, field(state, "CURRENT_TRACK_LABEL", "CURRENT TRACK") + ": ", false);
    text(530, 30, field(state, "TRACK_NAME", state.track), false);
    Configuration track;
    auto recommended = field(state, "RECOMMENDED_TIRES");
    if (recommended.empty() &&
        track.load(canvas.resources(), "Tracks/" + state.track + "/World/Data.txt") == Result::ok)
        recommended = track.value("AI", "TIRES");
    text(530, 50, field(state, "TIRES_LABEL", "TIRES") + ": " + recommended, false);
    const auto selection_color = [&](std::string_view prefix) {
        const auto key = std::string(prefix);
        float alpha{};
        setup.number("SELECTIONS", key + "OPACITY", alpha);
        return static_cast<std::uint32_t>(alpha * 255) << 24 |
               static_cast<std::uint32_t>(integer(setup, "SELECTIONS", key + "RED")) << 16 |
               static_cast<std::uint32_t>(integer(setup, "SELECTIONS", key + "GREEN")) << 8 |
               static_cast<std::uint32_t>(integer(setup, "SELECTIONS", key + "BLUE"));
    };
    const auto unselected_top = selection_color("SELECTIONFONTTOP");
    const auto unselected_bottom = selection_color("SELECTIONFONTBOTTOM");
    int max_width = 0;
    for (const auto& row : state.rows)
        max_width = std::max(max_width, canvas.text_width(FrontendFont::menu, row.label));
    const int x = integer(setup, "SELECTIONS", "SELECTIONSTARTX"),
              y = integer(setup, "SELECTIONS", "SELECTIONSTARTY"),
              indent_x = integer(setup, "SELECTIONS", "BORDERINDENTX"),
              indent_y = integer(setup, "SELECTIONS", "BORDERINDENTY"),
              spacing = integer(setup, "SELECTIONS", "SELECTIONSPACING"),
              height = canvas.font_height(FrontendFont::menu);
    float panel_alpha{};
    if (setup.number("SELECTIONS", "SELECTIONBACKGROUNDOPACITY", panel_alpha) != Result::ok)
        return Result::invalid_argument;
    const int count = static_cast<int>(state.rows.size());
    if (count > 20)
        return Result::invalid_argument;
    const auto store = store_directory(state.variant);
    if (count > 0 && store.empty()) {
        result = canvas.sprite("ModifyVehicle/16SelectionBackground.bmp",
                               {x - indent_x, y - indent_y, max_width + indent_x * 2,
                                height * count + spacing * (count - 1) + indent_y * 2},
                               panel_alpha);
        if (result != Result::ok)
            return result;
    }
    const auto color =
        0x96000000u |
        static_cast<std::uint32_t>(integer(character, "GARAGE", "FONTCOLORRED")) << 16 |
        static_cast<std::uint32_t>(integer(character, "GARAGE", "FONTCOLORGREEN")) << 8 |
        static_cast<std::uint32_t>(integer(character, "GARAGE", "FONTCOLORBLUE"));
    if (!store.empty()) {
        float selection_scale{}, description_scale{};
        if (setup.number("STORE", "SELECTIONFONTSCALE", selection_scale) != Result::ok ||
            setup.number("STORE", "DESCRIPTIONFONTSCALE", description_scale) != Result::ok ||
            selection_scale <= 0 || description_scale <= 0)
            return Result::invalid_argument;
        const int sx = integer(setup, "STORE", "SELECTIONRECTX"),
                  sy = integer(setup, "STORE", "SELECTIONRECTY"),
                  sw = integer(setup, "STORE", "SELECTIONRECTSIZEX"),
                  sh = integer(setup, "STORE", "SELECTIONRECTSIZEY");
        result =
            canvas.sprite("ModifyVehicle/16SelectionBackground.bmp", {sx, sy, sw, sh}, panel_alpha);
        if (result != Result::ok)
            return result;
        // 0043a208 reads MECHANICYOFFSET but uses the cached advice Y value.
        result = canvas.sprite(store + "/16Mechanic.bmp",
                               {sx + integer(setup, "STORE", "MECHANICXOFFSET"),
                                sy + integer(setup, "STORE", "ADVICESTARTYOFFSET"), 128, 128},
                               opacity);
        if (result != Result::ok)
            return result;
        const int store_spacing = integer(setup, "STORE", "SELECTIONSPACING");
        const int store_height = canvas.font_height(font, selection_scale);
        for (std::size_t i = 0; i < state.rows.size(); ++i) {
            const auto& row = state.rows[i];
            const bool selected = i == state.selected;
            if (row.values.empty() || row.selected >= row.values.size())
                continue;
            const auto row_y =
                static_cast<float>(y + static_cast<int>(i) * (store_height + store_spacing));
            const auto top = selected ? color : unselected_top;
            const auto bottom = selected ? 0x64ffff00 : unselected_bottom;
            canvas.text(font, static_cast<float>((sx + sx + sw) / 2), row_y,
                        row.values[row.selected], true, selection_scale, top, bottom);
            if (row.values.size() > 1) {
                canvas.text(font, static_cast<float>(sx + 10), row_y, "<", false, selection_scale,
                            top, bottom);
                canvas.text(font,
                            static_cast<float>(sx + sw - 10 -
                                               canvas.text_width(font, ">", selection_scale)),
                            row_y, ">", false, selection_scale, top, bottom);
            }
        }
        std::string path = store;
        for (const auto& row : state.rows) {
            if (row.label == "ACTION")
                break;
            if (!row.values.empty() && row.selected < row.values.size())
                path += "/" + row.values[row.selected];
        }
        path = field(state, "PRODUCT_RESOURCE", path);
        Configuration description;
        result = description.load(canvas.resources(), path + "/Data.ini");
        if (result != Result::ok)
            return result;
        const bool available = state.fields.contains("PRODUCT_AVAILABLE")
                                   ? field(state, "PRODUCT_AVAILABLE") == "1"
                                   : description.value("PRICE", "ISAVAILABLE") != "0";
        if (!available)
            return Result::ok;
        wrapped_text(canvas, field(state, "DESCRIPTION", description.value("DESCRIPTION", "TEXT")),
                     sx + integer(setup, "STORE", "DESCRIPTIONSTARTXOFFSET"),
                     sy + integer(setup, "STORE", "DESCRIPTIONSTARTYOFFSET"),
                     integer(setup, "STORE", "DESCRIPTIONSIZEX"),
                     integer(setup, "STORE", "DESCRIPTIONSPACING"), description_scale,
                     unselected_top, unselected_bottom);
        wrapped_text(canvas, field(state, "ADVICE", description.value("ADVICE", "TEXT")),
                     sx + integer(setup, "STORE", "ADVICESTARTXOFFSET"),
                     sy + integer(setup, "STORE", "ADVICESTARTYOFFSET"),
                     integer(setup, "STORE", "ADVICESIZEX"),
                     integer(setup, "STORE", "ADVICESPACING"), description_scale, unselected_top,
                     unselected_bottom);
        if (!state.preview_data && field(state, "TRADE IN VISIBLE") == "1") {
            canvas.text(font, static_cast<float>(sx + 50), static_cast<float>(sy + 275),
                        field(state, "TRADE_LABEL", "TRADE IN") + ": $" + field(state, "TRADE IN"),
                        false, description_scale, 0x64ffff00, 0x64ffff00);
        }
        if (!state.preview_data) {
            if (field(state, "AMMO") != "0")
                canvas.text(font, static_cast<float>(sx + 50), static_cast<float>(sy + 295),
                            field(state, "AMMO_LABEL", "AMMO") + ": " + field(state, "AMMO"), false,
                            description_scale, 0x5affff00, 0x64ffff00);
        }
    } else
        for (std::size_t i = 0; i < state.rows.size(); ++i) {
            const bool selected = i == state.selected;
            canvas.text(FrontendFont::menu, static_cast<float>(x),
                        static_cast<float>(y + static_cast<int>(i) * (height + spacing)),
                        state.rows[i].label, false, 1, selected ? color : unselected_top,
                        selected ? 0x64ffff00 : unselected_bottom, FrontendVerticalAlign::top, 5);
        }
    if (state.preview_data)
        canvas.text(FrontendFont::small, 320, 330, "PREVIEW DATA", true, 1, 0xff80ffff, 0xff80ffff);
    return Result::ok;
}
} // namespace

// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00475168
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00475c41
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00476741
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0047700b
Result frontend_custom_rows(ResourceStore& resources, std::string_view screen,
                            std::vector<FrontendOptionRow>& output, std::string& error,
                            std::string_view variant, std::size_t item_index) {
    std::vector<FrontendOptionRow> rows;
    Configuration config;
    auto result = config.load(resources, "Ini/RoadWars.ini");
    if (result != Result::ok) {
        error = "Frontend options: Ini/RoadWars.ini";
        return result;
    }
    const auto number = [&](std::string_view key, int fallback = 0) {
        return integer(config, "ROADWARS", key, fallback);
    };
    const auto yes_no = [&](const char* label, const char* key) {
        add(rows, label, {"YES", "NO"}, number(key) ? 0 : 1);
    };
    if (screen == "sound") {
        yes_no("PLAY MUSIC FROM CD", "PLAYMUSICFROMCD");
        for (const auto& pair : std::array<std::pair<const char*, const char*>, 3>{
                 {{"MUSIC", "MUSIC"}, {"SFX", "SFX"}, {"RACE MUSIC", "INGAMEMUSIC"}}}) {
            float value{};
            if (config.number("ROADWARS", pair.second, value) != Result::ok ||
                !std::isfinite(value) || value < 0 || value > 1) {
                error = "Missing sound option";
                return Result::invalid_argument;
            }
            add(rows, pair.first, percentages(), static_cast<int>(std::round(value * 20)));
        }
        yes_no("USE 3D ACCELERATION", "USE3DSOUNDHARDWARE");
        add(rows, "SOUND CHANNELS", {"8", "12", "16", "20", "24", "28", "32"},
            (number("SOUNDCHANNELS", 20) - 8) / 4);
    } else if (screen == "game") {
        add(rows, "USER NAME", {config.value("ROADWARS", "USERNAME")});
        add(rows, "TRANSMISSION", {"MANUAL", "AUTOMATIC"}, number("MANUALTRANSMISSION") ? 0 : 1);
        add(rows, "DIFFICULTY LEVEL", {"EASY", "MEDIUM", "HARD", "VERY HARD"},
            number("DIFFICULTY_LEVEL"));
        add(rows, "CHEAT", {""});
    } else if (screen == "graphics") {
        // Runtime rows are supplied by the modern graphics host transaction.
        add(rows, "GRAPHICS SETTINGS", {"HOST UNAVAILABLE"});
        rows.back().available = false;
        rows.back().id = "unavailable";
    } else if (screen == "keyboard" || screen == "joystick") {
        const bool joystick = screen == "joystick";
        Configuration inputs;
        result = inputs.load(resources, joystick ? "Controllers/Joystick/Data.ini"
                                                 : "Controllers/Keyboard/Data.ini");
        if (result != Result::ok) {
            error = "Frontend controller configuration";
            return result;
        }
        if (joystick) {
            yes_no("ENABLE JOYSTICK", "JOYSTICKENABLED");
            add(rows, "DEADZONE", percentages(), number("JOYSTICKDEADZONE") / 5);
            add(rows, "STEERING SENSITIVITY", percentages(),
                number("STEERING_SENSITIVITY_PCT") / 5);
        }
        constexpr std::array<const char*, 13> keys{"ACCELERATE",
                                                   "BRAKE",
                                                   "RIGHT",
                                                   "LEFT",
                                                   "DISPLAY",
                                                   "ACTIVATE_WEAPON",
                                                   "UPSHIFT",
                                                   "DOWNSHIFT",
                                                   "ACTIVATE_FRONT_WEAPON",
                                                   "ACTIVATE_SIDE_WEAPON",
                                                   "ACTIVATE_REAR_WEAPON",
                                                   "ACTIVATE_NITROUS_BOOST",
                                                   "DO_REAR_CAMERA_VIEW"};
        constexpr std::array<const char*, 13> labels{"ACCELERATE",
                                                     "BRAKE-REVERSE",
                                                     "RIGHT",
                                                     "LEFT",
                                                     "DISPLAY",
                                                     "ACTIVATE WEAPON",
                                                     "UP SHIFT",
                                                     "DOWN SHIFT",
                                                     "ACTIVATE FRONT WEAPON",
                                                     "ACTIVATE SIDE WEAPON",
                                                     "ACTIVATE REAR WEAPON",
                                                     "ACTIVATE NITROUS BOOST",
                                                     "REAR CAMERA"};
        for (std::size_t i = 0; i < keys.size(); ++i) {
            const auto code = integer(inputs, "INPUTS", keys[i], -1);
            add(rows, labels[i], {joystick ? joystick_name(code) : keyboard_name(code)});
        }
    } else if (screen == "modify_vehicle") {
        const auto store = store_directory(variant);
        if (!store.empty()) {
            result = config.load(resources, store + "/Data.ini");
            if (result != Result::ok) {
                error = "Frontend garage store configuration";
                return result;
            }
            const int count = integer(config, "ORDERING", "NUMITEMS");
            if (count < 1 || count > 64)
                return Result::invalid_argument;
            std::vector<std::string> items;
            for (int i = 0; i < count; ++i)
                items.push_back(config.value("ORDERING", "ITEM" + std::to_string(i)));
            if (item_index >= items.size())
                return Result::invalid_argument;
            add(rows, "ITEM", std::move(items), static_cast<int>(item_index));
            result =
                config.load(resources, store + "/" + rows.front().values[item_index] + "/Data.ini");
            if (result != Result::ok)
                return result;
            const int qualities = integer(config, "ORDERING", "NUMITEMS");
            if (qualities < 0 || qualities > 64)
                return Result::invalid_argument;
            if (qualities) {
                std::vector<std::string> names;
                for (int i = 0; i < qualities; ++i)
                    names.push_back(config.value("ORDERING", "ITEM" + std::to_string(i)));
                add(rows, "QUALITY", std::move(names));
            }
            add(rows, "ACTION", {"BUY"});
            output = std::move(rows);
            return Result::ok;
        }
        result = config.load(resources, "ModifyVehicle/Data.ini");
        if (result != Result::ok) {
            error = "Frontend garage configuration";
            return result;
        }
        const int count = integer(config, "SELECTIONS", "NUMSELECTIONS");
        if (count < 1 || count > 20)
            return Result::invalid_argument;
        for (int i = 0; i < count; ++i)
            add(rows, config.value("SELECTIONS", "SELECTION" + std::to_string(i)), {});
    } else if (screen == "start_race_summary") {
        add(rows, "LAPS", {"3"});
        add(rows, "MILES", {"2.50"});
        add(rows, "TIRES", {"STREET"});
    } else if (screen == "create_season" || screen == "load_season" || screen == "select_session") {
        // Explicit fixtures. The caller can supply empty or populated inventories;
        // no filesystem enumeration or DirectPlay operation is implied here.
        if (screen == "create_season")
            add(rows, "NEW SEASON", {});
        else if (screen == "load_season")
            add(rows, "PREVIEW SEASON", {});
        else
            add(rows, "PREVIEW SESSION [1/6]", {});
    } else if (screen == "pause") {
        const std::string variant_name = variant.empty() ? "Single Race" : std::string(variant);
        if (variant_name.find('/') != std::string::npos ||
            variant_name.find('\\') != std::string::npos ||
            variant_name.find("..") != std::string::npos ||
            variant_name.find(':') != std::string::npos)
            return Result::invalid_argument;
        result = config.load(resources, "Game Menu/" + variant_name + ".ini");
        if (result != Result::ok) {
            error = "Frontend pause configuration";
            return result;
        }
        const int count = integer(config, "INFO", "NUMSELECTIONS");
        if (count < 1 || count > 20)
            return Result::invalid_argument;
        for (int i = 0; i < count; ++i) {
            const auto key = "SELECTION" + std::to_string(i);
            add(rows, config.value("INFO", key), {});
            rows.back().available = integer(config, "INFO", key + "_ISAVAILABLE") != 0;
        }
    } else {
        return Result::not_found;
    }
    output = std::move(rows);
    return Result::ok;
}
bool FrontendCustom::supports(std::string_view screen) {
    for (const auto name :
         {"graphics", "sound", "game", "keyboard", "joystick", "start_race_summary", "race_results",
          "standings", "season_results", "loading", "dialog", "pause", "create_season",
          "load_season", "select_session", "host_or_join", "modify_vehicle", "victory", "defeat"})
        if (screen == name)
            return true;
    return false;
}
// These mappings recover presentation subsets only; business and legacy device paths remain open.
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0047837d
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0048fd6d
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004a682a
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00494a1d
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004aa5f3
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004afd70
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004ac5a6
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004582cb
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00472810
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00452e80
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0043d79c
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0044000a
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004c9db0
Result FrontendCustom::draw(FrontendCanvas& canvas, const FrontendCustomState& state,
                            std::string& error) {
    if (!std::isfinite(state.elapsed) || state.elapsed < 0 ||
        !std::isfinite(state.loading_progress) || state.loading_progress < 0 ||
        state.loading_progress > 1)
        return Result::invalid_argument;
    Result result = Result::unavailable;
    if (state.screen == "graphics" || state.screen == "sound" || state.screen == "game" ||
        state.screen == "keyboard" || state.screen == "joystick")
        result = draw_options(canvas, state);
    else if (state.screen == "race_results" || state.screen == "standings" ||
             state.screen == "season_results")
        result = draw_results(canvas, state);
    else if (state.screen == "start_race_summary")
        result = draw_summary(canvas, state);
    else if (state.screen == "dialog")
        result = draw_dialog(canvas, state);
    else if (state.screen == "pause")
        result = draw_pause(canvas, state);
    else if (state.screen == "create_season" || state.screen == "load_season" ||
             state.screen == "select_session")
        result = draw_list(canvas, state);
    else if (state.screen == "host_or_join")
        result = draw_host_or_join(canvas, state);
    else if (state.screen == "modify_vehicle")
        result = draw_garage(canvas, state);
    else if (state.screen == "victory" || state.screen == "defeat") {
        // The room, vehicles, podium and 3D cash are rendered by FrontendScene.
        const auto count = std::min<std::size_t>(3, state.racers.size());
        const int strip_width =
            static_cast<int>(count) * 33 + std::max(0, static_cast<int>(count) - 1) * 10;
        const int first = 320 - strip_width / 2;
        double total_distance = 0;
        for (std::size_t i = 0; i < count; ++i)
            total_distance += 640 - (first + static_cast<int>(i) * 43);
        // Sampled room metadata uses CamStart_2; its recovered denominator is 2-1.
        // The camera renderer recovers the same dummy duration from the original mesh.
        const double speed = total_distance;
        double owed = std::max(0., state.elapsed - .3) * speed;
        for (std::size_t i = 0; i < count; ++i) {
            const int target = first + static_cast<int>(i) * 43;
            const int distance = 641 - target;
            const int moved = static_cast<int>(std::min(owed, static_cast<double>(distance)));
            const int left = 641 - moved;
            owed = std::max(0., owed - distance);
            FrontendSprite portrait;
            result = original_size(
                canvas, "Characters/" + state.racers[i].character + "/16RaceSummaryPortrait.bmp",
                left, 428, portrait, .7f);
            if (result != Result::ok)
                return result;
            if (left == target)
                canvas.text(FrontendFont::small, static_cast<float>(target + 16),
                            static_cast<float>(418 - canvas.font_height(FrontendFont::small) / 2),
                            std::to_string(state.racers[i].points), true, 1, 0x64ffffff,
                            0x64ffffff);
        }
        canvas.text(FrontendFont::small, 320,
                    static_cast<float>(240 - canvas.font_height(FrontendFont::small) / 2),
                    "PRESS ANY KEY TO CONTINUE", true, 1, 0x64ffffff, 0x64ffffff);
        if (state.preview_data)
            canvas.text(FrontendFont::small, 320, 470, "PREVIEW DATA", true, 1, 0xff80ffff,
                        0xff80ffff);
        result = Result::ok;
    } else if (state.screen == "loading") {
        result =
            !state.preview_data && state.fields.contains("CHARACTER")
                ? canvas.sprite("Characters/" + state.fields.at("CHARACTER") + "/16Portrait.bmp",
                                {0, 0, 640, 480})
                : background(canvas);
        canvas.text(FrontendFont::small, 41, 352, "LOADING");
        canvas.text(FrontendFont::small, 320,
                    static_cast<float>(449 - canvas.font_height(FrontendFont::small)),
                    state.message, true, 1, 0x64ffffff, 0x64ffffff);
        const int width = static_cast<int>(state.loading_progress * 563);
        if (result == Result::ok && width > 0)
            result = canvas.sprite("Screens/16LoadingBar.bmp", {39, 454, width, 8}, .8f);
        preview_label(canvas, state);
    }
    if (result != Result::ok)
        error = "Frontend custom screen " + state.screen + ": " + canvas.error();
    return result;
}
} // namespace roadwars
