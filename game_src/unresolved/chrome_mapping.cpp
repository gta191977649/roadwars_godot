#include "game_src/unresolved/chrome_mapping.hpp"
#include "game_src/unresolved/bitmap.hpp"
#include <algorithm>
#include <cmath>

namespace roadwars {
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004d314d
bool chrome_mapping_applies(bool enabled, std::uint32_t flags, std::size_t part_index,
                            bool texture_bound) {
    return enabled && (flags & 0x400) != 0 && part_index == 0 && texture_bound;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004d5425
std::array<float, 2> chrome_mapping_uv(Vec3 n, const Transform& m) {
    const auto x = n.z * m.basis[2].x + n.y * m.basis[1].x + n.x * m.basis[0].x;
    const auto y = n.z * m.basis[2].y + n.y * m.basis[1].y + n.x * m.basis[0].y;
    return {(x + 1.f) * .5f, (1.f - y) * .5f};
}
std::array<float, 3> chrome_mapping_add(std::array<float, 3> base,
                                      std::array<float, 3> chrome) {
    for (std::size_t i = 0; i < base.size(); ++i)
        base[i] = std::clamp(base[i] + chrome[i], 0.f, 1.f);
    return base;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004b7439
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00438393
Result load_chrome_mapping(ResourceStore& resources, Renderer& renderer, TextureHandle& texture,
                            bool& enabled, std::string& error) {
    texture = {};
    enabled = false;
    Configuration config;
    float value{};
    if (config.load(resources, "Ini/RoadWars.ini") != Result::ok ||
        config.number("ROADWARS", "ENVIRONMENT_MAPPING", value) != Result::ok ||
        !std::isfinite(value)) {
        error = "Original ENVIRONMENT_MAPPING configuration missing or malformed";
        return Result::invalid_argument;
    }
    const auto options = renderer.chrome_mapping_options();
    enabled = options.enabled.value_or(value != 0);
    if (!enabled)
        return Result::ok;
    std::vector<std::byte> bytes;
    Bitmap bitmap;
    const auto result = resources.read(options.texture_path, bytes);
    if (result != Result::ok) {
        error = "Chrome texture unavailable: " + options.texture_path;
        return result;
    }
    if (decode_bitmap(bytes, bitmap) != Result::ok) {
        error = "Original chrome texture is malformed";
        return Result::invalid_argument;
    }
    TextureTransparency transparency;
    if (prepare_original_texture(options.texture_path, bitmap, transparency) != Result::ok) {
        error = "Original chrome texture conversion failed";
        return Result::invalid_argument;
    }
    texture = renderer.upload_texture(bitmap.width, bitmap.height, bitmap.rgba);
    if (!texture.value) {
        error = "Original chrome texture upload failed";
        return Result::unavailable;
    }
    return Result::ok;
}
} // namespace roadwars
