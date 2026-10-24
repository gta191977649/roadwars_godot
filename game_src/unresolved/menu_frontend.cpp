#include "game_src/unresolved/menu_frontend.hpp"
#include "game_src/unresolved/frontend_custom.hpp"
#include "game_src/unresolved/frontend_dialog.hpp"
#include "game_src/unresolved/frontend_localization.hpp"
#include "game_src/unresolved/frontend_scene.hpp"
#include "game_src/unresolved/frontend_video.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace roadwars {
namespace {
constexpr FrontendScreenInfo kScreens[] = {
    {"publisher_1", "", ""},
    {"publisher_2", "", ""},
    {"intro", "Screens/Intro/Data.ini", ""},
    {"title", "Screens/Title/Data.ini", ""},
    {"main_menu", "Screens/SelectGameOption/Data.ini", "Game Options"},
    {"select_option", "Screens/Select Option/Data.ini", "Options"},
    {"select_controller", "Screens/Select Controller/Data.ini", "Controllers"},
    {"select_character", "Screens/SelectCharacter/Data.ini", "Characters"},
    {"select_track", "Screens/SelectTrack/Data.ini", "Tracks"},
    {"season_type", "Screens/Select Season Race Type/Data.ini", "Season Types"},
    {"select_league", "Screens/Select League/Data.ini", "Season Leagues"},
    {"host_or_join", "Screens/Host Or Join/Data.ini", "Host Or Join", true},
    {"select_connection", "Screens/Select Connection/Data.ini", "Connections"},
    {"select_session", "Screens/Select Session/Data.ini", "", true},
    {"create_season", "Screens/Create Season/Data.ini", "", true},
    {"load_season", "Screens/Load Season/Data.ini", "", true},
    {"graphics", "", "", true},
    {"sound", "", "", true},
    {"game", "", "", true},
    {"keyboard", "", "", true},
    {"joystick", "", "", true},
    {"start_race_summary", "Screens/StartRaceSummary/Data.ini", "StartRaceSummary", true},
    {"race_results", "Screens/Race Results/Data.ini", "Race Results", true},
    {"standings", "Screens/Standings/Data.ini", "Standings", true},
    {"season_results", "Screens/Season Results/Data.ini", "Season Results", true},
    {"loading", "", "", true},
    {"dialog", "", "", true},
    {"pause", "", "", true},
    {"modify_vehicle", "", "", true},
    {"victory", "", "", true},
    {"defeat", "", "", true},
    {"info", "", ""},
};
const FrontendScreenInfo* find_screen(std::string_view id) {
    for (const auto& info : kScreens)
        if (info.id == id)
            return &info;
    return nullptr;
}
} // namespace
std::span<const FrontendScreenInfo> MenuFrontend::screens() { return kScreens; }
MenuFrontend::MenuFrontend(ResourceStore& resources, Renderer& renderer,
                           GraphicsSettingsHost* graphics)
    : resources_(resources), renderer_(renderer), graphics_host_(graphics),
      canvas_(resources, renderer) {}
