#include "game_src/unresolved/track_render_settings.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace roadwars;
class RenderProbeReader final : public ResourceReader {
  public:
    explicit RenderProbeReader(std::filesystem::path root) : root_(std::move(root)) {}
    Result read(std::string_view name, std::vector<std::byte>& bytes) override {
        std::ifstream input(root_ / std::filesystem::path(name), std::ios::binary | std::ios::ate);
        if (!input)
            return Result::not_found;
        const auto size = input.tellg();
        if (size < 0)
            return Result::invalid_argument;
        bytes.resize(static_cast<std::size_t>(size));
        input.seekg(0);
        input.read(reinterpret_cast<char*>(bytes.data()), size);
        return input ? Result::ok : Result::invalid_argument;
    }

  private:
    std::filesystem::path root_;
};
int main(int argc, char** argv) {
    if (argc != 2)
        return 2;
    RenderProbeReader reader(argv[1]);
    ResourceStore resources(reader);
    if (resources.open() != Result::ok)
        return 1;
    TrackRenderSettings settings;
    ViewDistanceMap map;
    std::string error;
    if (load_track_render_settings(resources, "Dark Forest", settings, error) != Result::ok ||
        map.load(resources, "Dark Forest", error) != Result::ok) {
        std::cerr << error;
        return 1;
    }
    const float distance = map.sample({153.592f, 4.07632f, 507.128f});
    std::cout << "regions=" << map.region_count() << " initialfar=" << map.maximum_distance()
              << " spawnfar=" << distance << " scalar=" << settings.view_distance_scalar
              << " fog=" << settings.fog_color.x << ',' << settings.fog_color.y << ','
              << settings.fog_color.z << " sky=" << settings.sky_path << '\n';
    if (map.region_count() != 6 || !settings.use_sky || settings.configured_view_distance != 400)
        return 1;
}
