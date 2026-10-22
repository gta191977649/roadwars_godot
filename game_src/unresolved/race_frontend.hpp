#pragma once
#include "game_src/unresolved/bitmap.hpp"
#include "game_src/unresolved/resource_store.hpp"
#include <optional>

namespace roadwars {
struct HudRect {
    int x{}, y{}, width{}, height{};
};
// New portable representations, not recovered original class or source-path claims.
class HudBitmapFont {
  public:
    Result decode(const Bitmap&, std::span<const unsigned char> alphabet);
    const HudRect& glyph(unsigned char character) const { return glyphs_[character]; }
    int height() const { return height_; }
    int space_width() const { return space_width_; }

  private:
    std::array<HudRect, 256> glyphs_{};
    int height_{}, space_width_{};
};
struct FrontendRacer {
    Vec3 position;
    std::size_t character_index{};
    bool hidden_on_map{}, eliminated{}, finished{};
};
struct FrontendRaceInfo {
    int completed_laps{}, total_laps{}, rank{}, racer_count{};
};
struct FrontendFrame {
    bool developer_build{};
    std::span<const FrontendRacer> racers;
    std::span<const std::size_t> ranking;
    std::size_t player_index{};
    // Speed is the original cached modulus in metres/second.
    float speed{}, rpm{};
    int gear_index{};
    bool reverse{};
    // No fabricated lap/rank data in the free-driving profile.
    std::optional<FrontendRaceInfo> race_info;
    std::string status_text;
    std::optional<float> start_countdown;
};
struct FrontendMapSettings {
    Vec3 start;
    float scale{};
    int start_x{}, start_y{}, rotation_degrees{};
    std::array<int, 2> project(Vec3 position, int x = 0, int y = 352) const;
};
class RaceFrontend {
  public:
    explicit RaceFrontend(Renderer& renderer) : renderer_(renderer) {}
    ~RaceFrontend();
    RaceFrontend(const RaceFrontend&) = delete;
    RaceFrontend& operator=(const RaceFrontend&) = delete;
    Result load(ResourceStore&, std::string_view track, Vec3 start,
                std::span<const std::string> characters, std::string& error);
    Result update(const FrontendFrame&);
    void stop();
    const FrontendMapSettings& map_settings() const { return map_; }
    std::span<const HudQuad> commands() const { return commands_; }
    void set_visible(bool visible) { visible_ = visible; }

  private:
    struct Sprite {
        TextureHandle texture;
        int width{}, height{};
    };
    Result load_sprite(ResourceStore&, const std::string&, Sprite&, Bitmap* decoded = nullptr);
    void sprite(const Sprite&, HudRect destination, float alpha = 1, float tint = 1,
                float degrees = 0, std::array<float, 2> pivot = {});
    void text(int x, int y, std::string_view, bool countdown = false);
    Renderer& renderer_;
    bool loaded_{}, visible_{true}, speedometer_{}, track_map_{}, lap_update_{};
    FrontendMapSettings map_;
    HudBitmapFont font_, countdown_font_;
    Sprite meter_, needle_, map_texture_, border_, font_texture_, countdown_texture_, eliminated_,
        finished_;
    std::vector<Sprite> dots_, portraits_;
    std::vector<TextureHandle> textures_;
    std::vector<HudQuad> commands_;
};
} // namespace roadwars
