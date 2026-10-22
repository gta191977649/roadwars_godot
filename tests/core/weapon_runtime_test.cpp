#include "game_src/unresolved/weapon_runtime.hpp"
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
using namespace roadwars;
namespace {
void require(bool value) {
    if (!value)
        throw std::runtime_error("Original weapon runtime contract mismatch");
}
class Reader final : public ResourceReader {
  public:
    std::string text;
    Result read(std::string_view path, std::vector<std::byte>& output) override {
        if (path != "Cars/Shared/Weapons/Front/Machine Gun/Data.ini")
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
        if (!file.read(reinterpret_cast<char*>(output.data()), size))
            return Result::unavailable;
        return Result::ok;
    }

  private:
    std::filesystem::path root_;
};
} // namespace
int main(int argc, char** argv) {
    require(weapon_descriptors().size() == 21);
    std::array<unsigned, 3> counts{};
    for (const auto& descriptor : weapon_descriptors()) {
        require(find_weapon_descriptor(descriptor.name) == &descriptor);
        require(descriptor.constructor_address >= 0x00400000 && descriptor.vtable_address != 0);
        ++counts[static_cast<unsigned>(descriptor.slot)];
    }
    require(counts == std::array<unsigned, 3>{8, 6, 7});
    require(weapon_is_guided("GUIDED MISSILE") && weapon_is_guided("Back Missile"));
    require(weapon_is_guided("The Excalibur") && weapon_is_guided("Swarmers"));
    require(!weapon_is_guided("Rocket Launcher") && !find_weapon_descriptor("Invented Weapon"));
    Reader reader;
    reader.text = "[PRICE]\nDEFAULTAMMO=100\n[DAMAGE]\nHITPOINTS=4\n[PHYSICS]\n"
                  "AMMO_COUNT=1\nAMMO_TOP_SPEED=200\nAMMO_ACC=1\n"
                  "AMMO_IMPACT_MULTIPLIER=3\nAMMO_YROTATIONAL_SPEED=360\n"
                  "AMMO_MASS_IN_POUNDS=200\nAMMO_TRACKING_TIMER_SECONDS=0\n";
    ResourceStore resources(reader);
    WeaponParameters parameters;
    std::string error;
    require(load_weapon_parameters(resources, *find_weapon_descriptor("Machine Gun"), parameters,
                                   error) == Result::ok);
    require(parameters.default_ammunition == 100 && parameters.hitpoints == 4 &&
            parameters.acceleration == 1 && parameters.top_speed == 200);
    require(std::abs(parameters.ammunition_mass_kg - 90.7199979f) < .0001f);
    reader.text = "[PRICE]\nDEFAULTAMMO=2.5\n";
    require(load_weapon_parameters(resources, *find_weapon_descriptor("Machine Gun"), parameters,
                                   error) == Result::invalid_argument);
    WeaponMountRuntime mount;
    std::optional<WeaponFireRequest> fired;
    require(mount.advance(.1f, true, 0, 0, {}, fired) == Result::unavailable);
    require(mount.initialize(WeaponSlot::side, 2) == Result::ok && !mount.visible());
    require(!mount.request(10, false) && mount.ammunition() == 2);
    require(mount.request(10, true) && mount.ammunition() == 1 && mount.visible());
    require(!mount.request(10, true) && mount.ammunition() == 1);
    require(mount.advance(.5f, true, 15, 20, {1, 2, 3}, fired) == Result::ok && !fired);
    require(mount.pending() && mount.deployment_roll() == -90);
    require(mount.advance(0, true, 15, 20, {1, 2, 3}, fired) == Result::ok && fired);
    require(!mount.pending() && fired->heading == 15 && fired->pitch == 20 &&
            fired->origin.y == 2 && fired->speed == 10);
    require(mount.advance(.5f, false, 0, 0, {}, fired) == Result::ok && mount.visible());
    require(mount.advance(.5f, true, 0, 0, {}, fired) == Result::ok && !mount.visible());
    require(mount.request(7, true) && mount.ammunition() == 0);
    mount.cancel();
    require(!mount.request(7, true));
    require(mount.initialize(WeaponSlot::rear, 1) == Result::ok);
    require(mount.request(9, true));
    require(mount.advance(.01f, true, 270, 4, {2, 3, 4}, fired) == Result::ok && fired);
    require(fired->heading == 450 && fired->pitch == 4 && mount.ammunition() == 0);
    require(mount.initialize(WeaponSlot::front, 0) == Result::ok && !mount.request(1, true));
    require(mount.advance(std::numeric_limits<float>::quiet_NaN(), true, 0, 0, {}, fired) ==
            Result::invalid_argument);
    if (argc == 2) {
        DirectoryReader installed(argv[1]);
        ResourceStore originals(installed);
        require(originals.open() == Result::ok);
        for (const auto& descriptor : weapon_descriptors()) {
            WeaponParameters original;
            require(load_weapon_parameters(originals, descriptor, original, error) == Result::ok);
            require(std::isfinite(original.hitpoints) && std::isfinite(original.top_speed));
        }
    }
}
