#include "game_src/unresolved/mesh_scene.hpp"
#include "game_src/unresolved/binary_reader.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>

namespace roadwars {
namespace {
Vec3 vector(BinaryReader& stream) { return {stream.f32(), stream.f32(), stream.f32()}; }
MeshObject read_object(BinaryReader& stream, unsigned depth, std::size_t& vertices) {
    if (depth > 128)
        throw std::runtime_error("MSH hierarchy depth exceeded");
    MeshObject object;
    object.name = stream.text(32);
    std::from_chars(object.name.data(), object.name.data() + object.name.size(), object.flags);
    object.type = stream.u32();
    const auto children = stream.u32();
    if (children > 100000)
        throw std::runtime_error("Invalid MSH child count");
    if (object.type == 4) {
        // Type-4 dummy stores three basis vectors followed by a position.
        stream.take(12);
        const auto forward = vector(stream);
        stream.take(12);
        object.position = vector(stream);
        const auto length =
            std::sqrt(forward.x * forward.x + forward.y * forward.y + forward.z * forward.z);
        if (length == 0)
            throw std::runtime_error("Zero type-4 dummy heading vector");
        // 004d9c05: compare normalized second basis against global +Z (004dab4a).
        object.dummy_heading_degrees =
            std::acos(std::clamp(forward.z / length, -1.0f, 1.0f)) * 57.288352966308594f;
        if (forward.x < 0)
            object.dummy_heading_degrees = 360.0f - object.dummy_heading_degrees;
        if (object.dummy_heading_degrees >= 360.0f)
            object.dummy_heading_degrees -= 360.0f;
    } else if (object.type == 1) {
        object.position = vector(stream);
        const auto parts = stream.u32();
        if (parts > 100000)
            throw std::runtime_error("Invalid MSH part count");
        const bool lit =
            ((object.flags & 0x200) == 0 || (object.flags & 0x40) != 0) && (object.flags & 4) == 0;
        for (std::uint32_t part_index = 0; part_index < parts; ++part_index) {
            MeshPart part;
            const auto count = stream.u32();
            if (count > 65536 || vertices + count > 10000000)
                throw std::runtime_error("MSH vertex limit exceeded");
            vertices += count;
            for (std::uint32_t index = 0; index < count; ++index) {
                part.positions.push_back(vector(stream));
                if (lit)
                    part.normals.push_back(vector(stream));
                const auto color = stream.u32();
                part.colors.push_back({((color >> 16) & 255) / 255.0f,
                    ((color >> 8) & 255) / 255.0f, (color & 255) / 255.0f,
                    ((color >> 24) & 255) / 255.0f});
                part.uv.push_back({stream.f32(), stream.f32()});
                stream.take(8);
            }
            const auto triangles = stream.u32();
            if (triangles > stream.remaining() / 6)
                throw std::runtime_error("Invalid MSH triangle count");
            for (std::uint64_t index = 0; index < static_cast<std::uint64_t>(triangles) * 3;
                 ++index) {
                const auto vertex = stream.u16();
                if (vertex >= count)
                    throw std::runtime_error("MSH index outside vertex array");
                part.indices.push_back(vertex);
            }
            part.texture = stream.text(32);
            for (auto& channel : part.color)
                channel = stream.f32();
            object.parts.push_back(std::move(part));
        }
        if (!lit) object.flags |= 4;
    } else if (object.type != 0) {
        throw std::runtime_error("Unsupported MSH object type " + std::to_string(object.type));
    }
    for (std::uint32_t index = 0; index < children; ++index)
        object.children.push_back(read_object(stream, depth + 1, vertices));
    return object;
}
} // namespace
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004d9aca
// Portable serialized-object reader; rendering allocation and bounding caches are separate.
Result decode_msh(std::span<const std::byte> bytes, MeshObject& output, std::string& error) {
    try {
        BinaryReader stream(bytes);
        std::size_t vertices{};
        auto scene = read_object(stream, 0, vertices);
        if (stream.remaining() != 0)
            throw std::runtime_error("Trailing MSH bytes: " + std::to_string(stream.remaining()));
        output = std::move(scene);
        error.clear();
        return Result::ok;
    } catch (const std::exception& exception) {
        error = exception.what();
        return Result::invalid_argument;
    }
}
} // namespace roadwars
