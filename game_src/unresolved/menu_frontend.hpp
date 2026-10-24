#pragma once
#include "game_src/core/graphics_settings.hpp"
#include "game_src/unresolved/frontend_canvas.hpp"
#include "game_src/unresolved/frontend_custom.hpp"
#include "game_src/unresolved/garage_session.hpp"
#include "game_src/unresolved/race_loadout.hpp"
#include "game_src/unresolved/sound_options.hpp"
#include <array>
#include <memory>
#include <optional>

namespace roadwars {
class FrontendVideo;
class FrontendScene;
struct FrontendVideoAudio;
struct FrontendInput {
    bool left{}, right{}, up{}, down{}, accept{}, back{}, any_key{};
    bool pointer_moved{}, pointer_accept{};
    float pointer_x{}, pointer_y{};
};
struct FrontendScreenInfo {
    std::string_view id, config, icons;
    bool custom{};
};
struct FrontendSelection {
    std::string value, label;
    bool available{}, fade{};
    float radius{50};
};
enum class FrontendRaceMode { none, single, quick, attract };
struct FrontendRaceRequest {
    FrontendRaceMode mode{FrontendRaceMode::none};
    std::string character, track;
    int laps{3};
    std::uint32_t seed{1};
    std::optional<RaceLoadout> player_loadout;
};
class MenuFrontend {
  public:
    MenuFrontend(ResourceStore&, Renderer&, GraphicsSettingsHost* graphics = nullptr);
    ~MenuFrontend();
    MenuFrontend(const MenuFrontend&) = delete;
    MenuFrontend& operator=(const MenuFrontend&) = delete;
    Result start(std::string& error, std::string_view initial_screen = "publisher_1");
    Result show_pause(std::span<const HudQuad> backdrop, std::string& error,
                      bool race_finished = false, bool replay_available = true);
    Result advance(double elapsed, const FrontendInput&, std::string& error);
    Result preview(std::string_view id, double seconds, std::string_view variant,
                   std::string& error);
    Result show_race_results(std::span<const FrontendPreviewRacer>, std::string_view track,
                             std::string& error);
    std::string_view screen() const { return screen_; }
    std::string_view action() const { return action_; }
    std::string_view assertion_message() const { return assertion_message_; }
    int selected() const { return selected_; }
    std::span<const FrontendOptionRow> option_rows() const { return custom_.rows; }
    const GarageSession* garage_session() const { return garage_.get(); }
    std::string_view garage_message() const { return garage_message_; }
    GarageSelection garage_selection() const { return garage_selection_; }
    bool garage_confirmation_pending() const { return garage_pending_selection_.has_value(); }
    bool garage_notice_pending() const { return !garage_message_.empty(); }
    bool garage_confirmation_yes() const { return garage_confirm_yes_; }
    double screen_time() const { return time_; }
    bool quit_requested() const { return quit_; }
    const FrontendVideoAudio* video_audio() const;
    float sfx_gain() const { return sfx_gain_; }
    void set_sound_options_host(SoundOptionsHost* host) { sound_host_ = host; }
    float music_volume() const;
    bool take_audio_restart_requested() {
        const bool requested = audio_restart_requested_;
        audio_restart_requested_ = false;
        return requested;
    }
    std::vector<std::string> take_sound_events();
    std::span<const FrontendSelection> selections() const { return selections_; }
    std::span<const HudQuad> commands() const { return canvas_.commands(); }
    void set_race_seed(std::uint32_t seed) { race_request_.seed = seed; }
    const FrontendRaceRequest& race_request() const { return race_request_; }
    bool race_requested() const {
        return screen_ == "loading" && live_loading_ && loading_target_ == "race";
    }
    Result set_loading_progress(float fraction, std::string_view message, std::string& error);
    bool loading_fade_complete() const {
        return custom_.loading_progress >= 1 && loading_finish_time_ >= .3;
    }
    float loading_progress() const { return custom_.loading_progress; }
    bool loading_intro_complete() const {
        return screen_ == "loading" && !loading_black_pending_ &&
               time_ - loading_fade_origin_ >= .3;
    }
    void release_loading_black() {
        if (loading_black_pending_) {
            loading_black_pending_ = false;
            loading_fade_origin_ = time_;
        }
    }
    std::string music_path() const;
    std::string_view selected_character() const { return character_; }
    float music_gain_scale() const;
    bool menu_music_requested() const;
    static std::span<const FrontendScreenInfo> screens();
    static std::array<int, 2> icon_center(int count, int slot);

