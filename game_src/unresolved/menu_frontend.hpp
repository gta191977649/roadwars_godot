#pragma once
#include "game_src/unresolved/frontend_canvas.hpp"
#include "game_src/unresolved/frontend_custom.hpp"
#include <memory>

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
class MenuFrontend {
  public:
    MenuFrontend(ResourceStore&, Renderer&);
    ~MenuFrontend();
    MenuFrontend(const MenuFrontend&) = delete;
    MenuFrontend& operator=(const MenuFrontend&) = delete;
    Result start(std::string& error, std::string_view initial_screen = "publisher_1");
    Result show_pause(std::span<const HudQuad> backdrop, std::string& error);
    Result advance(double elapsed, const FrontendInput&, std::string& error);
    Result preview(std::string_view id, double seconds, std::string_view variant,
                   std::string& error);
    Result show_race_results(std::span<const FrontendPreviewRacer>, std::string_view track,
                             std::string& error);
    std::string_view screen() const { return screen_; }
    std::string_view action() const { return action_; }
    int selected() const { return selected_; }
    double screen_time() const { return time_; }
    bool quit_requested() const { return quit_; }
    const FrontendVideoAudio* video_audio() const;
    float sfx_gain() const { return sfx_gain_; }
    std::vector<std::string> take_sound_events();
    std::span<const FrontendSelection> selections() const { return selections_; }
    std::span<const HudQuad> commands() const { return canvas_.commands(); }
    static std::span<const FrontendScreenInfo> screens();
    static std::array<int, 2> icon_center(int count, int slot);

  private:
    ResourceStore& resources_;
    Renderer& renderer_;
    FrontendCanvas canvas_;
    std::unique_ptr<FrontendVideo> video_;
    std::unique_ptr<FrontendScene> scene_;
    FrontendSprite video_frame_;
    int video_index_{-1};
    std::string screen_, title_, action_, variant_, character_{"ANIMAL"}, track_{"DARK FOREST"};
    std::string connection_{"IPX"};
    std::vector<std::string> history_;
    std::vector<std::string> sounds_;
    float sfx_gain_{1};
    std::vector<FrontendSelection> selections_;
    FrontendCustomState custom_;
    std::vector<HudQuad> backdrop_;
    std::map<std::string, std::vector<FrontendOptionRow>> option_values_;
    FrontendInput previous_;
    double time_{}, exit_time_{-1}, fade_seconds_{.3}, timeout_seconds_{};
    double pulse_time_{};
    bool back_enabled_{}, fade_in_{}, times_out_{}, preview_{}, quit_{};
    bool live_results_{}, live_pause_{};
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
