#include "game_src/unresolved/track_collision.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace roadwars;
class TrackProbeReader final : public ResourceReader {
  public:
    explicit TrackProbeReader(std::filesystem::path root) : root_(std::move(root)) {}
    Result read(std::string_view path, std::vector<std::byte>& output) override {
        std::ifstream stream(root_ / std::filesystem::path(path), std::ios::binary | std::ios::ate);
        if (!stream)
            return Result::not_found;
        auto size = stream.tellg();
        if (size < 0)
            return Result::invalid_argument;
        output.resize(static_cast<std::size_t>(size));
        stream.seekg(0);
        stream.read(reinterpret_cast<char*>(output.data()), size);
        return stream ? Result::ok : Result::invalid_argument;
    }

  private:
    std::filesystem::path root_;
};
int main(int argc, char** argv) {
    if (argc != 2)
        return 2;
    TrackProbeReader reader(argv[1]);
    ResourceStore resources(reader);
    if (resources.open() != Result::ok)
        return 1;
    std::vector<std::byte> bytes;
    if (resources.read("Tracks/Dark Forest/World/SpatialObject.msh", bytes) != Result::ok)
        return 1;
    std::string error;
    MeshObject scene;
    if (decode_msh(bytes, scene, error) != Result::ok) {
        std::cerr << error;
        return 1;
    }
    TrackCollision track;
    if (track.load(resources, "Dark Forest", scene, error) != Result::ok) {
        std::cerr << error;
        return 1;
    }
    const auto& spawn = track.spawn();
    auto ground = track.sample(spawn.position);
    std::cout << "Spawn: " << spawn.position.x << ',' << spawn.position.y << ',' << spawn.position.z
              << " heading=" << spawn.heading_degrees
              << " surface records=" << track.surface_record_count() << " ground=" << ground.point.y
              << " normal=" << ground.normal.x << ',' << ground.normal.y << ',' << ground.normal.z
              << " friction=" << ground.longitudinal_grip << ',' << ground.lateral_grip << ','
              << ground.rolling_resistance << '\n';
    return ground.hit ? 0 : 1;
}
