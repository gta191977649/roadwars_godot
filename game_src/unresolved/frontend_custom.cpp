#include "game_src/unresolved/frontend_custom.hpp"
#include "game_src/unresolved/frontend_input_names.hpp"
#include <algorithm>
#include <cmath>
#include <map>

namespace roadwars {
namespace {
constexpr std::uint32_t kRed = 0xffff0000, kYellow = 0xffffff00, kWhite = 0xffffffff;
std::string uppercase(std::string value) { return ResourceStore::canonical(value); }
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
    auto result = canvas.asset(
        std::string(root) + "/" + std::string(label) + "/16SelectionIcon.bmp", icon);
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
    const int step = canvas.font_height(FrontendFont::body) + 5;
    const int top = 240 - static_cast<int>(state.rows.size()) * step / 2;
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
        canvas.text(FrontendFont::body, 30, y, row.label, false, 1, color, bottom);
        if (!row.values.empty()) {
            if (row.selected >= row.values.size())
                return Result::invalid_argument;
            canvas.text(FrontendFont::body, 430, y, row.values[row.selected], true, 1, color,
                        bottom);
            if (row.values.size() > 1) {
                canvas.text(FrontendFont::body, 310, y, "<", false, 1, color, bottom);
                canvas.text(FrontendFont::body, 610, y, ">", false, 1, color, bottom);
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
Result draw_dialog(FrontendCanvas& canvas, const FrontendCustomState& state) {
    FrontendSprite panel, button;
    auto result = canvas.asset("Screens/16DialogBackground.bmp", panel);
    if (result != Result::ok)
        return result;
    result = canvas.asset("Screens/16DialogButton.bmp", button);
    if (result != Result::ok)
        return result;
    const int left = 320 - panel.width / 2, top = 240 - panel.height / 2;
    canvas.sprite(panel, {left, top, panel.width, panel.height}, .6f);
    canvas.text(FrontendFont::small, 320, static_cast<float>(top + 39), state.message, true);
    const int by = top + 55;
    canvas.sprite(button, {315 - button.width, by, button.width, button.height});
    canvas.sprite(button, {325, by, button.width, button.height});
    const auto green = 0xff00ff00;
    canvas.text(FrontendFont::small, static_cast<float>(315 - button.width / 2),
                static_cast<float>(by + button.height / 2), "YES", true, 1,
                state.selected == 0 ? green : kWhite, state.selected == 0 ? green : kWhite);
    canvas.text(FrontendFont::small, static_cast<float>(325 + button.width / 2),
                static_cast<float>(by + button.height / 2), "NO", true, 1,
                state.selected == 1 ? green : kWhite, state.selected == 1 ? green : kWhite);
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
void wrapped_text(FrontendCanvas& canvas, const std::string& value, int x, int y, int width,
                  int height, int spacing) {
    std::string line, word;
    const int line_height = canvas.font_height(FrontendFont::nebula) + spacing;
    const int bottom = y + height;
    const auto emit = [&]() {
        if (y + line_height <= bottom)
            canvas.text(FrontendFont::nebula, static_cast<float>(x), static_cast<float>(y), line);
        y += line_height;
        line.clear();
    };
    for (std::size_t i = 0; i <= value.size(); ++i) {
        if (i < value.size() && value[i] != ' ' && value[i] != '\n' && value[i] != '\r') {
            word += value[i];
            continue;
        }
        if (!line.empty() && canvas.text_width(FrontendFont::nebula, line + " " + word) > width)
            emit();
        if (!word.empty()) {
            if (!line.empty())
                line += ' ';
            line += word;
            word.clear();
        }
        if (i < value.size() && value[i] == '\n')
            emit();
    }
    if (!line.empty())
        emit();
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
    const auto font = FrontendFont::nebula;
    const auto text = [&](int x, int y, const std::string& value, bool center = true) {
        canvas.text(font, static_cast<float>(x), static_cast<float>(y), value, center, 1,
                    0xe5ffffff, 0xe5ffffff);
    };
    text(portrait_x + 168, portrait_y + 23,
         uppercase(state.racers.empty() ? name : state.racers.front().name));
    const auto cash = state.racers.empty() ? field(state, "CASH", "PREVIEW")
                                           : std::to_string(state.racers.front().cash);
    text(portrait_x + 168, portrait_y + 40, "$" + cash);
    text(panel_x + 99, panel_y + 27, field(state, "DRIVE TRAIN", "PREVIEW"));
    text(panel_x + 61, panel_y + 60, field(state, "TIRES", "PREVIEW"));
    text(panel_x + 61, panel_y + 94, field(state, "ARMOR", "PREVIEW"));
    text(panel_x + 128 + 37, panel_y + 62, field(state, "TRANSMISSION", "PREVIEW"));
    for (const auto& item :
         std::array<std::pair<const char*, int>, 3>{{{"FRONT", 31}, {"SIDE", 65}, {"REAR", 100}}})
        text(weapon_x + 52, weapon_y + item.second, field(state, item.first, "NONE"));
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
        const int sx = integer(setup, "STORE", "SELECTIONRECTX"),
                  sy = integer(setup, "STORE", "SELECTIONRECTY"),
                  sw = integer(setup, "STORE", "SELECTIONRECTSIZEX"),
                  sh = integer(setup, "STORE", "SELECTIONRECTSIZEY");
        result =
            canvas.sprite("ModifyVehicle/16SelectionBackground.bmp", {sx, sy, sw, sh}, panel_alpha);
        if (result != Result::ok)
            return result;
        const int store_spacing = integer(setup, "STORE", "SELECTIONSPACING");
        for (std::size_t i = 0; i < state.rows.size(); ++i) {
            const auto& row = state.rows[i];
            const bool selected = i == state.selected;
            if (row.values.empty() || row.selected >= row.values.size())
                continue;
            const auto row_y =
                static_cast<float>(y + static_cast<int>(i) * (height + store_spacing));
            canvas.text(FrontendFont::menu, static_cast<float>(sx + sw / 2), row_y,
                        row.values[row.selected], true, 1, selected ? color : 0x66ffffff,
                        selected ? 0x64ffff00 : 0x66ffffff);
            if (row.values.size() > 1) {
                canvas.text(FrontendFont::menu, static_cast<float>(sx + 10), row_y, "<");
                canvas.text(
                    FrontendFont::menu,
                    static_cast<float>(sx + sw - 10 - canvas.text_width(FrontendFont::menu, ">")),
                    row_y, ">");
            }
        }
        std::string path = store;
        for (const auto& row : state.rows) {
            if (row.label == "ACTION")
                break;
            if (!row.values.empty() && row.selected < row.values.size())
                path += "/" + row.values[row.selected];
        }
        Configuration description;
        result = description.load(canvas.resources(), path + "/Data.ini");
        if (result != Result::ok)
            return result;
        wrapped_text(canvas, description.value("DESCRIPTION", "TEXT"),
                     sx + integer(setup, "STORE", "DESCRIPTIONSTARTXOFFSET"),
                     sy + integer(setup, "STORE", "DESCRIPTIONSTARTYOFFSET"),
                     integer(setup, "STORE", "DESCRIPTIONSIZEX"),
                     integer(setup, "STORE", "DESCRIPTIONSIZEY"),
                     integer(setup, "STORE", "DESCRIPTIONSPACING"));
        wrapped_text(canvas, description.value("ADVICE", "TEXT"),
                     sx + integer(setup, "STORE", "ADVICESTARTXOFFSET"),
                     sy + integer(setup, "STORE", "ADVICESTARTYOFFSET"),
                     integer(setup, "STORE", "ADVICESIZEX"),
                     integer(setup, "STORE", "ADVICESIZEY", 190),
                     integer(setup, "STORE", "ADVICESPACING"));
    } else
        for (std::size_t i = 0; i < state.rows.size(); ++i) {
            const bool selected = i == state.selected;
            canvas.text(FrontendFont::menu, static_cast<float>(x),
                        static_cast<float>(y + static_cast<int>(i) * (height + spacing)),
                        state.rows[i].label, false, 1, selected ? color : 0x66ffffff,
                        selected ? 0x64ffff00 : 0x66ffffff);
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
            if (config.number("ROADWARS", pair.second, value) != Result::ok) {
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
        // Original DirectDraw enumeration is unavailable to the portable core.
        // A read-only display description is explicitly not a recovered device list.
        add(rows, "DISPLAY", {"UNAVAILABLE"});
        rows.back().available = false;
        add(rows, "RESOLUTION",
            {std::to_string(number("SCREENX", 640)) + " X " +
             std::to_string(number("SCREENY", 480))});
        add(rows, "TRIPLE BUFFER", {"OFF", "ON"}, number("BUFFERCOUNT") > 1 ? 1 : 0);
        add(rows, "3D NOW", {"OFF", "ON"}, number("3DNOW") ? 1 : 0);
        float distance{};
        config.number("ROADWARS", "VIEWDISTANCESCALAR", distance);
        const std::array<float, 4> distances{.4f, .5f, .75f, 1.f};
        const auto nearest =
            std::min_element(distances.begin(), distances.end(), [&](float a, float b) {
                return std::abs(distance - a) < std::abs(distance - b);
            });
        add(rows, "VIEW DISTANCE", {"NEAR", "MEDIUM", "FAR", "VERY FAR"},
            static_cast<int>(nearest - distances.begin()));
        add(rows, "TEXTURE RESOLUTION", {"HIGHER", "LOWER"}, number("LOWRES"));
        yes_no("COCKPIT IN HARDWARE", "COCKPIT_IN_HARDWARE");
        for (const auto& pair : std::array<std::pair<const char*, const char*>, 2>{
                 {{"SPECIAL EFFECTS", "SPECIAL_EFFECTS_SCALAR"},
                  {"SKIDMARK DETAIL", "SKIDMARK_SCALAR"}}}) {
            float value{};
            config.number("ROADWARS", pair.second, value);
            const std::array<float, 3> scalars{.33f, .66f, 1.f};
            const auto closest =
                std::min_element(scalars.begin(), scalars.end(), [&](float a, float b) {
                    return std::abs(value - a) < std::abs(value - b);
                });
            add(rows, pair.first, {"LOW", "MEDIUM", "HIGH"},
                static_cast<int>(closest - scalars.begin()));
        }
        add(rows, "CHROME MAPPING", {"OFF", "ON"}, number("ENVIRONMENT_MAPPING"));
        add(rows, "CAR DETAIL", {"LOW", "MEDIUM", "HIGH"}, number("CAR_DETAIL", 2));
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
