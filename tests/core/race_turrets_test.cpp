#include "game_src/unresolved/race_turrets.hpp"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
using namespace roadwars;
namespace {
void require(bool condition) {
    if (!condition)
        throw std::runtime_error("Original scene turret contract mismatch");
}
class Reader final : public ResourceReader {
  public:
    bool guns{true};
    Result read(std::string_view name, std::vector<std::byte>& output) override {
        std::string text;
        if (name == "Tracks/Test/World/Data.txt")
            text = guns ? "[DATA]\nGUNS=1\n" : "[DATA]\nGUNS=0\n";
        else if (name == "Characters/Test/Data.ini")
            text = "[PRICE_SCALARS]\nNOT_A_TURRET_TARGET=1\n";
        else if (name == "Cars/Shared/Weapons/Front/20mm Cannon/Data.ini")
            text = "[PRICE]\nDEFAULTAMMO=100\n[DAMAGE]\nHITPOINTS=4\n[PHYSICS]\n"
                   "AMMO_COUNT=1\nAMMO_TOP_SPEED=200\nAMMO_ACC=1\n"
                   "AMMO_IMPACT_MULTIPLIER=3\nAMMO_YROTATIONAL_SPEED=360\n"
                   "AMMO_MASS_IN_POUNDS=200\nAMMO_TRACKING_TIMER_SECONDS=0\n";
        else
            return Result::not_found;
        output.resize(text.size());
        std::memcpy(output.data(), text.data(), text.size());
        return Result::ok;
    }
};
class DirectoryReader final : public ResourceReader {
  public:
    explicit DirectoryReader(std::filesystem::path root) : root_(std::move(root)) {}
    Result read(std::string_view name, std::vector<std::byte>& output) override {
        std::ifstream file(root_ / std::filesystem::path(name), std::ios::binary | std::ios::ate);
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
} // namespace
int main(int argc, char** argv) {
    Reader reader;
    ResourceStore resources(reader);
    MeshObject world;
    world.name = "Root";
    world.position = {10, 2, 3};
    MeshObject turret;
    turret.name = "Turret_090_Full_C";
    turret.position = {20, 4, 5};
    turret.parts.resize(1);
    turret.parts[0].positions = {{-1, -1, -2}, {1, 1, 2}};
    world.children.push_back(turret);
    RaceTurrets race;
    std::string error;
    require(race.load(resources, "Test", world, error) == Result::ok);
    require(race.poses().size() == 1 && race.guns_enabled());
    const auto& pose = race.poses()[0];
    require(pose.node_path == std::vector<std::size_t>{0} && pose.world_position.x == 30 &&
            pose.world_position.y == 6 && pose.world_position.z == 8 && pose.heading == 90);
    bool exempt{};
    require(load_turret_target_exemption(resources, "Test", exempt, error) == Result::ok && exempt);
    TurretTarget target;
    target.id = 2;
    target.position = {130, 6, 8};
    std::vector<RaceTurretFireEvent> events;
    require(race.advance(3.01f, {&target, 1}, events, error) == Result::ok && events.empty());
    for (int i = 0; i < 4; ++i)
        require(race.advance(.1f, {&target, 1}, events, error) == Result::ok);
    require(events.size() == 1 && events[0].turret_index == 0 &&
            events[0].weapon->name == "20mm Cannon" && events[0].firing.target == 2);
    require(events[0].firing.origin.x > 33.99f && events[0].firing.origin.x < 34.01f);
    require(race.poses()[0].target == 2);
    race.reset();
    require(!race.poses()[0].target && race.poses()[0].heading == 90);
    reader.guns = false;
    require(race.load(resources, "Test", world, error) == Result::ok && !race.guns_enabled());
    require(race.advance(10, {&target, 1}, events, error) == Result::ok && events.empty());
    world.type = 4;
    world.dummy_heading_degrees = 90; // Metadata does not rotate descendants.
    require(race.load(resources, "Test", world, error) == Result::ok);
    world.angles_degrees.y = 1;
    require(race.load(resources, "Test", world, error) == Result::unavailable);
    world.angles_degrees = {};
    if (argc == 2) {
        const std::filesystem::path root(argv[1]);
        DirectoryReader installed(root);
        ResourceStore original(installed);
        require(original.open() == Result::ok);
        for (const auto& entry : std::filesystem::directory_iterator(root / "Tracks")) {
            if (!entry.is_directory() || entry.path().filename() == "Shared")
                continue;
            const auto track = entry.path().filename().string();
            std::vector<std::byte> bytes;
            MeshObject scene;
            require(original.read("Tracks/" + track + "/World/SpatialObject.msh", bytes) ==
                    Result::ok);
            require(decode_msh(bytes, scene, error) == Result::ok);
            require(race.load(original, track, scene, error) == Result::ok);
            if (track == "RWA Speedway")
                require(race.poses().size() == 6);
            for (const auto& item : race.poses()) {
                const MeshObject* node = &scene;
                Vec3 position = scene.position;
                for (auto index : item.node_path) {
                    node = &node->children[index];
                    position.x += node->position.x;
                    position.y += node->position.y;
                    position.z += node->position.z;
                }
                require(node->name == item.name && position.x == item.world_position.x &&
                        position.y == item.world_position.y && position.z == item.world_position.z);
            }
        }
    }
}
