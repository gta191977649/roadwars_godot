#include "game_src/unresolved/cockpit_frontend.hpp"
#include "game_src/unresolved/frontend_localization.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <numbers>
#include <stdexcept>
#include <string>

using namespace roadwars;
namespace {
void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}
bool close(float a, float b) { return std::abs(a - b) < .001f; }
class Files final : public ResourceReader {
  public:
    explicit Files(std::filesystem::path root) : root_(std::move(root)) {}
    Result read(std::string_view path, std::vector<std::byte>& output) override {
        std::ifstream file(root_ / std::filesystem::path(path), std::ios::binary | std::ios::ate);
        if (!file)
            return Result::not_found;
        const auto size = file.tellg();
        if (size < 0)
            return Result::unavailable;
        output.resize(static_cast<std::size_t>(size));
        file.seekg(0);
        return file.read(reinterpret_cast<char*>(output.data()), size) ? Result::ok
                                                                       : Result::unavailable;
    }

  private:
    std::filesystem::path root_;
};
class CaptureRenderer final : public Renderer {
  public:
    std::map<std::uint64_t, std::array<std::uint32_t, 2>> textures;
    std::uint64_t next{1};
    MeshHandle upload_mesh(std::span<const Vec3>, std::span<const std::uint32_t>) override {
        return {};
    }
    InstanceHandle create_instance(MeshHandle) override { return {}; }
    Result update_instance(InstanceHandle, const Transform&) override { return Result::ok; }
    void destroy_instance(InstanceHandle) override {}
    void destroy_mesh(MeshHandle) override {}
    void update_camera(const Transform&, float) override {}
    TextureHandle upload_texture(std::uint32_t width, std::uint32_t height,
                                 std::span<const std::byte> rgba) override {
        require(rgba.size() == static_cast<std::size_t>(width) * height * 4,
                "Invalid texture bytes");
        textures[next] = {width, height};
        return {next++};
    }
    void destroy_texture(TextureHandle texture) override {
        require(textures.erase(texture.value) == 1, "Texture released twice");
    }
};
bool equal(const std::vector<HudQuad>& a, const std::vector<HudQuad>& b) {
    if (a.size() != b.size())
        return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (a[i].texture.value != b[i].texture.value || a[i].positions != b[i].positions ||
            a[i].uv != b[i].uv || a[i].colors != b[i].colors ||
            a[i].preserve_aspect != b[i].preserve_aspect)
            return false;
    return true;
}
void validate_quads(const std::vector<HudQuad>& quads, const CaptureRenderer& renderer) {
    require(!quads.empty(), "Cockpit emitted no quads");
    for (const auto& q : quads) {
        require(renderer.textures.contains(q.texture.value), "Unknown texture");
        require(q.preserve_aspect, "Cockpit quad escaped isotropic space");
        for (const auto& p : q.positions)
            require(std::isfinite(p[0]) && std::isfinite(p[1]), "Nonfinite cockpit geometry");
        for (const auto& uv : q.uv)
            require(std::isfinite(uv[0]) && std::isfinite(uv[1]), "Nonfinite cockpit UV");
    }
}
void validate_wheel(const HudQuad& wheel, float x, float y, float size, float steering) {
    float center_x{}, center_y{};
    for (const auto& p : wheel.positions) {
        center_x += p[0] / 4;
        center_y += p[1] / 4;
    }
    // Original 0042f549 truncates SIZE/2 before forming the integer rectangle.
    // An odd-sized rectangle's geometric center is half a pixel from its pivot.
    const float offset = static_cast<int>(size) % 2 ? .5f : 0;
    const float angle = steering * 75 * std::numbers::pi_v<float> / 180;
    require(close(center_x, x + offset * (std::cos(angle) - std::sin(angle))) &&
                close(center_y, y + offset * (std::sin(angle) + std::cos(angle))),
            "Steering pivot moved");
    const auto length = [&](int a, int b) {
        return std::hypot(wheel.positions[a][0] - wheel.positions[b][0],
                          wheel.positions[a][1] - wheel.positions[b][1]);
    };
    require(close(length(0, 1), size) && close(length(1, 2), size), "Steering wheel was stretched");
    const float ax = wheel.positions[1][0] - wheel.positions[0][0];
    const float ay = wheel.positions[1][1] - wheel.positions[0][1];
    const float bx = wheel.positions[2][0] - wheel.positions[1][0];
    const float by = wheel.positions[2][1] - wheel.positions[1][1];
    require(std::abs(ax * bx + ay * by) < .05f, "Steering wheel corners lost orthogonality");
}
class MissingFiles final : public ResourceReader {
    Result read(std::string_view, std::vector<std::byte>&) override { return Result::not_found; }
};
class FixtureFiles final : public ResourceReader {
  public:
    Result read(std::string_view path, std::vector<std::byte>& output) override {
        const auto name = ResourceStore::canonical(path);
        if (name.ends_with(".INI")) {
            const std::string config =
                "[WHEEL]\nSIZE=128\nCENTERX=160\nCENTERY=400\n"
                "[TACKOMETER]\nMAXRPM=8000\nBASEROTATION=30\nROTATIONDEGREES=180\n"
                "X=50\nY=350\nPIVOTOFFSETX=3\nPIVOTOFFSETY=5\n"
                "[SPEEDOMETER]\nMAXMPH=200\nBASEROTATION=60\nROTATIONDEGREES=180\n"
                "X=250\nY=350\n[MAP]\nMAPOFFSETX=400\nMAPOFFSETY=350\n"
                "[WEAPONDISPLAY]\nWEAPON1X=100\nWEAPON1Y=300\nWEAPON1AMMOX=150\nWEAPON1AMMOY=300\n"
                "WEAPON2X=100\nWEAPON2Y=310\nWEAPON2AMMOX=150\nWEAPON2AMMOY=310\n"
                "WEAPON3X=100\nWEAPON3Y=320\nWEAPON3AMMOX=150\nWEAPON3AMMOY=320\n";
            output.clear();
            for (char c : config)
                output.push_back(static_cast<std::byte>(c));
            return Result::ok;
        }
        if (!name.ends_with(".BMP"))
            return Result::not_found;
        const bool font = name.find("FONT10IMP") != std::string::npos;
        const unsigned width = font ? 273 : 256, height = font ? 2 : 256;
        const unsigned stride = (width * 3 + 3) & ~3u;
        output.assign(54 + stride * height, std::byte{});
        const auto word = [&](unsigned offset, unsigned value, unsigned bytes) {
            for (unsigned i = 0; i < bytes; ++i)
                output[offset + i] = static_cast<std::byte>((value >> (8 * i)) & 255);
        };
        word(0, 0x4d42, 2);
        word(2, static_cast<unsigned>(output.size()), 4);
        word(10, 54, 4);
        word(14, 40, 4);
        word(18, width, 4);
        word(22, height, 4);
        word(26, 1, 2);
        word(28, 24, 2);
        for (unsigned y = 0; y < height; ++y)
            for (unsigned x = 0; x < width; ++x) {
                const auto value =
                    static_cast<std::byte>(font ? (y == 0 && x % 2 == 0 ? 255 : 0) : 120);
                const unsigned offset = 54 + (height - y - 1) * stride + x * 3;
                for (unsigned c = 0; c < 3; ++c)
                    output[offset + c] = value;
            }
        return Result::ok;
    }
};
} // namespace

