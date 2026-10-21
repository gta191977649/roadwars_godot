#include "game_src/unresolved/track_render_settings.hpp"
#include <bit>
#include <cmath>
#include <map>
#include <stdexcept>
using namespace roadwars;
namespace {
void require(bool condition) {
    if (!condition)
        throw std::runtime_error("Track rendering contract failed");
}
void u32(std::vector<std::byte>& bytes, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8)
        bytes.push_back(std::byte((value >> shift) & 255));
}
void f32(std::vector<std::byte>& bytes, float value) {
    u32(bytes, std::bit_cast<std::uint32_t>(value));
}
void region(std::vector<std::byte>& bytes, float extent, float distance) {
    u32(bytes, 1);
    for (auto point : {Vec3{0, 0, 0}, Vec3{extent, 0, 0}, Vec3{0, 0, extent}}) {
        f32(bytes, -point.x);
        f32(bytes, point.y);
        f32(bytes, -point.z);
    }
    f32(bytes, distance);
}
class MemoryReader final : public ResourceReader {
  public:
    std::map<std::string, std::vector<std::byte>> files;
    void text(std::string name, std::string_view content) {
        auto& bytes = files[name];
        bytes.clear();
        for (unsigned char value : content)
            bytes.push_back(std::byte(value));
    }
    Result read(std::string_view name, std::vector<std::byte>& bytes) override {
        auto found = files.find(std::string(name));
        if (found == files.end())
            return Result::not_found;
        bytes = found->second;
        return Result::ok;
    }
};
} // namespace
int main() {
    MemoryReader reader;
    reader.text("Tracks/Fixture/World/Data.txt",
                "[DATA]\nVIEWDISTANCE(M)=400\nFOGR=96\nFOGG=100\nFOGB=80\nUSESKY=1\n");
    reader.text("Ini/RoadWars.ini", "[ROADWARS]\nVIEWDISTANCESCALAR=0.75\n");
    ResourceStore resources(reader);
    std::string error;
    TrackRenderSettings settings;
    require(load_track_render_settings(resources, "Fixture", settings, error) == Result::ok);
    require(settings.use_sky && settings.sky_path == "Tracks/Fixture/Sky/Sky.msh");
    require(settings.configured_view_distance == 400 && settings.near_distance == 1);
    require(settings.view_distance_scalar == .75f && settings.fog_color.y == 100 / 255.0f);
    require(std::abs(settings.directional_light.y + .5735764f) < .00001f);
    require(settings.directional_light.x > .5792f && settings.directional_light.x < .5793f);
    require(load_track_render_settings(resources, "../Fixture", settings, error) ==
            Result::invalid_argument);
    std::vector<std::byte> bytes;
    u32(bytes, 2);
    region(bytes, 10, 100);
    region(bytes, 20, 200);
    ViewDistanceMap map;
    require(map.decode(bytes, error) == Result::ok);
    require(map.region_count() == 2 && map.maximum_distance() == 200);
    require(map.sample({15, 1234, 1}) == 200); // XZ-only test and negated serialized coordinates.
    require(map.sample({1, 0, 1}) == 200);     // Cached overlapping region wins.
    require(map.sample({30, 0, 30}) == 450);   // Original fallback clears cached region.
    require(map.sample({1, 0, 1}) == 100);     // First matching region now wins.
    bytes.pop_back();
    require(map.decode(bytes, error) == Result::invalid_argument);
    require(map.region_count() == 2 && map.sample({1, 0, 1}) == 100);
    const auto sky = original_sky_transform({4, 99, 8}, 400);
    require(sky.origin.x == 4 && sky.origin.y == 0 && sky.origin.z == 8);
    require(sky.basis[0].x == 380 && sky.basis[2].z == 380);
    require(std::abs(sky.basis[1].y - 295.55556f) < .0001f);
}
