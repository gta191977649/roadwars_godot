#pragma once
#include "game_src/interfaces/host.hpp"
#include <string>

namespace roadwars {
struct Bitmap {
    std::uint32_t width{}, height{};
    std::vector<std::byte> rgba;
    std::uint16_t source_bits{};
    bool source_bottom_up{};
    std::vector<std::byte> indices;
};
Result decode_bitmap(std::span<const std::byte> bytes, Bitmap& bitmap);
enum class TextureTransparency { opaque, color_key, alpha_blend };
std::string original_alpha_path(std::string_view path);
// Decode generic BMP first. This adapts rows/colors/alpha to the original texture surface.
// alpha is required only for XX-prefixed textures and must retain indexed pixel bytes.
Result prepare_original_texture(std::string_view path, Bitmap& bitmap,
                                TextureTransparency& transparency, const Bitmap* alpha = nullptr);
} // namespace roadwars
