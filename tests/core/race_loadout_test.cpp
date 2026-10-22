#include "game_src/unresolved/race_loadout.hpp"
#include "game_src/unresolved/vehicle_dynamics.hpp"
#include "game_src/unresolved/weapon_trails.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>

using namespace roadwars;
namespace {
void require(bool condition, std::string_view message) {
    if (!condition)
        throw std::runtime_error(std::string(message));
}
class Memory final : public ResourceReader {
  public:
    std::map<std::string, std::string> files;
    Result read(std::string_view path, std::vector<std::byte>& bytes) override {
        const auto found = files.find(ResourceStore::canonical(path));
        if (found == files.end())
            return Result::not_found;
        bytes.clear();
        for (unsigned char c : found->second)
            bytes.push_back(static_cast<std::byte>(c));
        return Result::ok;
    }
    void put(std::string_view path, std::string text) {
        files[ResourceStore::canonical(path)] = std::move(text);
    }
};
class Files final : public ResourceReader {
  public:
    explicit Files(std::filesystem::path root) : root_(std::move(root)) {}
    Result read(std::string_view path, std::vector<std::byte>& bytes) override {
        std::ifstream stream(root_ / std::filesystem::path(path), std::ios::binary | std::ios::ate);
        if (!stream)
            return Result::not_found;
        const auto size = stream.tellg();
        if (size < 0)
            return Result::invalid_argument;
        bytes.resize(static_cast<std::size_t>(size));
        stream.seekg(0);
        stream.read(reinterpret_cast<char*>(bytes.data()), size);
        return stream ? Result::ok : Result::invalid_argument;
    }

