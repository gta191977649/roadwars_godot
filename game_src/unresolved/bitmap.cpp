#include "game_src/unresolved/bitmap.hpp"
#include "game_src/unresolved/binary_reader.hpp"
#include <algorithm>
#include <limits>

namespace roadwars {
Result decode_bitmap(std::span<const std::byte> bytes, Bitmap& bitmap) {
    try {
        BinaryReader stream(bytes);
        if (stream.u16() != 0x4d42)
            return Result::invalid_argument;
        const auto file_size = stream.u32();
        stream.take(4);
        const auto pixels = stream.u32();
        const auto header = stream.u32();
        if (file_size > bytes.size() || header < 40 || header > bytes.size() - 14)
            return Result::invalid_argument;
        const auto width = stream.u32();
        const auto signed_height = std::bit_cast<std::int32_t>(stream.u32());
        if (width == 0 || width > 8192 || signed_height == 0 ||
            signed_height == std::numeric_limits<std::int32_t>::min())
            return Result::invalid_argument;
        const auto height =
            static_cast<std::uint32_t>(signed_height < 0 ? -signed_height : signed_height);
        if (height > 8192 || stream.u16() != 1)
            return Result::invalid_argument;
        const auto bits = stream.u16();
        const auto compression = stream.u32();
        if (compression != 0 || (bits != 8 && bits != 24 && bits != 32))
            return Result::unavailable;
        stream.take(12);
        const auto colors = stream.u32();
        stream.take(4);
        stream.take(header - 40);
        std::vector<std::array<std::byte, 4>> palette;
        if (bits == 8) {
            const auto count = colors ? colors : 256;
            if (count > 256)
                return Result::invalid_argument;
            for (std::uint32_t index = 0; index < count; ++index) {
                auto color = stream.take(4);
                palette.push_back({color[2], color[1], color[0], std::byte{255}});
            }
        }
        const auto stride = ((static_cast<std::size_t>(width) * bits + 31) / 32) * 4;
        if (pixels < stream.offset() || pixels > bytes.size() ||
            height > (bytes.size() - pixels) / stride)
            return Result::invalid_argument;
        Bitmap result{
            width, height, std::vector<std::byte>(static_cast<std::size_t>(width) * height * 4),
            0,     false,  {}};
        result.source_bits = bits;
        result.source_bottom_up = signed_height > 0;
        if (bits == 8)
            result.indices.resize(static_cast<std::size_t>(width) * height);
        for (std::uint32_t y = 0; y < height; ++y) {
            const auto source_y = signed_height > 0 ? height - 1 - y : y;
            const auto row = bytes.subspan(pixels + source_y * stride, stride);
            for (std::uint32_t x = 0; x < width; ++x) {
                auto* destination =
                    result.rgba.data() + (static_cast<std::size_t>(y) * width + x) * 4;
                if (bits == 8) {
                    const auto index = std::to_integer<unsigned>(row[x]);
                    if (index >= palette.size())
                        return Result::invalid_argument;
                    std::copy(palette[index].begin(), palette[index].end(), destination);
                    result.indices[static_cast<std::size_t>(y) * width + x] = row[x];
                } else {
                    const auto pixel =
                        row.subspan(static_cast<std::size_t>(x) * bits / 8, bits / 8);
                    destination[0] = pixel[2];
                    destination[1] = pixel[1];
                    destination[2] = pixel[0];
                    destination[3] = std::byte{255};
                }
            }
        }
        bitmap = std::move(result);
        return Result::ok;
    } catch (const std::exception&) {
        return Result::invalid_argument;
    }
}

namespace {
std::string upper_basename(std::string_view path) {
    const auto separator = path.find_last_of("/\\");
    std::string name(path.substr(separator == std::string_view::npos ? 0 : separator + 1));
    for (auto& character : name)
        if (character >= 'a' && character <= 'z')
            character -= 'a' - 'A';
    return name;
}
} // namespace

// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004ecbb6
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004e9ef2
std::string original_alpha_path(std::string_view path) {
    if (!upper_basename(path).starts_with("XX"))
        return {};
    const auto dot = path.find_last_of('.');
    if (dot == std::string_view::npos)
        return {};
    return std::string(path.substr(0, dot)) + "Alpha.bmp";
}

// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004e8dea
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004e9389
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004e9939
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004e9ef2
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004ecb48
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004ecc3f
Result prepare_original_texture(std::string_view path, Bitmap& bitmap,
                                TextureTransparency& transparency, const Bitmap* alpha) {
    const auto name = upper_basename(path);
    const bool blended = name.starts_with("XX");
    const bool explicit_1555 = name.starts_with("16");
    const bool keyed = !blended && (name.starts_with("X") || bitmap.source_bits == 24);
    const auto count = static_cast<std::size_t>(bitmap.width) * bitmap.height;
    if (!bitmap.width || !bitmap.height || bitmap.rgba.size() != count * 4)
        return Result::invalid_argument;
    if (!bitmap.source_bottom_up || (bitmap.source_bits != 8 && bitmap.source_bits != 24))
        return Result::unavailable;
    if (bitmap.source_bits == 8 && bitmap.indices.size() != count)
        return Result::invalid_argument;
    if (blended && (!alpha || alpha->source_bits != 8 || !alpha->source_bottom_up ||
                    alpha->width != bitmap.width || alpha->height != bitmap.height ||
                    alpha->indices.size() != count))
        return Result::invalid_argument;
    Bitmap result = bitmap;
    for (std::size_t pixel = 0; pixel < count; ++pixel) {
        auto* color = result.rgba.data() + pixel * 4;
        if (blended) {
            for (std::size_t channel = 0; channel < 3; ++channel)
                color[channel] = std::byte((std::to_integer<unsigned>(color[channel]) >> 4) * 17);
            color[3] = std::byte((std::to_integer<unsigned>(alpha->indices[pixel]) >> 4) * 17);
        } else if (explicit_1555 || bitmap.source_bits == 24) {
            bool black = true;
            for (std::size_t channel = 0; channel < 3; ++channel) {
                const unsigned quantized = std::to_integer<unsigned>(color[channel]) >> 3;
                black = black && quantized == 0;
                color[channel] = std::byte(quantized * 255 / 31);
            }
            color[3] = black ? std::byte{0} : std::byte{255};
        } else {
            // Palettized DirectDraw surfaces key index zero, including duplicate black entries.
            color[3] =
                keyed && result.indices[pixel] == std::byte{0} ? std::byte{0} : std::byte{255};
        }
    }
    // The original loaders copy on-disk bottom-up rows directly to surface rows.
    // Keep the original mesh UVs unchanged and reproduce that texture orientation.
    const auto stride = static_cast<std::size_t>(result.width) * 4;
    for (std::size_t y = 0; y < result.height / 2; ++y) {
        const auto other = result.height - 1 - y;
        std::swap_ranges(result.rgba.begin() + y * stride, result.rgba.begin() + (y + 1) * stride,
                         result.rgba.begin() + other * stride);
        if (!result.indices.empty())
            std::swap_ranges(result.indices.begin() + y * result.width,
                             result.indices.begin() + (y + 1) * result.width,
                             result.indices.begin() + other * result.width);
    }
    transparency = blended                  ? TextureTransparency::alpha_blend
                   : keyed || explicit_1555 ? TextureTransparency::color_key
                                            : TextureTransparency::opaque;
    bitmap = std::move(result);
    return Result::ok;
}
} // namespace roadwars
