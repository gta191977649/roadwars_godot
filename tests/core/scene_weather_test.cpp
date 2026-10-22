#include "game_src/unresolved/scene_weather.hpp"
#include <bit>
#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
using namespace roadwars;
namespace {
class Reader : public ResourceReader {
  public:
    Result read(std::string_view path, std::vector<std::byte>& bytes) override {
        if (path.ends_with(".pse"))
            return Result::not_found;
        if (path.ends_with("Data.txt")) {
            const std::string text = "[DATA]\nWEATHER=1\n";
            bytes.resize(text.size());
            std::memcpy(bytes.data(), text.data(), text.size());
            return Result::ok;
        }
        if (path == "Meshes/snowflake.msh") {
            bytes.assign(56, std::byte{});
            bytes[32] = std::byte{1};
            return Result::ok;
        }
        return Result::not_found;
    }
};
class DirectoryReader : public ResourceReader {
  public:
    explicit DirectoryReader(std::filesystem::path value) : root(std::move(value)) {}
    Result read(std::string_view path, std::vector<std::byte>& bytes) override {
        std::ifstream f(root / std::filesystem::path(path), std::ios::binary | std::ios::ate);
        if (!f)
            return Result::not_found;
        const auto size = f.tellg();
        if (size < 0)
            return Result::unavailable;
        bytes.resize(static_cast<std::size_t>(size));
        f.seekg(0);
        return f.read(reinterpret_cast<char*>(bytes.data()), size) ? Result::ok
                                                                   : Result::unavailable;
    }
    std::filesystem::path root;
};
void require(Result result, const std::string& error) {
    if (result != Result::ok)
        throw std::runtime_error(error);
}
} // namespace
int main(int argc, char** argv) {
    Reader reader;
    ResourceStore resources(reader);
    SceneWeather weather;
    std::string error;
    require(weather.load(resources, "Test", error), error);
    assert(weather.enabled());
    unsigned registered{};
    weather.observe_callbacks([&](std::uint64_t token, bool insert) {
        assert(token == 1 && insert);
        ++registered;
    });
    assert(registered == 1);
    SceneWeatherCamera camera;
    TrackCollision track;
    unsigned draws{};
    const auto random = [&]() {
        ++draws;
        return 0u;
    };
    assert(weather.advance(1.f / 60, camera, track, random, error) == Result::unavailable);
    assert(draws == 0);
    require(weather.advance(0, camera, track, random, error), error);
    require(weather.advance(.014999999664723873f, camera, track, random, error), error);
    assert(draws == 0);
    require(weather.advance(.001f, camera, track, random, error), error);
    assert(draws == 1 && weather.particles()[0].visible);
    assert(weather.particles()[0].heading == 180 && weather.particles()[0].age == .001f);
    assert(weather.particles()[0].ground_height == -99999);
    assert(weather.particles()[0].spawn_position.y == -99993);
    require(weather.load(resources, "Test", error), error);
    weather.seed_camera_history(camera);
    draws = 0;
    for (unsigned i = 0; i < 65; ++i)
        require(weather.advance(1.f / 60, camera, track, random, error), error);
    unsigned visible{};
    for (const auto& particle : weather.particles())
        visible += particle.visible;
    assert(draws == 65 && visible == 64); // Full-pool attempt still consumes one native rand().
    assert(weather.particles()[0].age > 1 && weather.particles()[0].age < 1.2f);
    assert(weather.advance(-1, camera, track, random, error) == Result::invalid_argument);
    if (argc == 2) {
        DirectoryReader directory(argv[1]);
        ResourceStore installed(directory);
        require(installed.open(), error);
        unsigned enabled{};
        for (const auto& entry : std::filesystem::directory_iterator(directory.root / "Tracks")) {
            if (!entry.is_directory() || entry.path().filename() == "Shared")
                continue;
            require(weather.load(installed, entry.path().filename().string(), error), error);
            enabled += weather.enabled();
        }
        assert(enabled != 0);
    }
}
