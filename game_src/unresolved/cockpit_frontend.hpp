#pragma once
#include "game_src/unresolved/race_frontend.hpp"

namespace roadwars {
// Portable cockpit presentation owner; original class/source ownership is unresolved.
class CockpitFrontend {
  public:
    explicit CockpitFrontend(Renderer& renderer) : renderer_(renderer) {}
    ~CockpitFrontend();
    CockpitFrontend(const CockpitFrontend&) = delete;
    CockpitFrontend& operator=(const CockpitFrontend&) = delete;
    Result load(ResourceStore&, std::string_view track, std::span<const std::string> cars,
                std::string& error);
    Result draw(std::size_t character_index, float steering_fraction, float rpm, float speed,
                int gear, bool reverse, float viewport_aspect, std::vector<HudQuad>& output,
                bool show_map = true) const;
    Result draw_readouts(std::size_t character_index, const std::array<std::string, 3>& names,
                         const std::array<int, 3>& ammunition, const std::array<bool, 3>& present,
                         std::vector<HudQuad>& output) const;
    std::array<int, 2> map_offset(std::size_t character_index) const;
    void stop();

  private:
    struct Sprite {
        TextureHandle texture;
        int width{}, height{};
    };
    struct Car {
        std::array<Sprite, 8> tiles;
        Sprite needle, steering, left_edge, right_edge;
        int wheel_size{}, wheel_x{}, wheel_y{}, tach_x{}, tach_y{}, speed_x{}, speed_y{},
            pivot_x{}, pivot_y{}, map_x{}, map_y{};
        float max_rpm{}, tach_base{}, tach_range{}, max_mph{}, speed_base{}, speed_range{};
        std::array<int, 3> ammo_x{}, ammo_y{}, weapon_x{}, weapon_y{};
    };
    Result load_sprite(ResourceStore&, const std::string&, Sprite&, Bitmap* raw = nullptr);
    static void quad(std::vector<HudQuad>&, const Sprite&, HudRect destination,
                     HudRect source = {}, float degrees = 0,
                     std::array<float, 2> pivot = {});
    Result text(std::vector<HudQuad>&, int x, int y, std::string_view, bool center_x) const;
    Renderer& renderer_;
    std::vector<TextureHandle> textures_;
    std::vector<Car> cars_;
    Sprite map_;
    Sprite font_texture_;
    HudBitmapFont font_;
    std::string track_name_;
};
} // namespace roadwars
