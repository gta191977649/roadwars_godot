#pragma once
#include "game_src/unresolved/race_frontend.hpp"
#include <map>

namespace roadwars {
// Portable presentation infrastructure shared by recovered frontend screens.
enum class FrontendVerticalAlign { top, center };
enum class FrontendFont { title, small, body, arial, menu, cyber16, cyber14, score, nebula };
struct FrontendSprite {
    TextureHandle texture;
    int width{}, height{};
};
class FrontendCanvas {
  public:
    FrontendCanvas(ResourceStore& resources, Renderer& renderer)
        : resources_(resources), renderer_(renderer) {}
    ~FrontendCanvas();
    FrontendCanvas(const FrontendCanvas&) = delete;
    FrontendCanvas& operator=(const FrontendCanvas&) = delete;
    Result load(std::string& error);
    Result asset(std::string_view path, FrontendSprite& output);
    Result bitmap(const Bitmap&, FrontendSprite& output);
    void release(FrontendSprite&);
    void begin();
    void append(std::span<const HudQuad> commands);
    Result submit();
    void sprite(const FrontendSprite&, HudRect, float alpha = 1, float tint = 1);
    Result sprite(std::string_view path, HudRect, float alpha = 1, float tint = 1);
    void rectangle(HudRect, std::array<float, 4> color);
    void text(FrontendFont, float x, float y, std::string_view, bool centered = false,
              float scale = 1, std::uint32_t top = 0xffffffff, std::uint32_t bottom = 0xffffffff,
              FrontendVerticalAlign vertical = FrontendVerticalAlign::top, float top_offset_x = 0);
    int text_width(FrontendFont, std::string_view, float scale = 1) const;
    int font_height(FrontendFont, float scale = 1) const;
    std::span<const HudQuad> commands() const { return commands_; }
    ResourceStore& resources() { return resources_; }
    const std::string& error() const { return error_; }

  private:
    struct Font {
        HudBitmapFont glyphs;
        FrontendSprite sprite;
        int spacing{};
    };
    ResourceStore& resources_;
    Renderer& renderer_;
    std::map<std::string, FrontendSprite> assets_;
    std::array<Font, 9> fonts_;
    FrontendSprite white_;
    std::vector<HudQuad> commands_;
    std::string error_;
    Result frame_result_{Result::ok};
};
} // namespace roadwars
