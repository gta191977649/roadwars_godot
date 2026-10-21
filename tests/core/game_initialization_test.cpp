#include "game_src/unresolved/game_initialization.hpp"

#include <map>
#include <stdexcept>
#include <string>

using namespace roadwars;

namespace {
void require(bool condition) {
    if (!condition)
        throw std::runtime_error("Shared initialization contract failed");
}

class MemoryReader final : public ResourceReader {
  public:
    std::map<std::string, std::vector<std::byte>> files;

    void text(const std::string& path, const std::string& content, bool encoded = false) {
        auto& bytes = files[path];
        bytes.clear();
        for (unsigned char value : content) {
            bytes.push_back(static_cast<std::byte>(encoded ? (value + 1) & 255 : value));
        }
    }

    Result read(std::string_view path, std::vector<std::byte>& output) override {
        auto found = files.find(std::string(path));
        if (found == files.end())
            return Result::not_found;
        output = found->second;
        return Result::ok;
    }
};

void install_fixture(MemoryReader& reader) {
    reader.text("Ini/RoadWars.ini",
                "[ROADWARS]\nSCREENX=640\nSCREENY=480\nFULLSCREEN=0\nLANGUAGE=0\n");
    reader.text("Ini/RoadWars.pse",
                "[ROADWARS]\nSCREENX=1280\nSCREENY=720\nFULLSCREEN=1\nLANGUAGE=2\n", true);
    reader.text("Localization/Garage.bin", "opaque garage table");
    reader.text("Localization/GarageDescriptions.bin", "opaque garage descriptions");
    reader.text("Cars/Animal/Data/Data.pse", "[METRICS]\nMASS(LBS)=3080\n", true);
    reader.text("Tracks/Dark Forest/World/Data.pse", "[DATA]\nVIEWDISTANCE(M)=400\n", true);
    reader.text("Tracks/Shared/Data.pse", "[INFO]\nTEST=1\n", true);
}
} // namespace

int main() {
    MemoryReader reader;
    install_fixture(reader);
    ResourceStore resources(reader);
    InitializedGame initialized;
    std::string error;

    require(initialize_test_game(resources, {"Dark Forest", "Animal"}, initialized, error) ==
            Result::ok);
    require(error.empty());
    require(initialized.display.width == 1280 && initialized.display.height == 720);
    require(initialized.display.fullscreen && initialized.display.language == 2);
    require(initialized.options.track == "Dark Forest" && initialized.options.vehicle == "Animal");
    require(initialized.vehicle_config.value("METRICS", "MASS(LBS)") == "3080");
    require(!initialized.garage_localization.empty());

    initialized.options.track = "sentinel";
    require(initialize_test_game(resources, {"../Dark Forest", "Animal"}, initialized, error) ==
            Result::invalid_argument);
    require(initialized.options.track == "sentinel");

    reader.files.erase("Localization/Garage.bin");
    require(initialize_test_game(resources, {"Dark Forest", "Animal"}, initialized, error) ==
            Result::not_found);
    require(error.find("Garage.bin") != std::string::npos);
    require(initialized.options.track == "sentinel");

    install_fixture(reader);
    reader.text("Ini/RoadWars.pse",
                "[ROADWARS]\nSCREENX=-1\nSCREENY=720\nFULLSCREEN=0\nLANGUAGE=0\n", true);
    require(initialize_test_game(resources, {"Dark Forest", "Animal"}, initialized, error) ==
            Result::invalid_argument);
    require(initialized.options.track == "sentinel");

    install_fixture(reader);
    reader.files.erase("Ini/RoadWars.pse");
    require(initialize_test_game(resources, {"Dark Forest", "Animal"}, initialized, error) ==
            Result::ok);
    require(initialized.display.width == 640);
}