int main(int argc, char** argv) {
    CaptureRenderer renderer;
    {
        CockpitFrontend cockpit(renderer);
        std::vector<HudQuad> output;
        require(cockpit.draw(0, 0, 0, 0, 0, false, 4.f / 3, output) != Result::ok,
                "Unloaded cockpit silently drew");
        MissingFiles missing;
        ResourceStore resources(missing);
        std::string error;
        require(cockpit.load(resources, "missing", {}, error) == Result::invalid_argument,
                "Empty participant set accepted");
        const std::array<std::string, 1> cars{"Missing"};
        require(cockpit.load(resources, "missing", cars, error) != Result::ok && !error.empty(),
                "Missing cockpit silently substituted");
        require(renderer.textures.empty(), "Failed load retained textures");
    }
    {
        FixtureFiles files;
        ResourceStore resources(files);
        const std::array<std::string, 1> cars{"Fixture"};
        CockpitFrontend cockpit(renderer);
        std::string error;
        require(cockpit.load(resources, "Fixture", cars, error) == Result::ok, error.c_str());
        for (float aspect : {4.f / 3, 16.f / 9, 16.f / 10, 21.f / 9, 32.f / 9}) {
            std::vector<HudQuad> output;
            require(cockpit.draw(0, 1, 4000, 20, 2, false, aspect, output) == Result::ok,
                    "Synthetic cockpit draw failed");
            validate_quads(output, renderer);
            validate_wheel(output.back(), 160, 400, 128, 1);
        }
        const std::array<std::string, 3> weapons{"MACHINE GUN", "ROCKET LAUNCHER", "LAND MINES"};
        const std::array<int, 3> ammunition{100, 20, 5};
        std::vector<HudQuad> track, armed;
        require(cockpit.draw_readouts(0, weapons, ammunition, {false, false, false}, track) ==
                    Result::ok,
                "Unarmed cockpit readout failed");
        require(cockpit.draw_readouts(0, weapons, ammunition, {true, true, true}, armed) ==
                        Result::ok &&
                    armed.size() > track.size(),
                "Installed weapon readouts missing");
        validate_quads(track, renderer);
        validate_quads(armed, renderer);
        require(close(armed.back().colors[0][0], 90.f / 255) &&
                    close(armed.back().colors[0][1], 182.f / 255) &&
                    close(armed.back().colors[0][2], 1),
                "Original cockpit font color changed");
        cockpit.stop();
        require(renderer.textures.empty(), "Synthetic cockpit leaked textures");
    }
    if (argc < 2)
        return 0;
    Files files(argv[1]);
    ResourceStore resources(files);
    require(resources.open() == Result::ok, "Installation resources unavailable");
    Configuration choices;
    require(choices.load(resources, "Screens/SelectCharacter/Data.ini") == Result::ok,
            "Character table unavailable");
    float count{};
    require(choices.number("INFO", "NUMSELECTIONS", count) == Result::ok && count == 12,
            "Expected twelve original characters");
    std::vector<std::string> cars;
    for (int i = 0; i < static_cast<int>(count); ++i)
        cars.push_back(choices.value("SELECTION" + std::to_string(i), "VALUE"));
    {
        CockpitFrontend cockpit(renderer);
        std::string error;
        require(cockpit.load(resources, "Dark Forest", cars, error) == Result::ok, error.c_str());
        const auto loaded_count = renderer.textures.size();
        for (std::size_t i = 0; i < cars.size(); ++i) {
            Configuration config;
            require(config.load(resources, "Cars/" + cars[i] + "/Cockpit/Data.ini") == Result::ok,
                    "Cockpit configuration unavailable");
            float x{}, y{}, size{}, map_x{}, map_y{};
            require(config.number("WHEEL", "CENTERX", x) == Result::ok &&
                        config.number("WHEEL", "CENTERY", y) == Result::ok &&
                        config.number("WHEEL", "SIZE", size) == Result::ok &&
                        config.number("MAP", "MAPOFFSETX", map_x) == Result::ok &&
                        config.number("MAP", "MAPOFFSETY", map_y) == Result::ok,
                    "Original cockpit anchors unavailable");
            const std::array<int, 2> expected_map{static_cast<int>(map_x), static_cast<int>(map_y)};
            require(cockpit.map_offset(i) == expected_map,
                    "Map anchor differs from original configuration");
            for (float aspect : {4.f / 3, 16.f / 9, 16.f / 10, 21.f / 9, 32.f / 9}) {
                for (float steering : {-1.f, 0.f, 1.f}) {
                    std::vector<HudQuad> first, different, repeated;
                    require(cockpit.draw(i, steering, 4000, 20, 2, false, aspect, first) ==
                                Result::ok,
                            "Installed cockpit draw failed");
                    validate_quads(first, renderer);
                    validate_wheel(first.back(), x, y, size, steering);
                    if (steering == 1) {
                        const auto& points = first.back().positions;
                        const float heading =
                            std::atan2(points[1][1] - points[0][1], points[1][0] - points[0][0]);
                        require(close(heading, 1.308996939f),
                                "Full steering does not rotate seventy-five degrees");
                    }
                    require(cockpit.draw(i, -steering, 8000, 40, 3, false, aspect, different) ==
                                Result::ok,
                            "Alternate cockpit draw failed");
                    require(cockpit.draw(i, steering, 4000, 20, 2, false, aspect, repeated) ==
                                    Result::ok &&
                                equal(first, repeated),
                            "Rendering mutated cockpit state");
                    require(renderer.textures.size() == loaded_count,
                            "Rendering allocated textures");
                    const auto saved = first;
                    require(cockpit.draw(i, std::numeric_limits<float>::quiet_NaN(), 0, 0, 0, false,
                                         aspect, first) == Result::invalid_argument &&
                                equal(first, saved),
                            "Invalid input changed output");
                }
            }
        }
        std::vector<HudQuad> invalid;
        require(cockpit.draw(0, 0, 1208410.625f, 7627.038086f, 3, false, 4.f / 3, invalid) ==
                    Result::ok,
                "Finite native high-RPM cockpit state was rejected by an arbitrary cap");
        invalid.clear();
        require(cockpit.draw(cars.size(), 0, 0, 0, 0, false, 4.f / 3, invalid) ==
                    Result::invalid_argument,
                "Invalid cockpit index accepted");
        require(cockpit.draw(0, 0, 0, 0, 0, false, 0, invalid) == Result::invalid_argument,
                "Invalid aspect accepted");
        cockpit.stop();
        cockpit.stop();
        require(renderer.textures.empty(), "Cockpit stop leaked textures");
    }
    require(renderer.textures.empty(), "Cockpit destructor leaked textures");
}
