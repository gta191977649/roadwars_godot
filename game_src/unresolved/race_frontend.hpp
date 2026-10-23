#pragma once
#include "game_src/unresolved/bitmap.hpp"
#include "game_src/unresolved/resource_store.hpp"
#include <optional>
#include <memory>

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
struct FrontendEquipmentFrame {
    int selected_item{};
    std::array<int, 3> ammunition{};
    std::array<bool, 3> present{};
    int nitrous_ammunition{};
    bool weapons_unlocked{};
    float internal_hp{}, maximum_internal_hp{};
    std::array<float, 4> armor_hp{}, maximum_armor_hp{};
};
struct AmmunitionNotice {
    std::string text;
    float remaining{};
};
// Four original fixed text lanes, with simulation-owned lifetimes.
class AmmunitionNotices {
  public:
    bool push(std::string_view name, int remaining);
    Result advance(float delta);
    void clear() { entries_ = {}; }
    std::span<const AmmunitionNotice> entries() const { return entries_; }
  private:
    std::array<AmmunitionNotice, 4> entries_;
};
struct FrontendFrame {
    std::span<const AmmunitionNotice> ammunition_notices;
    bool cockpit{};
    std::array<std::string, 3> cockpit_weapon_names;
    std::size_t cockpit_character{};
    float steering_fraction{}, viewport_aspect{4.f / 3};
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
    // Race-loop overlay at004823af, independent of weapon icon brightness.
    bool weapon_lock_warning{};
    std::optional<float> start_countdown;
    std::optional<FrontendEquipmentFrame> equipment;
};
struct FrontendMapSettings {
    Vec3 start;
    float scale{};
    int start_x{}, start_y{}, rotation_degrees{};
    std::array<int, 2> project(Vec3 position, int x = 0, int y = 352) const;
};
class CockpitFrontend;
class RaceFrontend {
  public:
    explicit RaceFrontend(Renderer& renderer);
    ~RaceFrontend();
    RaceFrontend(const RaceFrontend&) = delete;
    RaceFrontend& operator=(const RaceFrontend&) = delete;
    Result load(ResourceStore&, std::string_view track, Vec3 start,
                std::span<const std::string> characters, std::string& error);
    Result update(const FrontendFrame&);
    Result configure_equipment(ResourceStore&, std::string_view car,
                               const std::array<std::string, 3>& weapons, std::string& error);
    void stop();
    const FrontendMapSettings& map_settings() const { return map_; }
    std::span<const HudQuad> commands() const { return commands_; }
    void set_visible(bool visible) { visible_ = visible; }
    void start_fade_in() { fade_remaining_ = .5f; }
    void advance_fade(float delta);

  private:
    struct Sprite {
        TextureHandle texture;
        int width{}, height{};
    };
    Result load_sprite(ResourceStore&, const std::string&, Sprite&, Bitmap* decoded = nullptr);
    void sprite(const Sprite&, HudRect destination, float alpha = 1, float tint = 1,
                float degrees = 0, std::array<float, 2> pivot = {});
    void text(int x, int y, std::string_view, bool countdown = false);
    Result draw_equipment(const FrontendEquipmentFrame&);
    Result draw_ammunition_notices(std::span<const AmmunitionNotice>);
    Result submit();
    Renderer& renderer_;
    std::unique_ptr<CockpitFrontend> cockpit_;
    bool loaded_{}, visible_{true}, speedometer_{}, track_map_{}, lap_update_{};
    FrontendMapSettings map_;
    HudBitmapFont font_, countdown_font_;
    Sprite meter_, needle_, map_texture_, border_, font_texture_, countdown_texture_, eliminated_,
        finished_;
    std::vector<Sprite> dots_, portraits_;
    std::vector<TextureHandle> textures_;
    std::vector<HudQuad> commands_;
    std::array<Sprite, 3> weapon_icons_;
    Sprite nitrous_icon_, damage_background_, damage_shell_;
    std::array<HudRect, 5> damage_regions_;
    int internal_offset_{}, shell_offset_{};
    bool equipment_loaded_{};
    Sprite fade_pixel_;
    float fade_remaining_{};
};
} // namespace roadwars