  private:
    ResourceStore& resources_;
    SoundOptionsHost* sound_host_{};
    bool audio_restart_requested_{};
    bool loading_black_pending_{};
    double loading_fade_origin_{};
    void configure_sound_rows();
    Result commit_sound_options(std::string& error);
    Result prepare_attract(std::string& error);
    Renderer& renderer_;
    GraphicsSettingsHost* graphics_host_{};
    std::unique_ptr<GraphicsSettingsTransaction> graphics_;
    double graphics_time_{};
    bool graphics_quality_{}, graphics_discard_{};
    bool graphics_save_yes_{}, graphics_leave_after_save_{};
    void refresh_graphics_rows();
    Result advance_graphics(double, const FrontendInput&, std::string&);
    void edit_graphics(std::string_view id, int direction);
    FrontendCanvas canvas_;
    std::unique_ptr<FrontendVideo> video_;
    std::unique_ptr<FrontendScene> scene_;
    std::array<std::unique_ptr<FrontendScene>, 7> prepared_garage_;
    std::array<std::vector<FrontendOptionRow>, 7> prepared_garage_rows_;
    int garage_loading_step_{}, active_scene_variant_{};
    bool garage_prepared_{};
    std::unique_ptr<GarageSession> garage_;
    GarageSelection garage_selection_{};
    std::optional<GarageSelection> garage_pending_selection_;
    std::optional<GarageSelection> garage_pending_item_;
    std::vector<std::string> garage_confirmations_;
    std::size_t garage_confirmation_index_{};
    std::string garage_message_;
    std::vector<HudQuad> garage_modal_backdrop_;
    bool garage_confirm_yes_{};
    Result refresh_garage(bool select_initial, std::string& error);
    Result advance_garage(double, const FrontendInput&, std::string&);
    Result commit_garage(GarageSelection, bool confirmed, std::string&);
    Result draw_garage_modal(std::string& error);
    Result advance_garage_loading(double elapsed, std::string& error);
    FrontendSprite video_frame_;
    int video_index_{-1};
    std::string screen_, title_, action_, variant_, character_{"ANIMAL"}, track_{"DARK FOREST"};
    std::string connection_{"IPX"};
    std::string assertion_message_;
    void show_unimplemented_action();
    std::vector<std::string> history_;
    std::vector<std::string> sounds_;
    float sfx_gain_{1}, music_gain_{1};
    std::vector<FrontendSelection> selections_;
    FrontendCustomState custom_;
    std::vector<HudQuad> backdrop_;
    std::map<std::string, std::vector<FrontendOptionRow>> option_values_;
    FrontendInput previous_;
    double time_{}, exit_time_{-1}, fade_seconds_{.3}, timeout_seconds_{};
    double pulse_time_{};
    bool back_enabled_{}, fade_in_{}, times_out_{}, preview_{}, quit_{};
    bool live_results_{}, live_pause_{}, live_loading_{};
    bool pause_race_finished_{};
    FrontendRaceRequest race_request_;
    std::string loading_target_{"race"};
    double loading_finish_time_{};
    Result prepare_race(std::string& error);
    int selected_{}, scroll_{}, language_{};
    std::string pending_;
    Result enter(std::string_view id, std::string& error);
    Result draw(std::string& error);
    Result draw_custom(std::string& error);
    Result update_store_selection(std::string& error);
    Result activate(std::string& error);
    Result navigate(std::string_view id, std::string& error);
    void move(int dx, int dy);
    std::string translate(std::string_view table, std::string_view key);
};
} // namespace roadwars
