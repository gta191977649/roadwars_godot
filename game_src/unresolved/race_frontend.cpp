#include "game_src/unresolved/race_frontend.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace roadwars {
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004e14dc
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004e183d
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004e1890
Result HudBitmapFont::decode(const Bitmap& bitmap, std::span<const unsigned char> alphabet) {
    glyphs_ = {};
    height_ = space_width_ = 0;
    if (!bitmap.width || !bitmap.height || bitmap.width > 4096 || bitmap.height > 4096 ||
        bitmap.rgba.size() != static_cast<std::size_t>(bitmap.width) * bitmap.height * 4 ||
        alphabet.empty())
        return Result::invalid_argument;
    const int width = static_cast<int>(bitmap.width), height = static_cast<int>(bitmap.height);
    const auto ink = [&](int x, int y) {
        const auto pixel = (static_cast<std::size_t>(y) * width + x) * 4;
        return std::to_integer<unsigned>(bitmap.rgba[pixel]) >= 8 ||
               std::to_integer<unsigned>(bitmap.rgba[pixel + 1]) >= 8 ||
               std::to_integer<unsigned>(bitmap.rgba[pixel + 2]) >= 8;
    };
    const auto empty_row = [&](int y) {
        for (int x = 0; x < width; ++x)
            if (ink(x, y))
                return false;
        return true;
    };
    while (height_ < height && !empty_row(height_))
        ++height_;
    if (!height_ || height_ == height)
        return Result::invalid_argument;
    int x = 0, y = 0;
    for (auto character : alphabet) {
        if (y + height_ > height)
            return Result::invalid_argument;
        const int start = x;
        const auto empty_column = [&](int column) {
            for (int row = y; row < y + height_; ++row)
                if (ink(column, row))
                    return false;
            return true;
        };
        while (x < width && !empty_column(x))
            ++x;
        if (x == width || x == start)
            return Result::invalid_argument;
        glyphs_[character] = {start, y, x - start, height_};
        ++x;
        if (x >= width || empty_column(x)) {
            x = 0;
            while (y < height && !empty_row(y))
                ++y;
            ++y;
        }
    }
    space_width_ = glyphs_[alphabet.front()].width / 2 + 1;
    return Result::ok;
}

// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0048ace2
std::array<int, 2> FrontendMapSettings::project(Vec3 position, int x, int y) const {
    const float dx = (position.x - start.x) * scale;
    const float dz = (position.z - start.z) * scale;
    float px{}, py{};
    switch (rotation_degrees) {
    case 90:
        px = -dz;
        py = -dx;
        break;
    case -90:
        px = dz;
        py = dx;
        break;
    case 180:
        px = dx;
        py = -dz;
        break;
    default:
        px = -dx;
        py = dz;
        break;
    }
    return {static_cast<int>(static_cast<float>(x + start_x) + px + 0.5f),
            static_cast<int>(static_cast<float>(y + start_y) + py + 0.5f)};
}
RaceFrontend::~RaceFrontend() { stop(); }
void RaceFrontend::stop() {
    equipment_loaded_ = false;
    if (loaded_ || !textures_.empty())
        renderer_.draw_hud({});
    for (auto texture : textures_)
        renderer_.destroy_texture(texture);
    textures_.clear();
    commands_.clear();
    dots_.clear();
    portraits_.clear();
    loaded_ = false;
}
Result RaceFrontend::load_sprite(ResourceStore& resources, const std::string& path, Sprite& sprite,
                                 Bitmap* decoded) {
    std::vector<std::byte> bytes;
    Bitmap bitmap, alpha;
    auto result = resources.read(path, bytes);
    if (result != Result::ok)
        return result;
    if ((result = decode_bitmap(bytes, bitmap)) != Result::ok)
        return result;
    if (decoded)
        *decoded = bitmap;
    const auto alpha_path = original_alpha_path(path);
    if (!alpha_path.empty()) {
        if ((result = resources.read(alpha_path, bytes)) != Result::ok)
            return result;
        if ((result = decode_bitmap(bytes, alpha)) != Result::ok)
            return result;
    }
    TextureTransparency transparency;
    result =
        prepare_original_texture(path, bitmap, transparency, alpha_path.empty() ? nullptr : &alpha);
    if (result != Result::ok)
        return result;
    sprite = {renderer_.upload_texture(bitmap.width, bitmap.height, bitmap.rgba),
              static_cast<int>(bitmap.width), static_cast<int>(bitmap.height)};
    if (!sprite.texture.value)
        return Result::unavailable;
    textures_.push_back(sprite.texture);
    return Result::ok;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00487940
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00487fbe
Result RaceFrontend::load(ResourceStore& resources, std::string_view track, Vec3 start,
                          std::span<const std::string> characters, std::string& error) {
    stop();
    const auto fail = [&](Result result, const std::string& detail) {
        error = "Race frontend: " + detail;
        stop();
        return result;
    };
    if (characters.empty() || characters.size() > 6)
        return fail(Result::invalid_argument, "expected one to six characters");
    Configuration options, world;
    if (options.load(resources, "RaceInterface/Options.ini") != Result::ok ||
        world.load(resources, "Tracks/" + std::string(track) + "/World/Data.txt") != Result::ok)
        return fail(Result::not_found, "original configuration missing");
    float speed{}, map{}, lap{}, scale{}, sx{}, sy{}, rotation{};
    if (options.number("DATA", "SPEEDOMETER", speed) != Result::ok ||
        options.number("DATA", "TRACKMAP", map) != Result::ok ||
        options.number("DATA", "LAPUPDATE", lap) != Result::ok ||
        world.number("DATA", "TRACKTO2DMAPSCALE", scale) != Result::ok ||
        world.number("DATA", "START1_2DMAPX", sx) != Result::ok ||
        world.number("DATA", "START1_2DMAPY", sy) != Result::ok ||
        world.number("DATA", "MAPROTATIONDEGREES", rotation) != Result::ok)
        return fail(Result::invalid_argument, "original HUD/map fields missing");
    for (float value : {speed, map, lap, scale, sx, sy, rotation, start.x, start.z})
        if (!std::isfinite(value) || std::abs(value) > 1000000)
            return fail(Result::invalid_argument, "invalid HUD/map field");
    if (scale <= 0 || scale > 1000)
        return fail(Result::invalid_argument, "invalid map scale");
    speedometer_ = speed != 0;
    track_map_ = map != 0;
    lap_update_ = lap != 0;
    map_ = {start, scale, static_cast<int>(sx), static_cast<int>(sy), static_cast<int>(rotation)};
    const std::array<std::pair<std::string, Sprite*>, 8> assets{
        {{"RaceInterface/xxSpeedOMeter.bmp", &meter_},
         {"RaceInterface/16SpeedOMeterNeedle.bmp", &needle_},
         {"Tracks/" + std::string(track) + "/Map/16Map.bmp", &map_texture_},
         {"RaceInterface/16MapDotBorder.bmp", &border_},
         {"RaceInterface/16MapEliminated.bmp", &eliminated_},
         {"RaceInterface/16MapFinished.bmp", &finished_},
         {"Fonts/16Font1.bmp", &font_texture_},
         {"Fonts/16321go.bmp", &countdown_texture_}}};
    Bitmap font_bitmap, countdown_bitmap;
    for (const auto& [path, sprite] : assets) {
        const auto result = load_sprite(resources, path, *sprite,
                                        sprite == &font_texture_        ? &font_bitmap
                                        : sprite == &countdown_texture_ ? &countdown_bitmap
                                                                        : nullptr);
        if (result != Result::ok)
            return fail(result, path);
    }
    // First 39 characters of the original alphabet at 0061a0c8. Extended localized
    // glyphs are not required by this numeric/English free-driving presentation.
    constexpr std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ',0123456789/";
    if (font_.decode(font_bitmap, {reinterpret_cast<const unsigned char*>(alphabet.data()),
                                   alphabet.size()}) != Result::ok)
        return fail(Result::invalid_argument, "invalid original font atlas");
    // 00487fbe, alphabet321G at0061a13c: G selects the complete GO artwork.
    constexpr std::array<unsigned char, 4> countdown_alphabet{'3', '2', '1', 'G'};
    if (countdown_font_.decode(countdown_bitmap, countdown_alphabet) != Result::ok)
        return fail(Result::invalid_argument, "invalid original countdown atlas");
    for (const auto& character : characters) {
        Sprite dot, portrait;
        for (const auto& [name, sprite] : std::array<std::pair<const char*, Sprite*>, 2>{
                 {{"16MapDot.bmp", &dot}, {"16MapPortrait.bmp", &portrait}}}) {
            const auto path = "Characters/" + character + "/" + name;
            const auto result = load_sprite(resources, path, *sprite);
            if (result != Result::ok)
                return fail(result, path);
        }
        dots_.push_back(dot);
        portraits_.push_back(portrait);
    }
    visible_ = loaded_ = true;
    return Result::ok;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004e2490
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004e2ac6
void RaceFrontend::sprite(const Sprite& sprite, HudRect rect, float alpha, float tint,
                          float degrees, std::array<float, 2> pivot) {
    HudQuad quad;
    quad.texture = sprite.texture;
    const float x = static_cast<float>(rect.x), y = static_cast<float>(rect.y);
    quad.positions = {
        {{x, y}, {x + rect.width, y}, {x + rect.width, y + rect.height}, {x, y + rect.height}}};
    quad.uv = {{{0, 1}, {1, 1}, {1, 0}, {0, 0}}};
    for (auto& color : quad.colors)
        color = {tint, tint, tint, alpha};
    if (degrees != 0) {
        const float angle = degrees * std::numbers::pi_v<float> / 180;
        const float c = std::cos(angle), s = std::sin(angle);
        for (auto& point : quad.positions) {
            const float dx = point[0] - pivot[0], dy = point[1] - pivot[1];
            point = {pivot[0] + dx * c - dy * s, pivot[1] + dx * s + dy * c};
        }
    }
    commands_.push_back(quad);
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004e0a26
void RaceFrontend::text(int x, int y, std::string_view value, bool countdown) {
    const auto& font = countdown ? countdown_font_ : font_;
    const auto& texture = countdown ? countdown_texture_ : font_texture_;
    for (unsigned char character : value) {
        if (character == ' ') {
            x += font.space_width();
            continue;
        }
        const auto& glyph = font.glyph(character);
        if (!glyph.width)
            continue;
        sprite(texture, {x, y, glyph.width, glyph.height});
        auto& quad = commands_.back();
        const float u = static_cast<float>(glyph.x) / texture.width;
        const float v = 1 - static_cast<float>(glyph.y) / texture.height;
        const float right = static_cast<float>(glyph.x + glyph.width) / texture.width;
        const float bottom = 1 - static_cast<float>(glyph.y + glyph.height) / texture.height;
        quad.uv = {{{u, v}, {right, v}, {right, bottom}, {u, bottom}}};
        for (int i = 0; i < 4; ++i)
            quad.colors[i][3] = i < 2 ? 178.0f / 255 : 102.0f / 255;
        x += glyph.width + 2;
    }
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0048b37d
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0048be66
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0048b648
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0048b7b4
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0048b120
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0048cf43
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004e13f7
Result RaceFrontend::update(const FrontendFrame& frame) {
    commands_.clear();
    if (!loaded_)
        return Result::unavailable;
    if (frame.start_countdown &&
        (!std::isfinite(*frame.start_countdown) || *frame.start_countdown < 0))
        return Result::invalid_argument;
    if (frame.racers.empty() || frame.racers.size() > 6 ||
        frame.player_index >= frame.racers.size() || !std::isfinite(frame.speed) ||
        !std::isfinite(frame.rpm) || std::abs(frame.speed) > 1000000 ||
        std::abs(frame.rpm) > 1000000 || frame.gear_index < 0 || frame.gear_index > 6)
        return Result::invalid_argument;
    for (const auto& racer : frame.racers)
        if (racer.character_index >= dots_.size() || !std::isfinite(racer.position.x) ||
            !std::isfinite(racer.position.z) || std::abs(racer.position.x) > 1000000 ||
            std::abs(racer.position.z) > 1000000 ||
            std::abs((racer.position.x - map_.start.x) * map_.scale) > 100000000 ||
            std::abs((racer.position.z - map_.start.z) * map_.scale) > 100000000)
            return Result::invalid_argument;
    const auto& player = frame.racers[frame.player_index];
    if (frame.weapon_lock_warning && !player.eliminated) {
        // 004823af..00482446: original English localization key006196e4.
        // The native anchor is300 (0x12c), rather than the other HUD labels'320.
        constexpr std::string_view label = "WEAPONS LOCKED";
        int width = -2;
        for (unsigned char character : label)
            width += (character == ' ' ? font_.space_width() : font_.glyph(character).width + 2);
        const auto first = commands_.size();
        text(300 - width / 2, 5, label);
        for (auto i = first; i < commands_.size(); ++i)
            for (std::size_t vertex = 0; vertex < 4; ++vertex)
                commands_[i].colors[vertex] = {1, vertex < 2 ? 0.f : 1.f, 0, 102.f / 255};
    }
    if (!visible_)
        return renderer_.draw_hud(commands_);
    const bool terminal = player.eliminated || player.finished;
    if (!terminal && frame.start_countdown && *frame.start_countdown > 0) {
        const float remaining = *frame.start_countdown;
        const int digit = static_cast<int>(remaining);
        if (digit < 4) {
            const std::string value = digit == 0 ? "G" : std::to_string(digit);
            const float fraction = remaining - digit;
            const float scale = 2 * (1 - fraction);
            const auto first = commands_.size();
            text(0, 0, value, true);
            float width = 0;
            for (unsigned char character : value)
                width += countdown_font_.glyph(character).width + 2;
            width -= 2;
            for (auto index = first; index < commands_.size(); ++index) {
                auto& quad = commands_[index];
                for (int vertex = 0; vertex < 4; ++vertex) {
                    quad.positions[vertex][0] =
                        320 + (quad.positions[vertex][0] - width * .5f) * scale;
                    quad.positions[vertex][1] =
                        240 + (quad.positions[vertex][1] - countdown_font_.height() * .5f) * scale;
                    quad.colors[vertex] = {1, 1, 1, std::floor(fraction * 255) / 255};
                }
            }
        }
        if (remaining >= 3)
            return renderer_.draw_hud(commands_);
    }
    if (!terminal && speedometer_) {
        sprite(meter_, {512, 352, meter_.width, meter_.height});
        text(583, 425, std::to_string(static_cast<int>(frame.speed * 2.236997365951538f)));
        sprite(needle_, {546, 398, needle_.width, needle_.height}, 1, 1,
               (frame.rpm / 8000.0f) * 165.0f + 60.0f, {577, 418});
        constexpr std::array<std::string_view, 7> gears{"1ST", "2ND", "3RD", "4TH",
                                                        "5TH", "6TH", "7TH"};
        text(570, 452, frame.reverse ? "REV" : gears[frame.gear_index]);
    }
    // New diagnostic label requested by the host, not original game behavior.
    if (frame.developer_build) {
        constexpr std::string_view label = "DEV BUILD BY NURUPO";
        int width = -2;
        for (unsigned char character : label)
            width += (character == ' ' ? font_.space_width() : font_.glyph(character).width + 2);
        text((640 - width) / 2, 480 - font_.height() - 5, label);
    }
    if (!terminal && lap_update_ && frame.race_info) {
        const auto& race = *frame.race_info;
        text(5, 5,
             "LAP " + std::to_string(std::min(race.completed_laps + 1, race.total_laps)) + "/" +
                 std::to_string(race.total_laps));
        text(5, font_.height() + 10,
             std::to_string(race.rank) + "/" + std::to_string(race.racer_count));
    }
    if (!frame.status_text.empty()) {
        int width = -2;
        for (unsigned char character : frame.status_text)
            width += (character == ' ' ? font_.space_width() : font_.glyph(character).width + 2);
        text((640 - width) / 2, 100, frame.status_text);
    }
    if (!terminal && frame.equipment) {
        const auto result = draw_equipment(*frame.equipment);
        if (result != Result::ok)
            return result;
    }
    if ((terminal || track_map_) &&
        (terminal || !frame.equipment || frame.equipment->selected_item == 0)) {
        sprite(map_texture_, {0, 352, 128, 128}, 0.7f);
        for (std::size_t i = 0; i < frame.racers.size(); ++i) {
            const auto& racer = frame.racers[i];
            if (racer.hidden_on_map && i != frame.player_index)
                continue;
            const auto& dot = dots_[racer.character_index];
            const auto position = map_.project(racer.position);
            HudRect rect{position[0] - dot.width / 2, position[1] - dot.height / 2, dot.width,
                         dot.height};
            sprite(dot, rect, 1, i == frame.player_index ? 1 : 100.0f / 255);
            if (i == frame.player_index)
                sprite(border_, rect);
        }
        for (std::size_t slot = 0; slot < frame.racers.size(); ++slot) {
            if (!frame.ranking.empty() && frame.ranking.size() != frame.racers.size())
                return Result::invalid_argument;
            const auto i = frame.ranking.empty() ? slot : frame.ranking[slot];
            if (i >= frame.racers.size())
                return Result::invalid_argument;
            const auto& racer = frame.racers[i];
            const auto tint = i == frame.player_index ? 1.0f : 100.0f / 255;
            const HudRect rect{4, 360 + static_cast<int>(slot) * 20, 17, 17};
            sprite(portraits_[racer.character_index], rect, 1, tint);
            if (racer.eliminated)
                sprite(eliminated_, rect, 1, tint);
            else if (racer.finished)
                sprite(finished_, rect, 1, tint);
        }
    }
    if (player.eliminated) {
        constexpr std::string_view label = "CAR ELIMINATED";
        int width = -2;
        for (unsigned char character : label)
            width += (character == ' ' ? font_.space_width() : font_.glyph(character).width + 2);
        const auto first = commands_.size();
        text((640 - width) / 2, 400, label);
        for (auto i = first; i < commands_.size(); ++i)
            for (std::size_t vertex = 0; vertex < 4; ++vertex)
                commands_[i].colors[vertex] = {229.f / 255, vertex < 2 ? 0.f : 229.f / 255, 0,
                                               229.f / 255};
    }
    return renderer_.draw_hud(commands_);
}
} // namespace roadwars
