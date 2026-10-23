#include "game_src/unresolved/chrome_mapping.hpp"
#include "game_src/unresolved/bitmap.hpp"
#include "game_src/unresolved/mesh_scene.hpp"
#include "platform/godot/rendering/coordinate_conversion.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <source_location>
#include <stdexcept>

using namespace roadwars;
namespace {
void require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition)
        throw std::runtime_error("Chrome mapping contract failed at line " +
                                 std::to_string(location.line()));
}
void check_uv(Vec3 normal, const Transform& transform, float u, float v) {
    const auto actual = chrome_mapping_uv(normal, transform);
    require(std::abs(actual[0] - u) < .00001f && std::abs(actual[1] - v) < .00001f);
}
float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3 transform_normal(const Transform& matrix, Vec3 normal) {
    return {matrix.basis[0].x * normal.x + matrix.basis[1].x * normal.y +
                matrix.basis[2].x * normal.z,
            matrix.basis[0].y * normal.x + matrix.basis[1].y * normal.y +
                matrix.basis[2].y * normal.z,
            matrix.basis[0].z * normal.x + matrix.basis[1].z * normal.y +
                matrix.basis[2].z * normal.z};
}
void check_coordinate_conversion() {
    // Camera right has the original camera owner's opposite screen-right convention.
    Transform camera{{{{-.8f, 0, .6f}, {0, 1, 0}, {-.6f, 0, -.8f}}}, {17, 3, -9}};
    Transform object{{{{0, 2, 0}, {-3, 0, 0}, {0, 0, 4}}}, {-8, 1, 25}};
    Transform object_to_view;
    for (std::size_t i = 0; i < 3; ++i) {
        const auto column = object.basis[i];
        object_to_view.basis[i] = {-dot(camera.basis[0], column),
                                   dot(camera.basis[1], column),
                                   dot(camera.basis[2], column)};
    }
    const auto native_camera = original_camera_to_godot(camera);
    const auto native_object = original_object_to_godot(object);
    for (auto normal : {Vec3{1, 0, 0}, Vec3{0, 1, 0}, Vec3{0, 0, 1}, Vec3{2, -1, .5f}}) {
        const auto native_world =
            transform_normal(native_object, original_to_godot(normal));
        const auto native_x = dot(native_camera.basis[0], native_world);
        const auto native_y = dot(native_camera.basis[1], native_world);
        check_uv(normal, object_to_view, (native_x + 1) * .5f, (1 - native_y) * .5f);
    }
}
class MemoryReader final : public ResourceReader {
  public:
    std::map<std::string, std::vector<std::byte>> files;
    void text(std::string_view path, std::string_view value) {
        auto& bytes = files[ResourceStore::canonical(path)];
        bytes.clear();
        for (unsigned char c : value)
            bytes.push_back(std::byte{c});
    }
    Result read(std::string_view path, std::vector<std::byte>& output) override {
        const auto found = files.find(ResourceStore::canonical(path));
        if (found == files.end())
            return Result::not_found;
        output = found->second;
        return Result::ok;
    }
};
class Files final : public ResourceReader {
  public:
    explicit Files(std::filesystem::path root) : root_(std::move(root)) {}
    Result read(std::string_view path, std::vector<std::byte>& output) override {
        std::ifstream file(root_ / std::filesystem::path(path), std::ios::binary | std::ios::ate);
        if (!file)
            return Result::not_found;
        const auto size = file.tellg();
        if (size < 0)
            return Result::invalid_argument;
        output.resize(static_cast<std::size_t>(size));
        file.seekg(0);
        file.read(reinterpret_cast<char*>(output.data()), size);
        return file ? Result::ok : Result::invalid_argument;
    }

