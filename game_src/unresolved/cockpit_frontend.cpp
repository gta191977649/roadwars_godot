#include "game_src/unresolved/cockpit_frontend.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace roadwars {
namespace {
// Original Font10Imp glyph order at 00613a6c, including localized slots.
constexpr std::array<unsigned char, 136> kFontAlphabet{{
    0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50,
    0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
    0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76,
    0x77, 0x78, 0x79, 0x7a, 0x2e, 0x2c, 0x2f, 0x25, 0x24, 0x21, 0x40, 0x23, 0x5b, 0x5d, 0x2b, 0x3d,
    0x3f, 0x3b, 0x3a, 0x27, 0x27, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0xc4,
    0xcb, 0xd6, 0xdc, 0xdf, 0xe4, 0xeb, 0xf6, 0xfc, 0xd4, 0xd3, 0xd2, 0xca, 0xc9, 0xc8, 0xc2, 0xc1,
    0xc0, 0xdb, 0xda, 0xd9, 0xce, 0xcd, 0xcc, 0xdd, 0xc7, 0xc3, 0xd5, 0x9c, 0xd1, 0x9c, 0x9c, 0xf4,
    0xf3, 0xf2, 0xea, 0xe9, 0xe8, 0xe2, 0xe1, 0xe0, 0xfb, 0xfa, 0xf9, 0xee, 0xed, 0xec, 0xfd, 0xe7,
    0xe3, 0xf5, 0x9c, 0xf1, 0x9c, 0x9c, 0x2d, 0xbf
}};
constexpr std::array<HudRect, 8> kTileDestinations{{
    {0, 224, 256, 256}, {256, 224, 256, 256}, {512, 224, 128, 128},
    {0, 0, 256, 224}, {256, 0, 256, 224}, {512, 0, 128, 95},
    {512, 352, 128, 128}, {512, 95, 128, 223}}};
constexpr std::array<HudRect, 8> kTileSources{{
    {0, 0, 256, 256}, {0, 0, 256, 256}, {0, 0, 128, 128},
    {0, 33, 256, 223}, {0, 33, 256, 223}, {0, 33, 128, 95},
    {0, 0, 128, 128}, {0, 0, 128, 223}}};
}
CockpitFrontend::~CockpitFrontend() { stop(); }
void CockpitFrontend::stop() {
    for (auto texture : textures_)
        renderer_.destroy_texture(texture);
    textures_.clear();
    cars_.clear();
    map_ = {};
    font_texture_ = {};
    font_ = {};
    track_name_.clear();
}
Result CockpitFrontend::load_sprite(ResourceStore& resources, const std::string& path,
                                    Sprite& sprite, Bitmap* raw) {
    std::vector<std::byte> bytes;
    Bitmap bitmap;
    auto result = resources.read(path, bytes);
    if (result != Result::ok)
        return result;
    if ((result = decode_bitmap(bytes, bitmap)) != Result::ok)
        return result;
    if (raw)
        *raw = bitmap;
    TextureTransparency transparency;
    if ((result = prepare_original_texture(path, bitmap, transparency)) != Result::ok)
        return result;
    sprite = {renderer_.upload_texture(bitmap.width, bitmap.height, bitmap.rgba),
              static_cast<int>(bitmap.width), static_cast<int>(bitmap.height)};
    if (!sprite.texture.value)
        return Result::unavailable;
    textures_.push_back(sprite.texture);
    return Result::ok;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0042f549
Result CockpitFrontend::load(ResourceStore& resources, std::string_view track,
                             std::span<const std::string> cars, std::string& error) {
    stop();
    auto fail = [&](Result result, const std::string& resource) {
        error = "Cockpit resource unavailable or malformed: " + resource;
        stop();
        return result;
    };
    if (cars.empty() || cars.size() > 12)
        return fail(Result::invalid_argument, "participant count");
    for (const auto& name : cars) {
        Car car;
        const std::string prefix = "Cars/" + name + "/Cockpit/";
        Configuration config;
        if (config.load(resources, prefix + "Data.ini") != Result::ok)
            return fail(Result::invalid_argument, prefix + "Data.ini");
        const auto number = [&](std::string_view section, std::string_view key, float& value) {
            return config.number(section, key, value) == Result::ok && std::abs(value) < 100000;
        };
        const auto integer = [&](std::string_view section, std::string_view key, int& value) {
            float raw;
            if (!number(section, key, raw) || raw != std::trunc(raw))
                return false;
            value = static_cast<int>(raw);
            return true;
        };
        if (!integer("WHEEL", "SIZE", car.wheel_size) || car.wheel_size <= 0 || car.wheel_size > 4096 ||
            !integer("WHEEL", "CENTERX", car.wheel_x) ||
            !integer("WHEEL", "CENTERY", car.wheel_y) ||
            !number("TACKOMETER", "MAXRPM", car.max_rpm) || car.max_rpm <= 0 ||
            !number("TACKOMETER", "BASEROTATION", car.tach_base) ||
            !number("TACKOMETER", "ROTATIONDEGREES", car.tach_range) ||
            !integer("TACKOMETER", "X", car.tach_x) ||
            !integer("TACKOMETER", "Y", car.tach_y) ||
            !integer("TACKOMETER", "PIVOTOFFSETX", car.pivot_x) ||
            !integer("TACKOMETER", "PIVOTOFFSETY", car.pivot_y) ||
            !number("SPEEDOMETER", "MAXMPH", car.max_mph) || car.max_mph <= 0 ||
            !number("SPEEDOMETER", "BASEROTATION", car.speed_base) ||
            !number("SPEEDOMETER", "ROTATIONDEGREES", car.speed_range) ||
            !integer("SPEEDOMETER", "X", car.speed_x) ||
            !integer("SPEEDOMETER", "Y", car.speed_y) ||
            !integer("MAP", "MAPOFFSETX", car.map_x) ||
            !integer("MAP", "MAPOFFSETY", car.map_y))
            return fail(Result::invalid_argument, prefix + "Data.ini cockpit fields");
        for (std::size_t slot = 0; slot < 3; ++slot) {
            const auto key = "WEAPON" + std::to_string(slot + 1);
            if (!integer("WEAPONDISPLAY", key + "AMMOX", car.ammo_x[slot]) ||
                !integer("WEAPONDISPLAY", key + "AMMOY", car.ammo_y[slot]) ||
                !integer("WEAPONDISPLAY", key + "X", car.weapon_x[slot]) ||
                !integer("WEAPONDISPLAY", key + "Y", car.weapon_y[slot]))
                return fail(Result::invalid_argument, prefix + "Data.ini weapon display fields");
        }
        std::array<Bitmap, 8> raw;
        for (std::size_t tile = 0; tile < 8; ++tile) {
            const auto path = prefix + "16Cockpit" + std::to_string(tile + 1) + ".bmp";
            const auto result = load_sprite(resources, path, car.tiles[tile], &raw[tile]);
            if (result != Result::ok)
                return fail(result, path);
        }
        for (const auto& [path, sprite] :
             std::array<std::pair<const char*, Sprite*>, 2>{{
                 {"16Steer.bmp", &car.steering}, {"16TackOMeterNeedle.bmp", &car.needle}}}) {
            const auto result = load_sprite(resources, prefix + path, *sprite);
            if (result != Result::ok)
                return fail(result, prefix + path);
        }
        // Modern extension: sample only the outermost static interior column, one color
        // per row. This never repeats an instrument, glyph or steering-wheel texture.
        for (int side = 0; side < 2; ++side) {
            Bitmap edge;
            edge.width = 1;
            edge.height = 480;
            edge.rgba.assign(480 * 4, std::byte{});
            for (std::size_t tile = 0; tile < 8; ++tile) {
                const auto& dst = kTileDestinations[tile];
                if ((side == 0 && dst.x != 0) || (side == 1 && dst.x + dst.width != 640))
                    continue;
                const auto& src = kTileSources[tile];
                const auto& image = raw[tile];
                for (int row = 0; row < dst.height; ++row) {
                    const int sy = std::clamp(src.y + row * src.height / dst.height, 0,
                                              static_cast<int>(image.height) - 1);
                    const int sx = side ? static_cast<int>(image.width) - 1 : 0;
                    const auto from = (static_cast<std::size_t>(sy) * image.width + sx) * 4;
                    const auto to = static_cast<std::size_t>(479 - (dst.y + row)) * 4;
                    bool ink = false;
                    std::array<std::byte, 3> color;
                    for (int channel = 0; channel < 3; ++channel) {
                        const unsigned value = std::to_integer<unsigned>(image.rgba[from + channel]);
                        color[channel] = std::byte((value >> 3) * 255 / 31);
                        ink |= value >= 8;
                    }
                    // Transparent overlapping tiles must retain the underlying interior,
                    // exactly as alpha compositing does for the central cockpit.
                    if (ink) {
                        std::copy(color.begin(), color.end(), edge.rgba.begin() + to);
                        edge.rgba[to + 3] = std::byte{255};
                    }
                }
            }
            Sprite sprite{renderer_.upload_texture(edge.width, edge.height, edge.rgba), 1, 480};
            if (!sprite.texture.value)
                return fail(Result::unavailable, prefix + "widescreen edge upload");
            textures_.push_back(sprite.texture);
            (side ? car.right_edge : car.left_edge) = sprite;
        }
        cars_.push_back(std::move(car));
    }
    const std::string map_path = "Tracks/" + std::string(track) + "/Map/16CockpitMap.bmp";
    const auto result = load_sprite(resources, map_path, map_);
    if (result != Result::ok)
        return fail(result, map_path);
    Bitmap font_bitmap;
    if (load_sprite(resources, "Fonts/16Font10Imp.bmp", font_texture_, &font_bitmap) != Result::ok)
        return fail(Result::invalid_argument, "Fonts/16Font10Imp.bmp");
    if (font_.decode(font_bitmap, kFontAlphabet) != Result::ok)
        return fail(Result::invalid_argument, "Fonts/16Font10Imp.bmp glyph layout");
    track_name_ = ResourceStore::canonical(track);
    return Result::ok;
}
void CockpitFrontend::quad(std::vector<HudQuad>& output, const Sprite& sprite, HudRect dst,
                           HudRect src, float degrees, std::array<float, 2> pivot) {
    if (!src.width || !src.height)
        src = {0, 0, sprite.width, sprite.height};
    HudQuad q;
    q.texture = sprite.texture;
    q.preserve_aspect = true;
    const float x = static_cast<float>(dst.x), y = static_cast<float>(dst.y);
    q.positions = {{{x, y}, {x + dst.width, y}, {x + dst.width, y + dst.height}, {x, y + dst.height}}};
    const float u0 = static_cast<float>(src.x) / sprite.width;
    const float u1 = static_cast<float>(src.x + src.width) / sprite.width;
    const float v0 = 1 - static_cast<float>(src.y) / sprite.height;
    const float v1 = 1 - static_cast<float>(src.y + src.height) / sprite.height;
    q.uv = {{{u0, v0}, {u1, v0}, {u1, v1}, {u0, v1}}};
    for (auto& color : q.colors)
        color = {1, 1, 1, 1};
    if (degrees != 0) {
        const float angle = degrees * std::numbers::pi_v<float> / 180;
        for (auto& position : q.positions) {
            const float dx = position[0] - pivot[0], dy = position[1] - pivot[1];
            position = {pivot[0] + dx * std::cos(angle) - dy * std::sin(angle),
                        pivot[1] + dx * std::sin(angle) + dy * std::cos(angle)};
        }
    }
    output.push_back(q);
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004301f5
Result CockpitFrontend::draw(std::size_t index, float steering_fraction, float rpm, float speed,
                             int gear, bool reverse, float aspect, std::vector<HudQuad>& output,
                             bool show_map) const {
    (void)gear;
    (void)reverse;
    if (index >= cars_.size() || !std::isfinite(steering_fraction) || !std::isfinite(rpm) ||
        !std::isfinite(speed) || !std::isfinite(aspect) || aspect <= 0 || aspect > 32 ||
        std::abs(steering_fraction) > 1000 || std::abs(rpm) > 1000000 || std::abs(speed) > 1000000)
        return Result::invalid_argument;
    const auto& car = cars_[index];
    const int extension = static_cast<int>(std::ceil(std::max(0.f, aspect * 480 - 640) / 2));
    if (extension) {
        quad(output, car.left_edge, {-extension, 0, extension, 480});
        quad(output, car.right_edge, {640, 0, extension, 480});
    }
    for (std::size_t tile = 0; tile < 8; ++tile)
        quad(output, car.tiles[tile], kTileDestinations[tile], kTileSources[tile]);
    const auto draw_needle = [&](int x, int y, float degrees) {
        quad(output, car.needle, {x, y, car.needle.width, car.needle.height}, {}, degrees,
             {static_cast<float>(x + car.pivot_x), static_cast<float>(y + car.pivot_y)});
    };
    draw_needle(car.tach_x, car.tach_y, rpm / car.max_rpm * car.tach_range + car.tach_base);
    draw_needle(car.speed_x, car.speed_y,
                speed * 2.236997365951538f / car.max_mph * car.speed_range + car.speed_base);
    if (show_map)
        quad(output, map_, {car.map_x, car.map_y, map_.width, map_.height});
    quad(output, car.steering,
         {car.wheel_x - car.wheel_size / 2, car.wheel_y - car.wheel_size / 2,
          car.wheel_size, car.wheel_size}, {}, steering_fraction * 75,
         {static_cast<float>(car.wheel_x), static_cast<float>(car.wheel_y)});
    return Result::ok;
}
Result CockpitFrontend::text(std::vector<HudQuad>& output, int x, int y, std::string_view value,
                             bool center_x) const {
    int width = 0;
    for (unsigned char c : value) {
        if (c != ' ' && !font_.glyph(c).width)
            return Result::unavailable;
        width += (c == ' ' ? font_.space_width() : font_.glyph(c).width) + 1;
    }
    if (center_x)
        x -= std::max(0, width - 1) / 2;
    y -= font_.height() / 2;
    for (unsigned char c : value) {
        if (c == ' ') {
            x += font_.space_width() + 1;
            continue;
        }
        const auto& glyph = font_.glyph(c);
        quad(output, font_texture_, {x, y, glyph.width, glyph.height},
             {glyph.x, glyph.y, glyph.width, glyph.height});
        for (auto& color : output.back().colors)
            color = {90.f / 255, 182.f / 255, 1, 1};
        x += glyph.width + 1;
    }
    return Result::ok;
}
Result CockpitFrontend::draw_readouts(std::size_t index, const std::array<std::string, 3>& names,
                                      const std::array<int, 3>& ammunition,
                                      const std::array<bool, 3>& present,
                                      std::vector<HudQuad>& output) const {
    if (index >= cars_.size())
        return Result::invalid_argument;
    const auto& car = cars_[index];
    // 004301f5 uses tile6-left and tile4-top+17 for its track name.
    if (text(output, 512, 17, track_name_, false) != Result::ok)
        return Result::unavailable;
    for (std::size_t slot = 0; slot < 3; ++slot) {
        if (!present[slot])
            continue;
        if (ammunition[slot] < 0 || names[slot].size() > 256)
            return Result::invalid_argument;
        if (text(output, car.ammo_x[slot], car.ammo_y[slot], std::to_string(ammunition[slot]),
                 true) != Result::ok ||
            text(output, car.weapon_x[slot], car.weapon_y[slot], names[slot], false) != Result::ok)
            return Result::unavailable;
    }
    return Result::ok;
}
std::array<int, 2> CockpitFrontend::map_offset(std::size_t index) const {
    return index < cars_.size() ? std::array<int, 2>{cars_[index].map_x, cars_[index].map_y}
                                : std::array<int, 2>{};
}
} // namespace roadwars