  private:
    std::filesystem::path root_;
};
void fixture(Memory& memory) {
    for (std::string slot : {"Front", "Side", "Rear"}) {
        const auto base = "Cars/Shared/Weapons/" + slot;
        memory.put(base + "/Data.ini", "[ORDERING]\nNUMITEMS=3\nITEM0=A\nITEM1=B\nITEM2=C\n");
        memory.put(base + "/A/Data.ini", "[PRICE]\nPRICE($)=1\n");
        memory.put(base + "/B/Data.ini", "[PRICE]\nISAVAILABLE=0\nPRICE($)=1\n");
        memory.put(base + "/C/Data.ini", "[PRICE]\nISAVAILABLE=1\nPRICE($)=1\n");
    }
    memory.put("Cars/Shared/Armor/Data.ini", "[ORDERING]\nNUMITEMS=1\nITEM0=STEEL\n");
    memory.put("Cars/Shared/Armor/STEEL/Data.ini",
               "[ORDERING]\nNUMITEMS=2\nITEM0=POOR\nITEM1=BEST\n");
    memory.put("Cars/Shared/Drive Train/Data.ini",
               "[ORDERING]\nNUMITEMS=2\nITEM0=AIR FILTER\nITEM1=NITROUS BOOST\n");
    for (const std::string item : {"AIR FILTER", "NITROUS BOOST"})
        memory.put("Cars/Shared/Drive Train/" + item + "/Data.ini",
                   "[ORDERING]\nNUMITEMS=2\nITEM0=POOR\nITEM1=BEST\n");
    memory.put("Cars/Shared/Tires/Data.ini",
               "[ORDERING]\nNUMITEMS=2\nITEM0=STREET\nITEM1=OFF ROAD\n");
    memory.put("Tracks/Test/World/Data.txt", "[AI]\nTIRES=OFF ROAD\n");
}
} // namespace
int main(int argc, char** argv) {
    try {
        std::uint32_t rng = 1;
        constexpr std::array<std::uint32_t, 8> expected{41,    18467, 6334,  26500,
                                                        19169, 15724, 11478, 29358};
        for (const auto value : expected)
            require(original_random(rng) == value, "CRT random reference mismatch");
        const std::array<std::string, 5> opponents{"A", "B", "C", "D", "E"};
        std::vector<std::string> characters;
        std::string selection_error;
        rng = 1;
        require(select_race_characters(rng, opponents, "Player", 2, 6, characters,
                                       selection_error) == Result::ok,
                selection_error);
        require(characters == std::vector<std::string>{"B", "E", "Player", "C", "A", "D"},
                "Character draws/removal order differs from original");
        require(!original_randomizes_loadout(2, 2, false, false) &&
                    original_randomizes_loadout(2, 2, true, false) &&
                    original_randomizes_loadout(2, 2, false, true) &&
                    original_randomizes_loadout(0, 2, false, false),
                "Original preserved-player predicate differs");
        Memory memory;
        fixture(memory);
        ResourceStore resources(memory);
        RaceLoadoutTable table;
        RaceLoadout result;
        std::string error;
        require(table.assign(rng, result, error) == Result::unavailable,
                "Unloaded table succeeded");
        require(table.load(resources, "Test", error) == Result::ok, error);
        for (const auto& slot : table.weapons())
            require(slot == std::vector<std::string>{"A", "C"}, "Unavailable weapon not filtered");
        rng = 1;
        require(table.assign(rng, result, error) == Result::ok, error);
        require(result.paint_job == 6 &&
                    result.weapons == std::array<std::string, 3>{"C", "A", "A"},
                "Selection call order differs from assembly");
        require(result.tires.type == "OFF ROAD" && result.tires.quality == "BEST",
                "Track tires not used");
        require(result.armor.quality == "POOR" && result.drivetrain[0].quality == "POOR" &&
                    result.drivetrain[1].quality == "BEST",
                "Quality draws/nitrous override differ");
        memory.put("Cars/Shared/Weapons/Rear/Data.ini",
                   "[ORDERING]\nNUMITEMS=1\nITEM0=Reactive Mines\n");
        memory.put("Cars/Shared/Weapons/Rear/Reactive Mines/Data.ini", "[PRICE]\nISAVAILABLE=1\n");
        require(table.load(resources, "Test", error) == Result::ok, error);
        std::vector<std::string> stages;
        std::array<WeaponRearBlastPool, 5> reactive;
        rng = 1;
        ParticleRandom shared([&] { return original_random(rng); });
        RaceLoadoutInstallers installers;
        installers.paint = [&](int) {
            stages.push_back("paint");
            return Result::ok;
        };
        installers.tires = [&](const TireSelection&) {
            stages.push_back("tires");
            return Result::ok;
        };
        installers.weapon = [&](std::size_t slot, std::string_view weapon) {
            stages.push_back("weapon" + std::to_string(slot));
            if (slot == 2) {
                require(weapon == "Reactive Mines", "Expected actual Reactive constructor fixture");
                for (auto& pool : reactive)
                    require(pool.configure(weapon, shared) == Result::ok,
                            "Reactive initialization failed");
            }
            return Result::ok;
        };
        installers.armor = [&](const EquipmentSelection&) {
            stages.push_back("armor");
            return Result::ok;
        };
        installers.drivetrain = [&](const EquipmentSelection& item) {
            stages.push_back(item.type + "/" + item.quality);
            return Result::ok;
        };
        require(table.assign(rng, result, installers, error) == Result::ok, error);
        std::uint32_t reference = 1;
        for (int index = 0; index < 4 + 50; ++index)
            original_random(reference);
        original_random(reference); // Armor category, after Reactive's fifty constructor draws.
        const auto armor_quality = original_random(reference) % 2 ? "BEST" : "POOR";
        const auto air_quality = original_random(reference) % 2 ? "BEST" : "POOR";
        const auto nitro_quality = original_random(reference) % 2 ? "BEST" : "POOR";
        require(rng == reference && result.armor.quality == armor_quality &&
                    result.drivetrain[0].quality == air_quality,
                "Factory RNG did not occur between selection draws");
        require(stages == std::vector<std::string>{"paint", "tires", "weapon0", "weapon1",
                                                   "weapon2", "armor",
                                                   std::string("AIR FILTER/") + air_quality,
                                                   std::string("NITROUS BOOST/") + nitro_quality,
                                                   "NITROUS BOOST/BEST"},
                "Synchronous installer order differs");
        memory.put("Tracks/Test/World/Data.txt", "[AI]\nTIRES=UNKNOWN\n");
        require(table.load(resources, "Test", error) == Result::invalid_argument,
                "Unknown tire accepted");
        memory.put("Cars/Shared/Weapons/Front/C/Data.ini", "[PRICE]\nISAVAILABLE=nan\n");
        require(table.load(resources, "Test", error) == Result::invalid_argument,
                "Bad availability accepted");
        if (argc > 1) {
            Files files(argv[1]);
            ResourceStore installed(files);
            require(installed.open() == Result::ok, "Original archive unavailable");
            for (const auto track : {"Dark Forest", "RWA Speedway"}) {
                require(table.load(installed, track, error) == Result::ok, error);
                rng = 1;
                for (int car = 0; car < 6; ++car) {
                    require(table.assign(rng, result, error) == Result::ok, error);
                    VehicleParameters parameters;
                    require(load_vehicle_metrics(installed, "Animal", parameters, error) ==
                                Result::ok,
                            error);
                    parameters.tires = result.tires;
                    require(load_vehicle_geometry(installed, "Animal", parameters, error) ==
                                Result::ok,
                            error);
                    float scale{};
                    require(load_wheel_scale(installed, "Animal", result.tires, scale, error) ==
                                Result::ok,
                            error);
                    std::cout << "LOADOUT " << track << ' ' << car << " tires=" << result.tires.type
                              << '/' << result.tires.quality << " scale=" << scale
                              << " radius=" << parameters.rear_wheel_radius
                              << " weapons=" << result.weapons[0] << ',' << result.weapons[1] << ','
                              << result.weapons[2] << '\n';
                }
            }
        }
        std::cout << "Race loadout reference tests passed\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