MenuFrontend::~MenuFrontend() { canvas_.release(video_frame_); }
namespace {
std::string backend_name(RenderingBackend backend) {
    switch (backend) {
    case RenderingBackend::automatic:
        return "AUTO";
    case RenderingBackend::vulkan:
        return "VULKAN";
    case RenderingBackend::direct3d12:
        return "DIRECTX 12";
    case RenderingBackend::direct3d11:
        return "DIRECTX 11";
    case RenderingBackend::metal:
        return "METAL";
    }
    return "UNAVAILABLE";
}
template <class T, std::size_t N>
void cycle_value(T& value, const std::array<T, N>& choices, int dx) {
    const auto found = std::find(choices.begin(), choices.end(), value);
    const auto index = found == choices.end() ? 0 : found - choices.begin();
    value = choices[(index + N + dx) % N];
}
template <class T> void cycle_value(T& value, const std::vector<T>& choices, int dx) {
    if (choices.empty())
        return;
    const auto found = std::find(choices.begin(), choices.end(), value);
    const auto index = found == choices.end() ? 0 : found - choices.begin();
    value = choices[(index + choices.size() + dx) % choices.size()];
}
} // namespace
void MenuFrontend::refresh_graphics_rows() {
    custom_.rows.clear();
    custom_.fields["MODERN_GRAPHICS"] =
        graphics_host_ ? "CURRENT API: " + backend_name(graphics_host_->active_backend()) +
                             "  SAVED: " + backend_name(graphics_host_->saved_backend())
                       : "GRAPHICS HOST UNAVAILABLE";
    const auto add = [&](std::string id, std::string label, std::string value,
                         bool enabled = true) {
        custom_.rows.push_back({std::move(label), {std::move(value)}, 0, enabled, std::move(id)});
    };
    if (!graphics_) {
        add("unavailable", "GRAPHICS SETTINGS", "UNAVAILABLE", false);
        add("back", "BACK", "");
        return;
    }
    const auto& v = graphics_->draft();
    const auto caps = graphics_host_->capabilities();
    if (graphics_->pending_confirmation()) {
        custom_.message = "KEEP DISPLAY SETTINGS? " +
                          std::to_string(static_cast<int>(
                              std::ceil(graphics_->seconds_remaining(graphics_time_)))) +
                          " SECONDS";
        add("confirm", "KEEP AND SAVE", "");
        add("revert", "REVERT", "");
    } else {
        if (graphics_quality_)
            add("page", "PAGE", "QUALITY");
        if (!graphics_quality_) {
            const auto valid_monitor =
                v.monitor >= 0 && static_cast<std::size_t>(v.monitor) < caps.monitors.size();
            add("monitor", "DISPLAY", valid_monitor ? caps.monitors[v.monitor].name : "UNAVAILABLE",
                !caps.monitors.empty());
            add("resolution", "RESOLUTION",
                std::to_string(v.width) + " X " + std::to_string(v.height),
                v.window_mode == WindowMode::windowed);
            add("window_mode", "WINDOW MODE",
                v.window_mode == WindowMode::windowed     ? "WINDOWED"
                : v.window_mode == WindowMode::borderless ? "BORDERLESS FULLSCREEN"
                                                          : "FULLSCREEN");
            add("vsync", "VSYNC", v.vsync ? "ON" : "OFF");
            add("fps_limit", "FPS LIMIT", v.fps_limit ? std::to_string(v.fps_limit) : "UNLIMITED");
            add("backend", "RENDERING API", backend_name(v.backend), caps.backends.size() > 1);
            add("page", "PAGE", "DISPLAY");
        } else {
            constexpr std::array<const char*, 5> aa{"OFF", "FXAA", "MSAA 2X", "MSAA 4X", "MSAA 8X"};
            constexpr std::array<const char*, 5> filter{
                "LINEAR", "ANISOTROPIC 2X", "ANISOTROPIC 4X", "ANISOTROPIC 8X", "ANISOTROPIC 16X"};
            constexpr std::array<const char*, 3> quality{"HIGH", "MEDIUM", "LOW"};
            add("aa", "ANTI ALIASING", aa[static_cast<int>(v.aa)], caps.anti_aliasing.size() > 1);
            add("render_scale", "RENDER SCALE",
                std::to_string(static_cast<int>(std::lround(v.render_scale * 100))) + "%");
            add("texture_quality", "TEXTURE QUALITY", quality[static_cast<int>(v.texture_quality)],
                caps.texture_quality);
            add("texture_filtering", "TEXTURE FILTERING",
                filter[static_cast<int>(v.texture_filtering)], caps.texture_filtering);
            add("view_distance", "VIEW DISTANCE",
                std::to_string(static_cast<int>(std::lround(v.view_distance * 100))) + "%");
            add("effects", "SPECIAL EFFECTS",
                v.effects < .5f   ? "LOW"
                : v.effects < .9f ? "MEDIUM"
                                  : "HIGH");
            add("skid_marks", "SKIDMARK DETAIL",
                v.skid_marks < .5f   ? "LOW"
                : v.skid_marks < .9f ? "MEDIUM"
                                     : "HIGH");
            add("car_detail", "CAR DETAIL",
                v.car_detail == 0   ? "LOW"
                : v.car_detail == 1 ? "MEDIUM"
                                    : "HIGH",
                caps.car_detail);
            add("chrome_mapping", "CHROME MAPPING", v.chrome_mapping ? "ON" : "OFF");
        }
        add("apply", "APPLY AND SAVE", "");
        add("defaults", "RESTORE DEFAULTS", "");
        if (graphics_host_->restart_required())
            add("restart", "RESTART NOW", "API CHANGE SAVED");
        add("back", "BACK", "");
        if (custom_.message.empty() && graphics_host_->restart_required())
            custom_.message = "SAVED RENDERING API WILL APPLY AFTER RESTART";
    }
    selected_ = std::clamp(selected_, 0, static_cast<int>(custom_.rows.size()) - 1);
}
void MenuFrontend::edit_graphics(std::string_view id, int dx) {
    if (!graphics_)
        return;
    auto& v = graphics_->draft();
    const auto caps = graphics_host_->capabilities();
    if (id == "page") {
        graphics_quality_ = !graphics_quality_;
        selected_ = 0;
    } else if (id == "monitor" && !caps.monitors.empty())
        v.monitor = (v.monitor + static_cast<int>(caps.monitors.size()) + dx) %
                    static_cast<int>(caps.monitors.size());
    else if (id == "window_mode")
        cycle_value(
            v.window_mode,
            std::array{WindowMode::windowed, WindowMode::borderless, WindowMode::fullscreen}, dx);
    else if (id == "resolution") {
        std::vector<std::pair<int, int>> sizes{{640, 480},   {960, 540},   {1280, 720}, {1600, 900},
                                               {1920, 1080}, {2560, 1440}, {3840, 2160}};
        const auto current = std::pair{v.width, v.height};
        if (std::find(sizes.begin(), sizes.end(), current) == sizes.end())
            sizes.push_back(current);
        auto size = current;
        cycle_value(size, sizes, dx);
        v.width = size.first;
        v.height = size.second;
    } else if (id == "vsync")
        v.vsync = !v.vsync;
    else if (id == "fps_limit")
        cycle_value(v.fps_limit, std::array{0, 30, 60, 90, 120, 144, 165, 240}, dx);
    else if (id == "backend")
        cycle_value(v.backend, caps.backends, dx);
    else if (id == "aa")
        cycle_value(v.aa, caps.anti_aliasing, dx);
    else if (id == "render_scale")
        cycle_value(v.render_scale, std::array{.5f, .67f, .75f, .85f, 1.f}, dx);
    else if (id == "texture_quality")
        cycle_value(v.texture_quality,
                    std::array{TextureQuality::high, TextureQuality::medium, TextureQuality::low},
                    dx);
    else if (id == "texture_filtering")
        cycle_value(v.texture_filtering,
                    std::array{TextureFiltering::linear, TextureFiltering::anisotropic_2x,
                               TextureFiltering::anisotropic_4x, TextureFiltering::anisotropic_8x,
                               TextureFiltering::anisotropic_16x},
                    dx);
    else if (id == "view_distance")
        cycle_value(v.view_distance, std::array{.4f, .5f, .75f, 1.f}, dx);
    else if (id == "effects")
        cycle_value(v.effects, std::array{.33f, .66f, 1.f}, dx);
    else if (id == "skid_marks")
        cycle_value(v.skid_marks, std::array{.33f, .66f, 1.f}, dx);
    else if (id == "car_detail")
        cycle_value(v.car_detail, std::array{0, 1, 2}, dx);
    else if (id == "chrome_mapping")
        v.chrome_mapping = !v.chrome_mapping;
}
Result MenuFrontend::advance_graphics(double elapsed, const FrontendInput& input,
                                      std::string& error) {
    graphics_time_ += elapsed;
    if (graphics_ && graphics_host_) {
        const auto live = graphics_host_->current_settings();
        if (live.window_mode == WindowMode::windowed)
            graphics_->observe_window_size(live.width, live.height);
    }
    const auto edge = [](bool now, bool previous) { return now && !previous; };
    const auto leave = [&]() {
        auto target = history_.empty() ? std::string("main_menu") : history_.back();
        if (!history_.empty())
            history_.pop_back();
        return enter(target, error);
    };
    Result result = Result::ok;
    if (graphics_discard_) {
        if (edge(input.left, previous_.left))
            graphics_save_yes_ = true;
        if (edge(input.right, previous_.right))
            graphics_save_yes_ = false;
        bool hit = false;
        if (input.pointer_moved || input.pointer_accept) {
            FrontendDialogLayout layout;
            const auto layout_result = frontend_dialog_layout(canvas_, false, layout);
            if (layout_result != Result::ok)
                return layout_result;
            const int button = layout.hit(input.pointer_x, input.pointer_y);
            if (button >= 0) {
                graphics_save_yes_ = button == 0;
                hit = true;
            }
        }
        const bool choose = edge(input.accept, previous_.accept) ||
                            (hit && edge(input.pointer_accept, previous_.pointer_accept));
        if (edge(input.back, previous_.back))
            graphics_discard_ = false;
        else if (choose) {
            graphics_discard_ = false;
            if (!graphics_save_yes_) {
                graphics_->discard_draft();
                previous_ = input;
                return leave();
            }
            result = graphics_->apply(graphics_time_);
            if (result == Result::ok && !graphics_->pending_confirmation()) {
                previous_ = input;
                return leave();
            }
            graphics_leave_after_save_ = result == Result::ok;
            custom_.message = result == Result::ok ? "" : "SETTINGS COULD NOT BE SAVED";
            selected_ = 0;
        }
        previous_ = input;
        refresh_graphics_rows();
        return draw(error);
    }
    if (graphics_) {
        const bool pending = graphics_->pending_confirmation();
        result = graphics_->update(graphics_time_);
        if (pending && !graphics_->pending_confirmation()) {
            graphics_leave_after_save_ = false;
            selected_ = 0;
            custom_.message = "DISPLAY SETTINGS REVERTED";
        }
    }
    if (result != Result::ok) {
        custom_.message = "GRAPHICS UPDATE FAILED";
        result = Result::ok;
    }
    refresh_graphics_rows();
    bool hit = false;
    if (input.pointer_moved || input.pointer_accept) {
        const int step = canvas_.font_height(FrontendFont::body) + 5;
        const int top = 240 - static_cast<int>(custom_.rows.size()) * step / 2;
        for (std::size_t i = 0; i < custom_.rows.size(); ++i)
            if (input.pointer_x >= 20 && input.pointer_x < 630 &&
                input.pointer_y >= top + i * step && input.pointer_y < top + (i + 1) * step) {
                selected_ = static_cast<int>(i);
                hit = true;
                break;
            }
    }
    const int dx = edge(input.right, previous_.right) - edge(input.left, previous_.left);
    const int dy = edge(input.down, previous_.down) - edge(input.up, previous_.up);
    selected_ = std::clamp(selected_ + dy, 0, static_cast<int>(custom_.rows.size()) - 1);
    const auto row = custom_.rows[selected_];
    const bool back = edge(input.back, previous_.back);
    const bool accept = edge(input.accept, previous_.accept) ||
                        (hit && edge(input.pointer_accept, previous_.pointer_accept));
    if (back || (accept && row.id == "back")) {
        if (graphics_ && graphics_->pending_confirmation()) {
            graphics_leave_after_save_ = false;
            result = graphics_->revert();
            selected_ = 0;
        } else if (graphics_discard_)
            graphics_discard_ = false;
        else if (graphics_ && graphics_->draft() != graphics_->confirmed()) {
            graphics_discard_ = true;
            graphics_save_yes_ = false;
        } else {
            previous_ = input;
            return leave();
        }
    } else if (row.available && (dx || accept)) {
        if (row.id == "apply" && accept) {
            graphics_leave_after_save_ = false;
            result = graphics_->apply(graphics_time_);
            selected_ = 0;
            custom_.message = result == Result::ok
                                  ? "SETTINGS SAVED; RESOURCE QUALITY APPLIES ON NEXT SCENE"
                                  : "SETTINGS COULD NOT BE APPLIED OR SAVED";
        } else if (row.id == "confirm" && accept) {
            result = graphics_->confirm();
            if (result == Result::ok && graphics_leave_after_save_) {
                graphics_leave_after_save_ = false;
                previous_ = input;
                return leave();
            }
            selected_ = 0;
            custom_.message =
                result == Result::ok ? "SETTINGS SAVED" : "SETTINGS COULD NOT BE SAVED";
        } else if (row.id == "revert" && accept) {
            graphics_leave_after_save_ = false;
            result = graphics_->revert();
            selected_ = 0;
        } else if (row.id == "defaults" && accept) {
            graphics_->restore_defaults();
            custom_.message = "DEFAULTS READY; APPLY TO SAVE";
        } else if (row.id == "restart" && accept) {
            result = graphics_host_->restart();
            if (result == Result::ok)
                quit_ = true;
        } else if (row.id == "discard" && accept) {
            graphics_->discard_draft();
            previous_ = input;
            return leave();
        } else if (row.id == "cancel_discard" && accept) {
            graphics_discard_ = false;
            selected_ = 0;
        } else if (row.id != "apply" && row.id != "defaults" && row.id != "restart" &&
                   row.id != "confirm" && row.id != "revert") {
            edit_graphics(row.id, dx ? dx : input.pointer_accept && input.pointer_x < 350 ? -1 : 1);
            custom_.message.clear();
        }
    }
    if (result != Result::ok) {
        custom_.message = graphics_host_->last_error();
        if (custom_.message.empty())
            custom_.message = "GRAPHICS OPERATION FAILED";
    }
    refresh_graphics_rows();
    time_ += elapsed;
    previous_ = input;
    return draw(error);
}
const FrontendVideoAudio* MenuFrontend::video_audio() const {
    return video_ ? &video_->audio() : nullptr;
}
std::vector<std::string> MenuFrontend::take_sound_events() {
    std::vector<std::string> result;
    result.swap(sounds_);
    return result;
}
std::string MenuFrontend::translate(std::string_view table, std::string_view key) {
    if (!language_)
        return std::string(key);
    FrontendLocalization localization;
    std::string error, output;
    if (localization.load(resources_, table, error) != Result::ok)
        return std::string(key);
    localization.lookup(key, language_, output);
    return output;
}
Result MenuFrontend::start(std::string& error, std::string_view initial_screen) {
    Configuration config;
    auto result = config.load(resources_, "Ini/RoadWars.ini");
    float language{};
    if (result != Result::ok || config.number("ROADWARS", "LANGUAGE", language) != Result::ok ||
        language < 0 || language > 6 || std::floor(language) != language) {
        error = "Frontend LANGUAGE configuration unavailable";
        return Result::invalid_argument;
    }
    language_ = static_cast<int>(language);
    float gain{};
    if (config.number("ROADWARS", "SFX", gain) == Result::ok && gain >= 0 && gain <= 1)
        sfx_gain_ = gain;
    if (config.number("ROADWARS", "MUSIC", gain) == Result::ok && gain >= 0 && gain <= 1)
        music_gain_ = gain;
    result = canvas_.load(error);
    if (result != Result::ok)
        return result;
    history_.clear();
    live_loading_ = false;
    race_request_.mode = FrontendRaceMode::none;
    option_values_.clear();
    screen_.clear();
    previous_ = {};
    preview_ = quit_ = live_results_ = live_pause_ = false;
    garage_prepared_ = false;
    garage_.reset();
    garage_pending_selection_.reset();
    garage_pending_item_.reset();
    garage_message_.clear();
    garage_modal_backdrop_.clear();
    for (auto& scene : prepared_garage_)
        scene.reset();
    return enter(initial_screen, error);
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004a91f0
Result MenuFrontend::enter(std::string_view id, std::string& error) {
    if (screen_ == "modify_vehicle" && garage_prepared_)
        prepared_garage_rows_[active_scene_variant_] = custom_.rows;
    if (id == "main_menu") {
        garage_.reset();
        race_request_.mode = FrontendRaceMode::none;
        race_request_.player_loadout.reset();
        live_loading_ = false;
    }
    const bool modal = id == "dialog" || id == "pause";
    if (modal && !video_)
        backdrop_.assign(canvas_.commands().begin(), canvas_.commands().end());
    else
        backdrop_.clear();
    if (screen_ == "graphics" || screen_ == "sound" || screen_ == "game" || screen_ == "keyboard" ||
        screen_ == "joystick")
        option_values_[screen_] = custom_.rows;
    const auto* info = find_screen(id);
    if (!info) {
        error = "Unknown frontend screen: " + std::string(id);
        return Result::not_found;
    }
    screen_ = id;
    time_ = pulse_time_ = 0;
    exit_time_ = -1;
    pending_.clear();
    action_.clear();
    assertion_message_.clear();
    selections_.clear();
    selected_ = scroll_ = 0;
    fade_seconds_ = .3;
    timeout_seconds_ = 0;
    fade_in_ = back_enabled_ = times_out_ = false;
    loading_black_pending_ = false;
    loading_fade_origin_ = 0;
    title_.clear();
    video_.reset();
    video_index_ = -1;
    canvas_.release(video_frame_);
    if (id == "publisher_1" || id == "publisher_2") {
        std::vector<std::byte> bytes;
        const auto path = id == "publisher_1" ? "Screens/Publisher Intro/Avi1.avi"
                                              : "Screens/Publisher Intro/Avi2.avi";
        auto result = resources_.read(path, bytes);
        if (result != Result::ok) {
            error = "Publisher video unavailable: " + std::string(path);
            return result;
        }
        video_ = std::make_unique<FrontendVideo>();
        if ((result = video_->open(bytes, error)) != Result::ok)
            return result;
        times_out_ = true;
        timeout_seconds_ = video_->duration();
    } else if (!info->config.empty()) {
        Configuration config;
        auto result = config.load(resources_, info->config);
        if (result != Result::ok) {
            error = "Frontend config unavailable: " + std::string(info->config);
            return result;
        }
        const auto number = [&](std::string_view key, float& value) {
            return config.number("INFO", key, value) == Result::ok;
        };
        float count{}, fade{}, back{}, timeout{}, fade_seconds{}, timeout_seconds{};
        if (!number("NUMSELECTIONS", count) || !number("DOFADEIN", fade) ||
            !number("GOBACKENABLED", back) || !number("TIMESOUT", timeout) ||
            !number("FADESECS", fade_seconds) || !number("TIMEOUTSECS", timeout_seconds) ||
            count < 0 || count > 64 || std::floor(count) != count || fade_seconds < 0 ||
            fade_seconds > 60 || timeout_seconds < 0 || timeout_seconds > 3600) {
            error = "Invalid frontend screen configuration: " + std::string(info->config);
            return Result::invalid_argument;
        }
        fade_in_ = fade != 0;
        back_enabled_ = back != 0;
        times_out_ = timeout != 0;
        fade_seconds_ = fade_seconds;
        timeout_seconds_ = timeout_seconds;
        title_ = config.value("INFO", "TITLE");
        if (title_ == "NONE")
            title_.clear();
        else
            title_ = translate("ScreenTitles", title_);
        for (int i = 0; i < static_cast<int>(count); ++i) {
            const auto section = "SELECTION" + std::to_string(i);
            float available{}, do_fade{};
            const auto value = config.value(section, "VALUE");
            if (value.empty() || config.number(section, "ISAVAILABLE", available) != Result::ok ||
                config.number(section, "DOFADE", do_fade) != Result::ok) {
                error = "Invalid frontend selection: " + section;
                return Result::invalid_argument;
            }
            selections_.push_back(
                {value, translate("ScreenIconLabels", value), available != 0, do_fade != 0, 50});
        }
        for (std::size_t i = 0; i < selections_.size(); ++i)
            if (selections_[i].available) {
                selected_ = static_cast<int>(i);
                break;
            }
        if (!selections_.empty())
            selections_[selected_].radius = 100;
    } else
        back_enabled_ = id != "info";
    custom_ = {};
    custom_.screen = screen_;
    custom_.variant = variant_;
    custom_.track = track_;
    custom_.fields["CHARACTER"] = character_;
    if (id == "dialog") {
        const std::map<std::string, std::string> questions{
            {"delete_season", "ARE YOU SURE YOU WANT TO DELETE THIS SEASON?"},
            {"replace_season", "A SEASON WITH THE SAME NAME ALREADY EXISTS. REPLACE?"},
            {"end_season", "END SEASON AND RETURN TO MAIN MENU?"},
            {"change_character", "ARE YOU SURE YOU WANT TO CHANGE YOUR CHARACTER?"},
            {"downgrade_item", "ARE YOU SURE YOU WANT TO DOWNGRADE THE QUALITY OF THIS ITEM?"}};
        const auto found = questions.find(variant_);
        custom_.message = translate("Misc", found == questions.end()
                                                ? "ARE YOU SURE YOU WANT TO EXIT THE SESSION?"
                                                : found->second);
    }
    if (id == "host_or_join" && variant_.empty())
        custom_.variant = connection_;
    if (id == "race_results" || id == "standings" || id == "season_results" ||
        id == "start_race_summary" || id == "victory" || id == "defeat") {
        constexpr std::array<const char*, 6> characters{"ANIMAL",   "BUD THE CRUD", "NIKI ROADKILL",
                                                        "RADCLIFF", "ELECTRA",      "SARGE"};
        custom_.racers.push_back({character_, character_, 60, 6000, true, {}});
        for (const auto name : characters) {
            if (name == character_ || custom_.racers.size() == 6)
                continue;
            const int i = static_cast<int>(custom_.racers.size());
            custom_.racers.push_back(
                {name, name, (6 - i) * 10, 1000 * (6 - i), variant_ != "dnf" || i < 5, {}});
        }
        if (id == "victory" || id == "defeat") {
            custom_.racers.resize(3);
            if (id == "defeat")
                std::rotate(custom_.racers.begin(), custom_.racers.begin() + 1,
                            custom_.racers.end());
            constexpr std::array<int, 3> points{10, 6, 4}, money{10000, 5000, 2500};
            for (int i = 0; i < 3; ++i) {
                custom_.racers[i].points = points[i];
                custom_.racers[i].cash = money[i];
            }
        }
    }
    custom_.loading_progress = variant_ == "empty" ? 0 : variant_ == "complete" ? 1 : .5f;
    if (info->custom) {
        auto result = frontend_custom_rows(resources_, screen_, custom_.rows, error, variant_);
        if (result != Result::ok && result != Result::not_found)
            return result;
        if (const auto found = option_values_.find(screen_); found != option_values_.end())
            custom_.rows = found->second;
        if (id == "sound")
            configure_sound_rows();
        if (id == "graphics") {
            graphics_leave_after_save_ = false;
            graphics_discard_ = false;
            graphics_quality_ = false;
            graphics_time_ = 0;
            graphics_.reset();
            if (graphics_host_)
                graphics_ = std::make_unique<GraphicsSettingsTransaction>(*graphics_host_);
            refresh_graphics_rows();
        }
        if (variant_ == "empty" && (id == "load_season" || id == "select_session"))
            custom_.rows.clear();
    }
    if (!preview_ && race_request_.mode != FrontendRaceMode::none &&
        (id == "start_race_summary" || id == "loading" || id == "modify_vehicle")) {
        custom_.preview_data = false;
        custom_.racers.clear();
        Configuration characters;
        if (characters.load(resources_, "Screens/SelectCharacter/Data.ini") != Result::ok)
            return Result::unavailable;
        float count{};
        if (characters.number("INFO", "NUMSELECTIONS", count) != Result::ok || count < 6 ||
            count > 256 || std::floor(count) != count)
            return Result::invalid_argument;
        std::vector<std::string> available, lineup;
        for (int i = 0; i < static_cast<int>(count); ++i) {
            const auto section = "SELECTION" + std::to_string(i);
            float enabled{};
            if (characters.number(section, "ISAVAILABLE", enabled) != Result::ok)
                return Result::invalid_argument;
            auto name = characters.value(section, "VALUE");
            if (enabled && ResourceStore::canonical(name) != ResourceStore::canonical(character_))
                available.push_back(std::move(name));
        }
        auto seed = race_request_.seed;
        if (select_race_characters(seed, available, character_, 5, 6, lineup, error) != Result::ok)
            return Result::unavailable;
        for (const auto& name : lineup)
            custom_.racers.push_back({name, name, 0, 25000, true, {}});
        if (id == "modify_vehicle") {
            custom_.racers = {{character_, character_, 0, 25000, true, {}}};
            custom_.fields["DRIVE TRAIN"] = "STOCK";
            custom_.fields["TRANSMISSION"] = "AUTOMATIC";
            custom_.fields["TIRES"] = "STREET / POOR";
            custom_.fields["ARMOR"] = "NONE";
            custom_.fields["FRONT"] = "NONE";
            custom_.fields["SIDE"] = "NONE";
            custom_.fields["REAR"] = "NONE";
        }
        if (id == "start_race_summary") {
            Configuration world;
            float meters{};
            if (world.load(resources_, "Tracks/" + track_ + "/World/Data.txt") != Result::ok ||
                world.number("OUTPUT", "TRACKLENGTH(M)", meters) != Result::ok ||
                !std::isfinite(meters) || meters <= 0)
                return Result::unavailable;
            char miles[32];
            std::snprintf(miles, sizeof(miles), "%.2f", meters / 1609.344f);
            custom_.rows = {{"LAPS", {std::to_string(race_request_.laps)}},
                            {"MILES", {miles}},
                            {"TIRES", {world.value("AI", "TIRES")}}};
        }
        if (id == "loading") {
            fade_in_ = race_request_.mode == FrontendRaceMode::attract;
            loading_black_pending_ = fade_in_;
            custom_.message =
                loading_target_ == "garage" ? "STARTING" : "LOADING VEHICLES AND TRACK";
            custom_.loading_progress = 0;
            loading_finish_time_ = 0;
            back_enabled_ = false;
        }
    }
    if (id == "dialog")
        selected_ = 1;
    if (id == "modify_vehicle" || id == "victory" || id == "defeat") {
        if (!scene_)
            scene_ = std::make_unique<FrontendScene>(resources_, renderer_);
        int scene_variant = 0;
        constexpr std::array<std::string_view, 6> variants{"tires", "drive_train", "weapons",
                                                           "armor", "repair",      "paint"};
        for (int i = 0; i < 6; ++i)
            if (variant_ == variants[i])
                scene_variant = i + 1;
        if (!preview_ && id == "modify_vehicle" && garage_prepared_) {
            if (scene_->loaded()) {
                scene_->set_visible(false);
                prepared_garage_[active_scene_variant_] = std::move(scene_);
            }
            scene_ = std::move(prepared_garage_[scene_variant]);
            if (!scene_) {
                error = "Prepared garage scene unavailable";
                return Result::unavailable;
            }
            active_scene_variant_ = scene_variant;
            scene_->set_visible(true);
            custom_.rows = prepared_garage_rows_[scene_variant];
        } else {
            const auto result = scene_->load(id, character_, scene_variant, error);
            if (result != Result::ok)
                return result;
        }
        if (id == "modify_vehicle" && garage_ && !preview_) {
            garage_pending_item_.reset();
            garage_pending_selection_.reset();
            garage_message_.clear();
            if (refresh_garage(true, error) != Result::ok)
                return Result::unavailable;
            if (!variant_.empty()) {
                scene_->cancel_store_transition();
                const auto& item =
                    garage_->items(garage_selection_.category)[garage_selection_.item];
                if (scene_->select_store_product(item.name,
                                                 item.products[garage_selection_.product].name,
                                                 error) != Result::ok)
                    return Result::unavailable;
            }
        }
    } else if (scene_ && !modal) {
        scene_->stop();
        if (id != "loading") {
            garage_prepared_ = false;
            for (auto& scene : prepared_garage_)
                scene.reset();
        }
    }
    return draw(error);
}
std::array<int, 2> MenuFrontend::icon_center(int count, int slot) {
    if (count == 2)
        return {slot == 0 ? 160 : 480, 253};
    if (count == 3)
        return {110 + 210 * slot, 253};
    if (count == 4)
        return {slot % 2 == 0 ? 160 : 480, slot < 2 ? 166 : 340};
    if (count == 5)
        return slot < 3 ? std::array<int, 2>{110 + 210 * slot, 166}
                        : std::array<int, 2>{slot == 3 ? 160 : 480, 340};
    return {110 + (slot % 3) * 210, slot < 3 ? 180 : 348};
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004adecd
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004ae02b
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004ae146
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004ae1ed
void MenuFrontend::move(int dx, int dy) {
    const int count = static_cast<int>(selections_.size());
    if (!count)
        return;
    const int old = selected_, split = (count + 1) / 2;
    if (dx > 0) {
        if (count < 4)
            selected_ = std::min(count - 1, selected_ + 1);
        else if (selected_ != split - 1 && selected_ < count - 1)
            ++selected_;
    } else if (dx < 0) {
        if (count < 4)
            selected_ = std::max(0, selected_ - 1);
        else if (selected_ % split)
            --selected_;
    }
    if (count > 3 && dy > 0 && selected_ < split)
        selected_ = std::min(count - 1, selected_ + split);
    if (count > 3 && dy < 0 && selected_ >= split)
        selected_ -= split;
    if (count > 6) {
        const int column = selected_ % (count / 2);
        scroll_ = std::clamp(scroll_, std::max(0, column - 2), column);
    }
    if (old != selected_) {
        pulse_time_ = 0;
        sounds_.push_back("Sfx/Sound1.wav");
    }
}
Result MenuFrontend::navigate(std::string_view id, std::string& error) {
    (void)error;
    if (!find_screen(id))
        return Result::not_found;
    pending_ = id;
    exit_time_ = video_ ? fade_seconds_ : 0;
    return Result::ok;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004ab60d
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004aac8f
Result MenuFrontend::activate(std::string& error) {
    if (screen_ == "sound")
        return Result::ok;
    if (screen_ == "race_results" && live_results_)
        return navigate("main_menu", error);
    if (screen_ == "publisher_1")
        return navigate("publisher_2", error);
    if (screen_ == "publisher_2")
        return navigate("intro", error);
    if (screen_ == "intro")
        return navigate("title", error);
    if (screen_ == "title")
        return navigate("main_menu", error);
    if (screen_ == "info") {
        quit_ = true;
        return Result::ok;
    }
    if (screen_ == "victory" || screen_ == "defeat")
        return navigate("main_menu", error);
    if (screen_ == "dialog") {
        action_ = selected_ == 0 ? "confirmation_yes" : "confirmation_no";
        return Result::ok;
    }
    if (screen_ == "modify_vehicle" && variant_.empty() && !custom_.rows.empty()) {
        constexpr std::array<std::string_view, 6> variants{"tires", "drive_train", "weapons",
                                                           "armor", "repair",      "paint"};
        if (selected_ < 0 || selected_ >= 6)
            return Result::invalid_argument;
        variant_ = variants[selected_];
        return navigate("modify_vehicle", error);
    }
    if (screen_ == "create_season") {
        action_ = "unavailable_business_action:Create Season";
        return Result::ok;
    }
    if (screen_ == "pause" && !custom_.rows.empty()) {
        const auto& row = custom_.rows[selected_];
        if (!row.available) {
            action_ = "selection_unavailable:" + row.label;
            return Result::ok;
        }
        if (live_pause_) {
            action_ = "race_pause:" + row.label;
            return Result::ok;
        }
        if (row.label == "EXIT TO MAIN MENU")
            return navigate("main_menu", error);
        if (row.label == "EXIT GAME")
            return navigate("info", error);
        action_ = "unavailable_business_action:" + row.label;
        return Result::ok;
    }
    if (live_loading_ && screen_ == "loading")
        return Result::ok;
    if (selections_.empty()) {
        action_ = "unavailable_business_action:" + screen_;
        if (!variant_.empty())
            action_ += "/" + variant_;
        if (!custom_.rows.empty() && selected_ >= 0 &&
            static_cast<std::size_t>(selected_) < custom_.rows.size())
            action_ += "/" + custom_.rows[selected_].label;
        return Result::ok;
    }
    if (selected_ < 0 || static_cast<std::size_t>(selected_) >= selections_.size())
        return Result::invalid_argument;
    const auto& selection = selections_[selected_];
    if (!selection.available) {
        action_ = "selection_unavailable:" + selection.value;
        sounds_.push_back("Sfx/notavail.wav");
        return Result::ok;
    }
    sounds_.push_back("Sfx/SoundSelection.wav");
    const auto value = selection.value;
    std::string target;
    if (screen_ == "main_menu") {
        if (value == "OPTIONS")
            target = "select_option";
        else if (value == "SINGLE RACE" || value == "QUICK RACE") {
            original_random(race_request_.seed);
            race_request_.mode =
                value == "SINGLE RACE" ? FrontendRaceMode::single : FrontendRaceMode::quick;
            race_request_.player_loadout.reset();
            target = "select_character";
        } else if (value == "SEASON RACE" || value == "MULTIPLAYER") {
            action_ = "unavailable_business_action:" + value;
            return Result::ok;
        } else if (value == "EXIT")
            target = "info";
    } else if (screen_ == "select_option") {
        if (value == "GRAPHICS")
            target = "graphics";
        else if (value == "SOUND")
            target = "sound";
        else if (value == "GAME")
            target = "game";
        else if (value == "CONTROLLERS")
            target = "select_controller";
    } else if (screen_ == "select_controller")
        target = value == "KEYBOARD" ? "keyboard" : "joystick";
    else if (screen_ == "select_character") {
        character_ = value;
        target = "select_track";
    } else if (screen_ == "select_track") {
        track_ = value;
        if (race_request_.mode != FrontendRaceMode::none) {
            if (prepare_race(error) != Result::ok)
                return Result::unavailable;
            target =
                race_request_.mode == FrontendRaceMode::quick ? "loading" : "start_race_summary";
            live_loading_ = race_request_.mode == FrontendRaceMode::quick;
            loading_target_ = "race";
        } else
            target = "start_race_summary";
    } else if (screen_ == "season_type") {
        action_ = "unavailable_business_action:" + value;
        return Result::ok;
    } else if (screen_ == "select_league")
        target = "select_character";
    else if (screen_ == "host_or_join") {
        action_ = "unavailable_business_action:Multiplayer/" + value;
        return Result::ok;
    } else if (screen_ == "select_connection") {
        connection_ = value;
        target = "host_or_join";
    } else if (screen_ == "start_race_summary" && value == "GARAGE" && !preview_) {
        target = "loading";
        loading_target_ = "garage";
        live_loading_ = true;
        garage_loading_step_ = 0;
        garage_prepared_ = false;
    } else if (screen_ == "start_race_summary" && value == "GARAGE")
        target = "modify_vehicle";
    else if (screen_ == "start_race_summary" && value == "START RACE" &&
             race_request_.mode == FrontendRaceMode::single) {
        live_loading_ = true;
        loading_target_ = "race";
        target = "loading";
    } else if (value == "CONTINUE")
        target = "main_menu";
    if (target.empty()) {
        action_ = "unavailable_business_action:" + value;
        return Result::ok;
    }
    history_.push_back(screen_);
    return navigate(target, error);
}
void MenuFrontend::show_unimplemented_action() {
    constexpr std::string_view prefix = "unavailable_business_action:";
    if (action_.starts_with(prefix))
        assertion_message_ = action_.substr(prefix.size());
}
Result MenuFrontend::advance_garage_loading(double elapsed, std::string& error) {
    time_ += elapsed;
    const auto load_scene = [&](int variant) {
        auto scene = std::make_unique<FrontendScene>(resources_, renderer_);
        const auto result = scene->load("modify_vehicle", character_, variant, error);
        if (result != Result::ok)
            return result;
        scene->set_visible(false);
        prepared_garage_[variant] = std::move(scene);
        return Result::ok;
    };
    const auto load_rows = [&](int variant, std::string_view name) {
        return frontend_custom_rows(resources_, "modify_vehicle", prepared_garage_rows_[variant],
                                    error, name);
    };
    const int step = garage_loading_step_;
    if (step == 0) {
        ++garage_loading_step_;
        return set_loading_progress(0, "STARTING", error);
    }
    if (step == 1) {
        if (load_scene(0) != Result::ok || load_rows(0, "") != Result::ok)
            return Result::unavailable;
    } else if (step == 2) {
        if (load_scene(1) != Result::ok)
            return Result::unavailable;
    } else if (step == 3) {
        if (load_rows(1, "tires") != Result::ok)
            return Result::unavailable;
    } else if (step >= 4 && step <= 8) {
        constexpr std::array<int, 5> variants{4, 3, 2, 5, 6};
        constexpr std::array<std::string_view, 5> names{"armor", "weapons", "drive_train", "repair",
                                                        "paint"};
        if (load_scene(variants[step - 4]) != Result::ok ||
            load_rows(variants[step - 4], names[step - 4]) != Result::ok)
            return Result::unavailable;
    } else if (step == 9) {
        ++garage_loading_step_;
        garage_prepared_ = true;
        return set_loading_progress(1, "FINISHED", error);
    } else {
        if (!loading_fade_complete())
            return draw(error);
        live_loading_ = false;
        variant_.clear();
        return enter("modify_vehicle", error);
    }
    constexpr std::array<std::string_view, 8> messages{
        "LOADING GARAGE",       "LOADING STORE",        "LOADING TIRE STORE",
        "LOADING ARMOR STORE",  "LOADING WEAPON STORE", "LOADING DRIVE TRAIN STORE",
        "LOADING REPAIR STORE", "LOADING PAINT STORE"};
    ++garage_loading_step_;
    return set_loading_progress(static_cast<float>(step) / 10, messages[step - 1], error);
}
Result MenuFrontend::set_loading_progress(float fraction, std::string_view message,
                                          std::string& error) {
    if (screen_ != "loading" || !std::isfinite(fraction) || fraction < 0 || fraction > 1 ||
        fraction < custom_.loading_progress)
        return Result::invalid_argument;
    if (custom_.loading_progress < 1 && fraction == 1)
        loading_finish_time_ = 0;
    custom_.loading_progress = fraction;
    custom_.message = translate("Misc", message);
    return draw(error);
}
std::string MenuFrontend::music_path() const {
    if (screen_ == "loading" && race_request_.mode == FrontendRaceMode::attract)
        return "Sfx/SoundBackgroundMusic.wav";
    if (screen_ == "loading" || screen_ == "modify_vehicle")
        return "Characters/" + character_ + "/ThemeMusic.wav";
    return menu_music_requested() ? "Sfx/SoundBackgroundMusic.wav" : "";
}
float MenuFrontend::music_gain_scale() const {
    if (race_request_.mode == FrontendRaceMode::attract)
        return 1;
    if (screen_ != "loading" || loading_target_ != "race")
        return 1;
    return static_cast<float>(custom_.loading_progress >= 1
                                  ? std::max(0.0, 1 - loading_finish_time_ / .3)
                                  : std::min(1.0, time_ / .3));
}
bool MenuFrontend::menu_music_requested() const {
    return screen_ != "publisher_1" && screen_ != "publisher_2" && screen_ != "intro" &&
           screen_ != "loading";
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004aa4d3
Result MenuFrontend::prepare_race(std::string& error) {
    race_request_.character = character_;
    race_request_.track = track_;
    Configuration world;
    float laps{};
    if (world.load(resources_, "Tracks/" + track_ + "/World/Data.txt") != Result::ok ||
        world.number("DATA", "NUMLAPS", laps) != Result::ok || !std::isfinite(laps) || laps < 1 ||
        laps > 10 || std::floor(laps) != laps) {
        error = "Original track lap count unavailable";
        return Result::unavailable;
    }
    race_request_.laps = static_cast<int>(laps);
    if (race_request_.mode == FrontendRaceMode::single) {
        if (!garage_ || ResourceStore::canonical(garage_->state().character) !=
                            ResourceStore::canonical(character_)) {
            garage_ = std::make_unique<GarageSession>(resources_);
            if (garage_->start_single(character_, track_, error) != Result::ok)
                return Result::unavailable;
        } else if (garage_->restore(garage_->state(), track_, error) != Result::ok)
            return Result::unavailable;
        race_request_.player_loadout = garage_->race_loadout();
    } else
        race_request_.player_loadout.reset();
    return Result::ok;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00406fc0
Result MenuFrontend::prepare_attract(std::string& error) {
    const auto choose = [&](std::string_view path, std::string& output) -> Result {
        Configuration config;
        float count{};
        if (config.load(resources_, path) != Result::ok ||
            config.number("INFO", "NUMSELECTIONS", count) != Result::ok || !std::isfinite(count) ||
            count < 1 || count > 64 || std::floor(count) != count)
            return Result::unavailable;
        std::vector<std::string> available;
        for (int i = 0; i < static_cast<int>(count); ++i) {
            const auto section = "SELECTION" + std::to_string(i);
            float enabled{};
            const auto value = config.value(section, "VALUE");
            if (config.number(section, "ISAVAILABLE", enabled) != Result::ok ||
                !std::isfinite(enabled) || value.empty() ||
                value.find_first_of("/\\:\r\n") != std::string::npos)
                return Result::unavailable;
            if (enabled != 0)
                available.push_back(value);
        }
        if (available.empty())
            return Result::unavailable;
        output = available[original_random(race_request_.seed) % available.size()];
        return Result::ok;
    };
    if (choose("Screens/SelectCharacter/Data.ini", character_) != Result::ok ||
        choose("Screens/SelectTrack/Data.ini", track_) != Result::ok) {
        error = "Original attract-mode selection tables unavailable";
        return Result::unavailable;
    }
    race_request_.mode = FrontendRaceMode::attract;
    if (prepare_race(error) != Result::ok)
        return Result::unavailable;
    live_loading_ = true;
    loading_target_ = "race";
    return navigate("loading", error);
}
Result MenuFrontend::preview(std::string_view id, double seconds, std::string_view variant,
                             std::string& error) {
    if (!std::isfinite(seconds) || seconds < 0 || seconds > 86400)
        return Result::invalid_argument;
    preview_ = true;
    live_loading_ = false;
    variant_ = variant;
    history_.clear();
    previous_ = {};
    auto result = enter(id, error);
    if (result != Result::ok)
        return result;
    time_ = pulse_time_ = seconds;
    if (scene_ && scene_->loaded() && screen_ != "dialog" && screen_ != "pause") {
        // Preview seeks must settle the complete requested presentation timeline.
        double remaining = seconds;
        while (remaining > 0) {
            const double step = std::min(remaining, 4.0);
            auto result = scene_->advance(step);
            if (result != Result::ok)
                return result;
            remaining -= step;
        }
    }
    return draw(error);
}
Result MenuFrontend::show_pause(std::span<const HudQuad> backdrop, std::string& error,
                                bool race_finished, bool replay_available) {
    const std::vector<HudQuad> saved(backdrop.begin(), backdrop.end());
    auto result = start(error, "pause");
    if (result != Result::ok)
        return result;
    live_pause_ = true;
    pause_race_finished_ = race_finished;
    variant_ = std::string("Single Race") + (race_finished ? " Over" : "") +
               (replay_available ? "" : " No Replay");
    custom_.variant = variant_;
    result = frontend_custom_rows(resources_, "pause", custom_.rows, error, variant_);
    if (result != Result::ok)
        return result;
    fade_in_ = false;
    backdrop_ = saved;
    return draw(error);
}
Result MenuFrontend::advance(double elapsed, const FrontendInput& input, std::string& error) {
    if (!std::isfinite(elapsed) || elapsed < 0 || elapsed > 86400)
        return Result::invalid_argument;
    if (screen_ == "graphics")
        return advance_graphics(elapsed, input, error);
    if (screen_ == "modify_vehicle" && garage_ && !preview_ && exit_time_ < 0)
        return advance_garage(elapsed, input, error);
    if (screen_ == "loading" && custom_.loading_progress >= 1)
        loading_finish_time_ += elapsed;
    if (screen_ == "loading" && live_loading_ && loading_target_ == "garage") {
        previous_ = input;
        return advance_garage_loading(elapsed, error);
    }
    const auto edge = [](bool now, bool before) { return now && !before; };
    if (!assertion_message_.empty()) {
        const bool dismiss_pointer = edge(input.pointer_accept, previous_.pointer_accept) &&
                                     input.pointer_x >= 256 && input.pointer_x < 384 &&
                                     input.pointer_y >= 300 && input.pointer_y < 330;
        if (edge(input.accept, previous_.accept) || edge(input.back, previous_.back) ||
            dismiss_pointer) {
            assertion_message_.clear();
            action_.clear();
        }
        previous_ = input;
        return draw(error);
    }
    if (exit_time_ < 0) {
        bool hit = false;
        int pointer_direction = 0;
        if (screen_ == "sound" && custom_.rows.size() == 6 &&
            (input.pointer_moved || input.pointer_accept)) {
            const int step = canvas_.font_height(FrontendFont::body) + 5;
            const int top = 240 - static_cast<int>(custom_.rows.size()) * step / 2;
            for (int i = 0; i < 6; ++i)
                if (input.pointer_y >= top + i * step && input.pointer_y < top + (i + 1) * step &&
                    input.pointer_x >= 20 && input.pointer_x < 640) {
                    selected_ = i;
                    if (edge(input.pointer_accept, previous_.pointer_accept)) {
                        if (input.pointer_x >= 300 && input.pointer_x < 340)
                            pointer_direction = -1;
                        else if (input.pointer_x >= 590)
                            pointer_direction = 1;
                    }
                    break;
                }
        }
        if ((input.pointer_moved || input.pointer_accept) && !selections_.empty() &&
            (!find_screen(screen_)->custom || screen_ == "start_race_summary")) {
            const int count = static_cast<int>(selections_.size());
            for (int slot = 0; slot < std::min(6, count); ++slot) {
                const int index =
                    count <= 6 ? slot : scroll_ + (slot < 3 ? slot : slot - 3 + count / 2);
                const auto center = screen_ == "start_race_summary"
                                        ? std::array<int, 2>{slot == 0 ? 98 : 540, 392}
                                        : icon_center(count, slot);
                const float radius = selections_[index].radius;
                if (input.pointer_x >= center[0] - radius && input.pointer_x < center[0] + radius &&
                    input.pointer_y >= center[1] - radius && input.pointer_y < center[1] + radius) {
                    if (selected_ != index)
                        pulse_time_ = 0;
                    selected_ = index;
                    hit = true;
                    break;
                }
            }
        }
        if (screen_ == "dialog" && (input.pointer_moved || input.pointer_accept)) {
            FrontendDialogLayout layout;
            const auto result = frontend_dialog_layout(canvas_, false, layout);
            if (result != Result::ok)
                return result;
            const int button = layout.hit(input.pointer_x, input.pointer_y);
            if (button >= 0) {
                selected_ = button;
                hit = true;
            }
        }
        if (live_pause_ && (input.pointer_moved || input.pointer_accept)) {
            Configuration setup;
            if (setup.load(resources_, "Game Menu/Screen Setup.ini") != Result::ok)
                return Result::unavailable;
            float spacing{};
            if (setup.number("INFO", "SELECTIONSPACING", spacing) != Result::ok)
                return Result::invalid_argument;
            const int height = canvas_.font_height(FrontendFont::menu);
            const float top =
                240 - (height * custom_.rows.size() + spacing * (custom_.rows.size() - 1)) * .5f;
            for (std::size_t row = 0; row < custom_.rows.size(); ++row)
                if (input.pointer_x >= 64 && input.pointer_x < 576 &&
                    input.pointer_y >= top + row * (height + spacing) &&
                    input.pointer_y < top + row * (height + spacing) + height &&
                    custom_.rows[row].available) {
                    selected_ = static_cast<int>(row);
                    hit = true;
                    break;
                }
        }
        const int dx = pointer_direction
                           ? pointer_direction
                           : edge(input.right, previous_.right) - edge(input.left, previous_.left);
        const int dy = edge(input.down, previous_.down) - edge(input.up, previous_.up);
        if (!custom_.rows.empty() && selections_.empty()) {
            selected_ = std::clamp(selected_ + dy, 0, static_cast<int>(custom_.rows.size()) - 1);
            auto& row = custom_.rows[selected_];
            if (dx && row.available && !row.values.empty()) {
                if (screen_ == "graphics" || screen_ == "game" || screen_ == "keyboard" ||
                    screen_ == "joystick") {
                    action_ = "unavailable_business_action:" + screen_ + "/" + row.label;
                    show_unimplemented_action();
                    previous_ = input;
                    return draw(error);
                }
                row.selected = (row.selected + row.values.size() + dx) % row.values.size();
                if (screen_ == "sound") {
                    sounds_.push_back("Sfx/Sound1.wav");
                    if (selected_ == 0)
                        audio_restart_requested_ = true;
                    custom_.message = custom_.rows[0].selected == 0
                                          ? "CD AUDIO UNAVAILABLE - USING GAME MUSIC"
                                          : "";
                }
                if (screen_ == "modify_vehicle" && !variant_.empty()) {
                    const auto result = update_store_selection(error);
                    if (result != Result::ok)
                        return result;
                }
            }
        } else if (screen_ == "dialog") {
            selected_ = std::clamp(selected_ + dx, 0, 1);
        } else
            move(dx, dy);
        if (live_pause_ && edge(input.back, previous_.back)) {
            action_ = pause_race_finished_ ? "race_pause:CANCEL" : "race_pause:CONTINUE";
            previous_ = input;
            return draw(error);
        }
        if (screen_ != "dialog" && edge(input.back, previous_.back) && back_enabled_) {
            if (screen_ == "sound" && commit_sound_options(error) != Result::ok) {
                previous_ = input;
                return draw(error);
            }
            const bool store = screen_ == "modify_vehicle" && !variant_.empty();
            auto target = store              ? std::string("modify_vehicle")
                          : history_.empty() ? std::string("main_menu")
                                             : history_.back();
            if (store)
                variant_.clear();
            else if (!history_.empty())
                history_.pop_back();
            navigate(target, error);
        } else if (edge(input.accept, previous_.accept) ||
                   (hit && edge(input.pointer_accept, previous_.pointer_accept)) ||
                   (selections_.empty() &&
                    (!find_screen(screen_)->custom || screen_ == "victory" ||
                     screen_ == "defeat") &&
                    edge(input.any_key, previous_.any_key))) {
            auto result = activate(error);
            if (result != Result::ok)
                return result;
            show_unimplemented_action();
            if (!assertion_message_.empty()) {
                previous_ = input;
                return draw(error);
            }
        }
    }
    previous_ = input;
    // Preserve excess elapsed time across automatic startup-page boundaries.
    double remaining = elapsed;
    for (int transitions = 0; transitions < 64; ++transitions) {
        double step = remaining;
        const bool timed = times_out_ && !preview_;
        if (exit_time_ >= 0)
            step = std::min(step, std::max(0.0, fade_seconds_ - exit_time_));
        else if (timed)
            step = std::min(step, std::max(0.0, timeout_seconds_ - time_));
        time_ += step;
        pulse_time_ += step;
        if (scene_ && scene_->loaded() && screen_ != "dialog" && screen_ != "pause") {
            const auto result = scene_->advance(step);
            if (result != Result::ok) {
                error = "Frontend scene update failed";
                return result;
            }
        }
        for (std::size_t i = 0; i < selections_.size(); ++i) {
            auto& radius = selections_[i].radius;
            radius = i == static_cast<std::size_t>(selected_)
                         ? std::min(100.f, radius + static_cast<float>(step * 400))
                         : std::max(50.f, radius - static_cast<float>(step * 400));
        }
        remaining -= step;
        if (exit_time_ >= 0) {
            exit_time_ += step;
            if (exit_time_ + 1e-10 < fade_seconds_)
                break;
            const auto target = pending_;
            auto result = enter(target, error);
            if (result != Result::ok)
                return result;
        } else if (timed && time_ + 1e-10 >= timeout_seconds_) {
            auto result = screen_ == "title" ? prepare_attract(error) : activate(error);
            if (result != Result::ok)
                return result;
            if (video_)
                exit_time_ = fade_seconds_; // AVI has no bitmap fade.
        } else
            break;
        if (remaining <= 0 && exit_time_ < 0)
            break;
    }
    return draw(error);
}
Result MenuFrontend::draw_custom(std::string& error) {
    if (screen_ == "dialog" || screen_ == "pause") {
        if (backdrop_.empty() && !live_pause_)
            canvas_.sprite("Screens/Background.bmp", {0, 0, 640, 480});
        else
            canvas_.append(backdrop_);
    }
    custom_.elapsed = time_;
    custom_.selected = static_cast<std::size_t>(selected_);
    const bool live_race_menu =
        !preview_ && race_request_.mode != FrontendRaceMode::none &&
        (screen_ == "start_race_summary" || screen_ == "loading" || screen_ == "modify_vehicle");
    custom_.preview_data = !live_race_menu && !live_pause_ &&
                           !(screen_ == "race_results" && live_results_) &&
                           (preview_ || (screen_ != "sound" && screen_ != "graphics"));
    const auto result = FrontendCustom::draw(canvas_, custom_, error);
    if (result != Result::ok || screen_ != "graphics" || !graphics_discard_)
        return result;
    FrontendCustomState dialog;
    dialog.screen = "dialog";
    dialog.preview_data = false;
    dialog.message = translate("Misc", "SAVE CHANGES?");
    dialog.selected = graphics_save_yes_ ? 0 : 1;
    return FrontendCustom::draw(canvas_, dialog, error);
}
Result MenuFrontend::show_race_results(std::span<const FrontendPreviewRacer> racers,
                                       std::string_view track, std::string& error) {
    if (racers.empty() || racers.size() > 6)
        return Result::invalid_argument;
    auto result = start(error, "race_results");
    if (result != Result::ok)
        return result;
    live_results_ = true;
    custom_.racers.assign(racers.begin(), racers.end());
    custom_.track = track;
    custom_.fields["LIVE_RACE"] = "1";
    custom_.preview_data = false;
    return draw(error);
}
Result MenuFrontend::update_store_selection(std::string& error) {
    if (!scene_ || !scene_->loaded() || custom_.rows.empty() || custom_.rows[0].values.empty())
        return Result::unavailable;
    const auto item = custom_.rows[0].values[custom_.rows[0].selected];
    const std::map<std::string, std::string> directories{
        {"tires", "Tires"}, {"drive_train", "Drive Train"}, {"weapons", "Weapons"},
        {"armor", "Armor"}, {"repair", "Repair"},           {"paint", "Paint Job"}};
    const auto found = directories.find(variant_);
    if (found == directories.end())
        return Result::invalid_argument;
    Configuration ordering;
    const auto result =
        ordering.load(resources_, "Cars/Shared/" + found->second + "/" + item + "/Data.ini");
    if (result != Result::ok) {
        error = "Store item configuration unavailable: " + item;
        return result;
    }
    float count{};
    const bool has_quality = ordering.number("ORDERING", "NUMITEMS", count) == Result::ok;
    if (has_quality && (count < 1 || count > 64 || std::floor(count) != count))
        return Result::invalid_argument;
    if (has_quality) {
        std::vector<std::string> values;
        for (int i = 0; i < static_cast<int>(count); ++i)
            values.push_back(ordering.value("ORDERING", "ITEM" + std::to_string(i)));
        if (custom_.rows.size() < 2 || custom_.rows[1].label != "QUALITY")
            custom_.rows.insert(custom_.rows.begin() + 1, {"QUALITY", values, 0, true});
        else if (custom_.rows[1].values != values) {
            custom_.rows[1].values = std::move(values);
            custom_.rows[1].selected = 0;
        }
    } else if (custom_.rows.size() > 1 && custom_.rows[1].label == "QUALITY")
        custom_.rows.erase(custom_.rows.begin() + 1);
    selected_ = std::min(selected_, static_cast<int>(custom_.rows.size()) - 1);
    const std::string quality = has_quality ? custom_.rows[1].values[custom_.rows[1].selected] : "";
    return scene_->select_store_product(item, quality, error);
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004ae2ad
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00460fdf
Result MenuFrontend::draw(std::string& error) {
    canvas_.begin();
    const auto* info = find_screen(screen_);
    if (!info)
        return Result::unavailable;
    if (video_) {
        const int index =
            std::min(static_cast<int>(video_->frame_count()) - 1,
                     static_cast<int>(time_ * video_->frame_count() / video_->duration()));
        if (index != video_index_) {
            Bitmap frame;
            auto result = video_->frame_at(time_, frame);
            if (result != Result::ok) {
                error = "Publisher video frame decode failed";
                return result;
            }
            const auto stride = static_cast<std::size_t>(frame.width) * 4;
            for (std::size_t y = 0; y < frame.height / 2; ++y)
                for (std::size_t x = 0; x < stride; ++x)
                    std::swap(frame.rgba[y * stride + x],
                              frame.rgba[(frame.height - 1 - y) * stride + x]);
            canvas_.release(video_frame_);
            if ((result = canvas_.bitmap(frame, video_frame_)) != Result::ok)
                return result;
            video_index_ = index;
        }
        canvas_.rectangle({0, 0, 640, 480}, {0, 0, 0, 1});
        canvas_.sprite(video_frame_, screen_ == "publisher_1" ? HudRect{0, 0, 640, 480}
                                                              : HudRect{160, 120, 320, 240});
    } else if (screen_ == "intro" || screen_ == "title" || screen_ == "info") {
        canvas_.sprite(screen_ == "intro"  ? "Screens/Intro/Logo.bmp"
                       : screen_ == "info" ? "Screens/Info/Info.bmp"
                                           : "Screens/Title/Title.bmp",
                       {0, 0, 640, 480});
        if (screen_ == "title") {
            // RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004b156f
            // Version suffix at 0061cc60 in the hash-matched executable.
            canvas_.text(FrontendFont::small, 10, 475 - canvas_.font_height(FrontendFont::small),
                         translate("Misc", "Version") + " 1.1");
        }
    } else if (info->custom) {
        // Custom data contract is supplied by frontend_custom.
        auto result = draw_custom(error);
        if (result != Result::ok)
            return result;
    } else {
        canvas_.sprite("Screens/Background.bmp", {0, 0, 640, 480});
        if (!title_.empty())
            canvas_.text(FrontendFont::title, 320, 10, title_, true);
        const int count = static_cast<int>(selections_.size());
        for (int slot = 0; slot < std::min(6, count); ++slot) {
            const int index =
                count <= 6 ? slot : scroll_ + (slot < 3 ? slot : slot - 3 + count / 2);
            const auto& selection = selections_[index];
            const auto center = icon_center(count, slot);
            const float scale = selection.radius / 100;
            const HudRect r{center[0] - static_cast<int>(selection.radius),
                            center[1] - static_cast<int>(selection.radius),
                            static_cast<int>(256 * scale), static_cast<int>(256 * scale)};
            const auto path =
                std::string(info->icons) + "/" + selection.value + "/16SelectionIcon.bmp";
            canvas_.sprite(path, r);
            canvas_.text(FrontendFont::arial, r.x + 98 * scale,
                         r.y + (26 - canvas_.font_height(FrontendFont::arial) / 2) * scale,
                         selection.label, true, scale);
            if (!selection.available)
                canvas_.sprite("Screens/16NAOverlay.bmp", r, .7f);
            const auto phase = std::fmod(pulse_time_, 1.4);
            const float alpha = index == selected_ && selection.radius == 100
                                    ? static_cast<float>(phase <= .7 ? phase : 1.4 - phase)
                                    : .7f;
            canvas_.sprite("Screens/16IconTexture.bmp", r, alpha);
        }
        if (scroll_)
            canvas_.sprite("Screens/16ScrollArrowLeft.bmp", {0, 432, 64, 128});
        if (count > 6 && scroll_ + 3 < count / 2)
            canvas_.sprite("Screens/16ScrollArrowRight.bmp", {536, 432, 64, 128});
        if (back_enabled_)
            canvas_.text(FrontendFont::small, 320, 455, translate("Misc", "HIT ESC TO GO BACK"),
                         true, 1, 0x64ffffff, 0x64ffffff);
    }
    if (!video_) {
        float fade = exit_time_ >= 0
                         ? static_cast<float>(fade_seconds_ > 0 ? exit_time_ / fade_seconds_ : 1)
                     : fade_in_ && fade_seconds_ > 0 ? static_cast<float>(1 - time_ / fade_seconds_)
                                                     : 0;
        if (screen_ == "loading" && race_request_.mode == FrontendRaceMode::attract &&
            custom_.loading_progress >= 1)
            fade = std::max(fade, static_cast<float>(loading_finish_time_ / .3));
        if (screen_ == "loading" && race_request_.mode == FrontendRaceMode::attract) {
            const float intro = loading_black_pending_
                                    ? 1.f
                                    : static_cast<float>(1 - (time_ - loading_fade_origin_) / .3);
            fade = std::max(fade, intro);
        }
        if (fade > 0) {
            const float opacity = std::clamp(fade, 0.f, 1.f);
            // The original bitmap is dark gray; the requested title handoff is black.
            if (screen_ == "title" || race_request_.mode == FrontendRaceMode::attract)
                canvas_.rectangle({0, 0, 640, 480}, {0, 0, 0, opacity});
            else
                canvas_.sprite("Screens/16Fade.bmp", {0, 0, 640, 480}, opacity);
        }
    }
    if (preview_)
        canvas_.text(FrontendFont::small, 4, 4, "PREVIEW DATA", false, 1, 0xffffff00, 0xffffff00);
    if (screen_ == "modify_vehicle" && garage_ && !preview_ &&
        draw_garage_modal(error) != Result::ok)
        return Result::unavailable;
    if (!assertion_message_.empty()) {
        // Modern recoverable assertion UI; the rejected business action is not executed.
        canvas_.rectangle({0, 0, 640, 480}, {0, 0, 0, .65f});
        canvas_.rectangle({62, 138, 516, 204}, {.8f, .16f, .12f, 1});
        canvas_.rectangle({64, 140, 512, 200}, {.045f, .045f, .055f, 1});
        canvas_.text(FrontendFont::body, 320, 158, "ASSERTION FAILED", true, 1, 0xffff7070,
                     0xffff7070);
        canvas_.text(FrontendFont::body, 320, 192, "Not implemented", true);
        constexpr std::size_t columns = 42;
        for (std::size_t at = 0, row = 0; at < assertion_message_.size() && row < 3;
             at += columns, ++row)
            canvas_.text(FrontendFont::small, 320, static_cast<float>(228 + row * 20),
                         assertion_message_.substr(at, columns), true);
        canvas_.rectangle({256, 300, 128, 30}, {.22f, .22f, .26f, 1});
        canvas_.text(FrontendFont::small, 320, 309, "OK", true);
    }
    auto result = canvas_.submit();
    if (result != Result::ok)
        error = canvas_.error();
    return result;
}
} // namespace roadwars