  private:
    std::filesystem::path root_;
};
class RecordingRenderer final : public Renderer {
  public:
    ChromeMappingOptions options;
    ChromeMappingOptions chrome_mapping_options() const override { return options; }
    std::uint32_t width{}, height{}, uploads{};
    std::vector<std::byte> pixels;
    std::set<std::uint64_t> live;
    bool fail_upload{};
    MeshHandle upload_mesh(std::span<const Vec3>, std::span<const std::uint32_t>) override {
        return {};
    }
    InstanceHandle create_instance(MeshHandle) override { return {}; }
    Result update_instance(InstanceHandle, const Transform&) override { return Result::ok; }
    void destroy_instance(InstanceHandle) override {}
    void destroy_mesh(MeshHandle) override {}
    void update_camera(const Transform&, float) override {}
    TextureHandle upload_texture(std::uint32_t w, std::uint32_t h,
                                 std::span<const std::byte> bytes) override {
        ++uploads;
        width = w;
        height = h;
        pixels.assign(bytes.begin(), bytes.end());
        if (fail_upload)
            return {};
        live.insert(uploads);
        return {uploads};
    }
    void destroy_texture(TextureHandle texture) override {
        require(live.erase(texture.value) == 1);
    }
};
void u32(std::vector<std::byte>& bytes, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8)
        bytes.push_back(std::byte((value >> shift) & 255));
}
std::vector<std::byte> fixture_bitmap() {
    std::vector<std::byte> bytes{std::byte{'B'}, std::byte{'M'}};
    for (auto value : {70u, 0u, 54u, 40u, 2u, 2u, 0x180001u, 0u, 16u, 0u, 0u, 0u, 0u})
        u32(bytes, value);
    // File rows: red/green, then blue/white. Padding must not become pixel data.
    for (auto value : {0, 0, 255, 0, 255, 0, 99, 99, 255, 0, 0, 255, 255, 255, 88, 88})
        bytes.push_back(std::byte(value));
    return bytes;
}
void check_bitmap(const RecordingRenderer& renderer, const Bitmap& bitmap) {
    require(renderer.width == bitmap.width && renderer.height == bitmap.height);
    require(renderer.pixels == bitmap.rgba);
}
std::size_t check_chrome_normals(const MeshObject& object) {
    std::size_t eligible{};
    if ((object.flags & 0x400) != 0 && !object.parts.empty()) {
        const auto& first = object.parts[0];
        require(first.positions.size() > 2);
        require(first.normals.size() == first.positions.size());
        ++eligible;
    }
    for (const auto& child : object.children)
        eligible += check_chrome_normals(child);
    return eligible;
}
} // namespace
int main(int argc, char** argv) {
    for (bool enabled : {false, true})
        for (auto flags : {0u, 0x200u, 0x400u, 0x600u, 0xffffffffu})
            for (std::size_t part : {0u, 1u, 9u})
                for (bool bound : {false, true})
                    require(chrome_mapping_applies(enabled, flags, part, bound) ==
                            (enabled && (flags & 0x400) != 0 && part == 0 && bound));
    check_uv({1, 0, 0}, {}, 1, .5f);
    check_uv({0, 1, 0}, {}, .5f, 0);
    check_uv({0, 0, -1}, {}, .5f, .5f);
    check_uv({2, -3, 9}, {}, 1.5f, 2); // No normalization or UV clamp.
    Transform rotated{{{{0, 1, 0}, {-1, 0, 0}, {0, 0, 1}}}, {900, -700, 100}};
    check_uv({1, 0, 0}, rotated, .5f, 0);
    check_uv({0, 1, 0}, rotated, 0, .5f);
    Transform scaled{{{{0, 2, 0}, {-3, 0, 0}, {0, 0, 4}}}, {-99, 88, 77}};
    check_uv({1, 1, 1}, scaled, -1, -.5f); // Upper 3x3, not inverse transpose.
    Transform mixed{{{{1, 2, 3}, {4, 5, 6}, {7, 8, 9}}}, {999, 999, 999}};
    check_uv({2, -1, .5f}, mixed, 1.25f, -1);
    check_coordinate_conversion();
    const auto color = chrome_mapping_add({.1f, .8f, 1}, {.2f, .4f, 1});
    require(std::abs(color[0] - .3f) < .00001f && color[1] == 1 && color[2] == 1);
    require(chrome_mapping_add({0, .25f, .75f}, {0, 0, 0}) ==
            (std::array<float, 3>{0, .25f, .75f}));

    MemoryReader reader;
    ResourceStore resources(reader);
    RecordingRenderer renderer;
    TextureHandle texture;
    bool enabled{};
    std::string error;
    require(load_chrome_mapping(resources, renderer, texture, enabled, error) != Result::ok);
    require(!texture.value && !error.empty() && renderer.uploads == 0);
    reader.text("Ini/RoadWars.ini", "[ROADWARS]\nENVIRONMENT_MAPPING=0\n");
    require(load_chrome_mapping(resources, renderer, texture, enabled, error) == Result::ok);
    require(!enabled && !texture.value && renderer.uploads == 0);
    reader.text("Ini/RoadWars.ini", "[ROADWARS]\nENVIRONMENT_MAPPING=1\n");
    error.clear();
    require(load_chrome_mapping(resources, renderer, texture, enabled, error) == Result::not_found);
    require(!texture.value && !error.empty() && renderer.live.empty());
    reader.files["TEXTURES/CR1.BMP"] = {std::byte{'B'}};
    error.clear();
    require(load_chrome_mapping(resources, renderer, texture, enabled, error) ==
            Result::invalid_argument);
    require(!texture.value && !error.empty() && renderer.uploads == 0);
    const auto bitmap_bytes = fixture_bitmap();
    reader.files["TEXTURES/CR1.BMP"] = bitmap_bytes;
    Bitmap bitmap;
    require(decode_bitmap(bitmap_bytes, bitmap) == Result::ok);
    TextureTransparency transparency;
    require(prepare_original_texture("Textures/cr1.bmp", bitmap, transparency) == Result::ok);
    // All original 24-bit surfaces use the RGB1555/color-key loader, even without X prefix.
    // This fixture contains no black key pixels, so every uploaded alpha remains opaque.
    require(transparency == TextureTransparency::color_key);
    // The original texture receives the first BMP file row, not decode_bitmap's top row.
    require(bitmap.rgba[0] == std::byte{255} && bitmap.rgba[1] == std::byte{0} &&
            bitmap.rgba[2] == std::byte{0});
    for (std::size_t i = 3; i < bitmap.rgba.size(); i += 4)
        require(bitmap.rgba[i] == std::byte{255});
    for (unsigned cycle = 0; cycle < 3; ++cycle) {
        require(load_chrome_mapping(resources, renderer, texture, enabled, error) == Result::ok);
        require(enabled && texture.value && renderer.live.size() == 1);
        check_bitmap(renderer, bitmap);
        renderer.destroy_texture(texture);
        require(renderer.live.empty());
    }
    renderer.fail_upload = true;
    error.clear();
    require(load_chrome_mapping(resources, renderer, texture, enabled, error) ==
            Result::unavailable);
    require(!texture.value && !error.empty() && renderer.live.empty());
    for (auto config : {"[ROADWARS]\n", "[ROADWARS]\nENVIRONMENT_MAPPING=bad\n"}) {
        reader.text("Ini/RoadWars.ini", config);
        error.clear();
        require(load_chrome_mapping(resources, renderer, texture, enabled, error) ==
                Result::invalid_argument);
        require(!texture.value && !error.empty());
    }
    renderer.fail_upload = false;
    renderer.options = {false, "Textures/16cr1.bmp"};
    reader.text("Ini/RoadWars.ini", "[ROADWARS]\nENVIRONMENT_MAPPING=1\n");
    const auto previous_uploads = renderer.uploads;
    require(load_chrome_mapping(resources, renderer, texture, enabled, error) == Result::ok);
    require(!enabled && !texture.value && renderer.uploads == previous_uploads);
    renderer.options.enabled = true;
    reader.text("Ini/RoadWars.ini", "[ROADWARS]\nENVIRONMENT_MAPPING=0\n");
    // Native cr1 exists, but the explicit missing compatibility asset must never fall back to it.
    error.clear();
    require(load_chrome_mapping(resources, renderer, texture, enabled, error) == Result::not_found);
    require(!texture.value && renderer.uploads == previous_uploads);
    require(error.find("Textures/16cr1.bmp") != std::string::npos);
    auto compatible_bytes = fixture_bitmap();
    compatible_bytes[54] = std::byte{255};
    compatible_bytes[56] = std::byte{0}; // Distinct blue first pixel verifies asset selection.
    reader.files["TEXTURES/16CR1.BMP"] = compatible_bytes;
    require(decode_bitmap(compatible_bytes, bitmap) == Result::ok);
    require(prepare_original_texture("Textures/16cr1.bmp", bitmap, transparency) == Result::ok);
    require(load_chrome_mapping(resources, renderer, texture, enabled, error) == Result::ok);
    require(enabled && texture.value && renderer.live.size() == 1);
    check_bitmap(renderer, bitmap);
    require(renderer.pixels[0] == std::byte{0} && renderer.pixels[2] == std::byte{255});
    renderer.destroy_texture(texture);
    require(renderer.live.empty());
    if (argc == 2) {
        Files files(argv[1]);
        ResourceStore installed(files);
        require(installed.open() == Result::ok);
        RecordingRenderer actual;
        Configuration config;
        float configured{};
        require(config.load(installed, "Ini/RoadWars.ini") == Result::ok);
        require(config.number("ROADWARS", "ENVIRONMENT_MAPPING", configured) == Result::ok);
        require(configured == 0); // Installed original mode is disabled and needs no native cr1.
        require(load_chrome_mapping(installed, actual, texture, enabled, error) == Result::ok);
        require(!enabled && !texture.value && actual.uploads == 0);
        actual.options = {true, "Textures/16cr1.bmp"};
        std::vector<std::byte> bytes;
        require(installed.read("Textures/16cr1.bmp", bytes) == Result::ok);
        require(decode_bitmap(bytes, bitmap) == Result::ok);
        require(prepare_original_texture("Textures/16cr1.bmp", bitmap, transparency) == Result::ok);
        require(transparency == TextureTransparency::color_key);
        for (unsigned cycle = 0; cycle < 3; ++cycle) {
            require(load_chrome_mapping(installed, actual, texture, enabled, error) == Result::ok);
            require(enabled && texture.value && actual.live.size() == 1);
            check_bitmap(actual, bitmap);
            actual.destroy_texture(texture);
            require(actual.live.empty());
        }
        require(actual.live.empty());
        for (const auto* vehicle :
             {"Animal", "Avenging Angel", "Bud The Crud", "Daisy", "Electra", "Heartbreaker",
              "Kitiara", "Niki Roadkill", "Radcliff", "Romulus", "Sarge", "Wainright"}) {
            MeshObject body;
            require(installed.read(std::string("Cars/") + vehicle + "/Body/SpatialObject.msh",
                                   bytes) == Result::ok);
            require(decode_msh(bytes, body, error) == Result::ok);
            require(check_chrome_normals(body) > 0);
        }
    }
}
