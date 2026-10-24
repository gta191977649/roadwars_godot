#include "game_src/unresolved/frontend_canvas.hpp"
#include "game_src/unresolved/frontend_localization.hpp"
#include <algorithm>

namespace roadwars {
FrontendCanvas::~FrontendCanvas() {
    renderer_.draw_hud({});
    for (auto& [path, sprite] : assets_)
        renderer_.destroy_texture(sprite.texture);
    release(white_);
}
Result FrontendCanvas::bitmap(const Bitmap& bitmap, FrontendSprite& output) {
    output = {renderer_.upload_texture(bitmap.width, bitmap.height, bitmap.rgba),
              static_cast<int>(bitmap.width), static_cast<int>(bitmap.height)};
    return output.texture.value ? Result::ok : Result::unavailable;
}
void FrontendCanvas::release(FrontendSprite& sprite) {
    if (sprite.texture.value) {
        renderer_.draw_hud({});
        renderer_.destroy_texture(sprite.texture);
    }
    sprite = {};
}
Result FrontendCanvas::asset(std::string_view path, FrontendSprite& output) {
    const auto key = ResourceStore::canonical(path);
    if (const auto found = assets_.find(key); found != assets_.end()) {
        output = found->second;
        return Result::ok;
    }
    std::vector<std::byte> bytes;
    Bitmap decoded, alpha;
    auto result = resources_.read(path, bytes);
    if (result == Result::ok)
        result = decode_bitmap(bytes, decoded);
    const auto alpha_path = original_alpha_path(path);
    if (result == Result::ok && !alpha_path.empty()) {
        result = resources_.read(alpha_path, bytes);
        if (result == Result::ok)
            result = decode_bitmap(bytes, alpha);
    }
    TextureTransparency transparency;
    if (result == Result::ok)
        result = prepare_original_texture(path, decoded, transparency,
                                          alpha_path.empty() ? nullptr : &alpha);
    if (result == Result::ok)
        result = bitmap(decoded, output);
    if (result != Result::ok) {
        error_ = "Frontend asset unavailable or malformed: " + std::string(path);
        return result;
    }
    assets_.emplace(key, output);
    return Result::ok;
}
Result FrontendCanvas::load(std::string& error) {
    if (white_.texture.value)
        return Result::ok;
    constexpr std::array<std::string_view, 9> paths{
        "Fonts/16ScreenTitleFont.bmp", "Fonts/16Font12Imp.bmp", "Fonts/16Font18Imp.bmp",
        "Fonts/16FontArial15.bmp",     "Fonts/16Font1.bmp",     "Fonts/16FontCyber16.bmp",
        "Fonts/16FontCyber14.bmp",     "Fonts/16Score12.bmp",   "Fonts/16Nebula12.bmp"};
    for (std::size_t i = 0; i < paths.size(); ++i) {
        std::vector<std::byte> bytes;
        Bitmap decoded;
        auto result = resources_.read(paths[i], bytes);
        if (result == Result::ok)
            result = decode_bitmap(bytes, decoded);
        if (result == Result::ok)
            result = fonts_[i].glyphs.decode(
                decoded,
                i == 0   ? frontend_title_alphabet()
                : i == 2 ? frontend_option_alphabet()
                : i == 4 ? frontend_menu_alphabet()
                : i == 5 ? frontend_cyber_alphabet(false)
                : i == 6
                    ? frontend_cyber_alphabet(true)
                    :
                    // The sampled Score12 atlas contains 137 slots; its final inverted-question
                    // slot from the shared 138-byte text alphabet is absent, and is unavailable.
                    i == 7 ? frontend_text_alphabet().first(frontend_text_alphabet().size() - 1)
                : i == 8   ? frontend_garage_alphabet()
                           : frontend_text_alphabet());
        if (result == Result::ok)
            result = asset(paths[i], fonts_[i].sprite);
        if (result != Result::ok) {
            error = "Frontend font unavailable or malformed: " + std::string(paths[i]);
            return result;
        }
        fonts_[i].spacing = i == 1 || i >= 5 ? 1 : i == 3 ? 0 : 2;
    }
    Bitmap white;
    white.width = white.height = 1;
    white.rgba.assign(4, std::byte{255});
    return bitmap(white, white_);
}
void FrontendCanvas::begin() {
    commands_.clear();
    frame_result_ = Result::ok;
}
void FrontendCanvas::append(std::span<const HudQuad> commands) {
    commands_.insert(commands_.end(), commands.begin(), commands.end());
}
Result FrontendCanvas::submit() {
    return frame_result_ == Result::ok ? renderer_.draw_hud(commands_) : frame_result_;
}
void FrontendCanvas::sprite(const FrontendSprite& sprite, HudRect r, float alpha, float tint) {
    HudQuad q;
    q.texture = sprite.texture;
    const float x = static_cast<float>(r.x), y = static_cast<float>(r.y);
    q.positions = {{{x, y}, {x + r.width, y}, {x + r.width, y + r.height}, {x, y + r.height}}};
    q.uv = {{{0, 1}, {1, 1}, {1, 0}, {0, 0}}};
    for (auto& c : q.colors)
        c = {tint, tint, tint, alpha};
    commands_.push_back(q);
}
Result FrontendCanvas::sprite(std::string_view path, HudRect r, float alpha, float tint) {
    FrontendSprite s;
    auto result = asset(path, s);
    if (result == Result::ok)
        sprite(s, r, alpha, tint);
    else
        frame_result_ = result;
    return result;
}
void FrontendCanvas::rectangle(HudRect r, std::array<float, 4> color) {
    sprite(white_, r);
    for (auto& c : commands_.back().colors)
        c = color;
}
int FrontendCanvas::font_height(FrontendFont font, float scale) const {
    return static_cast<int>(fonts_[static_cast<std::size_t>(font)].glyphs.height() * scale);
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004e13f7
int FrontendCanvas::text_width(FrontendFont font, std::string_view value, float scale) const {
    const auto& f = fonts_[static_cast<std::size_t>(font)];
    int width = 0;
    for (unsigned char c : value)
        width += static_cast<int>(
            (c == ' ' ? f.glyphs.space_width() : f.glyphs.glyph(c).width + f.spacing) * scale);
    return width - static_cast<int>(f.spacing * scale);
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004e0a26
void FrontendCanvas::text(FrontendFont font, float x, float y, std::string_view value,
                          bool centered, float scale, std::uint32_t top, std::uint32_t bottom,
                          FrontendVerticalAlign vertical, float top_offset_x) {
    const auto& f = fonts_[static_cast<std::size_t>(font)];
    if (centered)
        x -= text_width(font, value, scale) / 2;
    if (vertical == FrontendVerticalAlign::center)
        y -= static_cast<int>(f.glyphs.height() * scale / 2);
    const auto color = [](std::uint32_t c) {
        return std::array<float, 4>{((c >> 16) & 255) / 255.f, ((c >> 8) & 255) / 255.f,
                                    (c & 255) / 255.f, (c >> 24) / 255.f};
    };
    for (unsigned char c : value) {
        if (c == ' ') {
            x += static_cast<int>(f.glyphs.space_width() * scale);
            continue;
        }
        const auto& g = f.glyphs.glyph(c);
        if (!g.width) {
            x += static_cast<int>(f.spacing * scale);
            continue;
        }
        sprite(f.sprite, {static_cast<int>(x), static_cast<int>(y),
                          static_cast<int>(g.width * scale), static_cast<int>(g.height * scale)});
        auto& q = commands_.back();
        q.positions[0][0] += top_offset_x;
        q.positions[1][0] += top_offset_x;
        const float u = static_cast<float>(g.x) / f.sprite.width;
        const float v = 1 - static_cast<float>(g.y) / f.sprite.height;
        const float right = static_cast<float>(g.x + g.width) / f.sprite.width;
        const float lower = 1 - static_cast<float>(g.y + g.height) / f.sprite.height;
        q.uv = {{{u, v}, {right, v}, {right, lower}, {u, lower}}};
        q.colors = {color(top), color(top), color(bottom), color(bottom)};
        x += static_cast<int>((g.width + f.spacing) * scale);
    }
}
} // namespace roadwars
