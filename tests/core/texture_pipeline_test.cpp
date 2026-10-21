#include "game_src/unresolved/bitmap.hpp"
#include <stdexcept>
using namespace roadwars;
namespace {
void require(bool condition) {
    if (!condition)
        throw std::runtime_error("Original texture pipeline contract failed");
}
void u32(std::vector<std::byte>& bytes, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8)
        bytes.push_back(std::byte((value >> shift) & 255));
}
Bitmap indexed(std::vector<std::byte> colors, std::vector<std::byte> indices) {
    Bitmap result{2, 2, std::move(colors), 8, true, std::move(indices)};
    return result;
}
} // namespace
int main() {
    require(original_alpha_path("Tracks\\World\\xxGlass.bmp") == "Tracks\\World\\xxGlassAlpha.bmp");
    require(original_alpha_path("Tracks/World/XTree.bmp").empty());
    // A valid padded two-row BMP. Generic decoding is top-down; original surfaces are file-order.
    std::vector<std::byte> bmp{std::byte{'B'}, std::byte{'M'}};
    u32(bmp, 70);
    u32(bmp, 0);
    u32(bmp, 62);
    u32(bmp, 40);
    u32(bmp, 2);
    u32(bmp, 2);
    u32(bmp, 0x80001);
    u32(bmp, 0);
    u32(bmp, 8);
    u32(bmp, 0);
    u32(bmp, 0);
    u32(bmp, 2);
    u32(bmp, 0);
    bmp.insert(bmp.end(), {std::byte{0}, std::byte{0}, std::byte{255}, std::byte{0}, std::byte{0},
                           std::byte{255}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{1},
                           std::byte{0}, std::byte{0}, std::byte{1}, std::byte{0}, std::byte{0},
                           std::byte{0}});
    Bitmap image;
    require(decode_bitmap(bmp, image) == Result::ok);
    require(image.rgba[0] == std::byte{0} && image.rgba[1] == std::byte{255});
    TextureTransparency mode;
    require(prepare_original_texture("World/XTree.bmp", image, mode) == Result::ok);
    require(mode == TextureTransparency::color_key);
    require(image.rgba[0] == std::byte{255} && image.rgba[1] == std::byte{0});
    require(image.rgba[3] == std::byte{0} && image.rgba[7] == std::byte{255});
    // The palettized key is index zero, not a black-RGB threshold.
    image = indexed(std::vector<std::byte>(16, std::byte{0}),
                    {std::byte{0}, std::byte{1}, std::byte{1}, std::byte{0}});
    require(prepare_original_texture("XTree.bmp", image, mode) == Result::ok);
    require(image.rgba[3] == std::byte{255} && image.rgba[7] == std::byte{0});
    image = indexed(std::vector<std::byte>(16, std::byte{0}), std::vector<std::byte>(4));
    require(prepare_original_texture("Opaque.bmp", image, mode) == Result::ok);
    require(mode == TextureTransparency::opaque && image.rgba[3] == std::byte{255});
    // ARGB4444 alpha takes the raw alpha-map pixel index, independently of its palette RGB.
    image = indexed(std::vector<std::byte>(16, std::byte{255}), std::vector<std::byte>(4));
    Bitmap alpha = indexed(std::vector<std::byte>(16, std::byte{0}),
                           {std::byte{0}, std::byte{128}, std::byte{255}, std::byte{31}});
    require(prepare_original_texture("XXGlass.bmp", image, mode, &alpha) == Result::ok);
    require(mode == TextureTransparency::alpha_blend);
    require(image.rgba[3] == std::byte{255} && image.rgba[7] == std::byte{17});
    require(image.rgba[11] == std::byte{0} && image.rgba[15] == std::byte{136});
    Bitmap absent = image;
    require(prepare_original_texture("XXGlass.bmp", absent, mode) == Result::invalid_argument);
    require(absent.rgba == image.rgba);
    image = {1, 1, {std::byte{7}, std::byte{7}, std::byte{7}, std::byte{255}}, 24, true};
    require(prepare_original_texture("16Sky.bmp", image, mode) == Result::ok);
    require(image.rgba[0] == std::byte{0} && image.rgba[3] == std::byte{0});
    image = {1, 1, {std::byte{8}, std::byte{255}, std::byte{127}, std::byte{255}}, 24, true};
    require(prepare_original_texture("16Sky.bmp", image, mode) == Result::ok);
    require(image.rgba[0] == std::byte{8} && image.rgba[1] == std::byte{255} &&
            image.rgba[2] == std::byte{123} && image.rgba[3] == std::byte{255});
}
