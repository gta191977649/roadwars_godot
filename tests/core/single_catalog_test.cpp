#include "game_src/unresolved/garage_session.hpp"
#include "game_src/unresolved/race_session.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace roadwars;
namespace {
void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
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
std::vector<std::string> selections(ResourceStore& resources, const std::string& path) {
    Configuration config;
    require(config.load(resources, path) == Result::ok, "Cannot load selection table: " + path);
    float count{};
    require(config.number("INFO", "NUMSELECTIONS", count) == Result::ok && count > 0 &&
                count <= 256 && std::floor(count) == count,
            "Invalid original selection count");
    std::vector<std::string> result;
    for (int index = 0; index < static_cast<int>(count); ++index) {
        const auto section = "SELECTION" + std::to_string(index);
        float available{};
        require(config.number(section, "ISAVAILABLE", available) == Result::ok,
                "Missing original selection availability");
        if (available != 0) {
            const auto name = config.value(section, "VALUE");
            require(!name.empty(), "Empty available original selection");
            result.push_back(name);
        }
    }
    return result;
}
void transact(GarageSession& garage, GarageSelection selection, std::string& error) {
    GarageQuote quote;
    require(garage.quote(selection, quote, error) == Result::ok, error);
    require(quote.affordable &&
                (quote.action == GarageAction::buy || quote.action == GarageAction::repair),
            "Fixture selected a non-purchasable product");
    const auto cash = garage.state().cash;
    bool changed{};
    require(garage.transact(selection, true, changed, error) == Result::ok && changed, error);
    require(garage.state().cash == cash + quote.balance_change,
            "Quote and committed balance differ");
}
GarageSelection named_product(const GarageSession& garage, GarageCategory category,
                              std::string_view type, std::string_view quality) {
    const auto items = garage.items(category);
    for (std::size_t item = 0; item < items.size(); ++item)
        if (ResourceStore::canonical(items[item].name) == ResourceStore::canonical(type))
            for (std::size_t product = 0; product < items[item].products.size(); ++product)
                if (ResourceStore::canonical(items[item].products[product].name) ==
                        ResourceStore::canonical(quality) &&
                    items[item].products[product].available)
                    return {category, item, product};
    throw std::runtime_error("Original fixture product unavailable: " + std::string(type) + "/" +
                             std::string(quality));
}
void upgrade(GarageSession& garage, std::string_view track, std::string& error) {
    // Diagnostic funding/damage fixture. Original Single still starts with its actual funds.
    auto state = garage.state();
    require(state.cash == 25000, "Single initial funds changed");
    state.cash = 2000000;
    state.condition.internal_hp *= .5f;
    for (auto& hp : state.condition.armor_hp)
        hp *= .5f;
    require(garage.restore(std::move(state), track, error) == Result::ok, error);
    const auto tires = garage.items(GarageCategory::tires);
    const auto original_tires = ResourceStore::canonical(garage.state().loadout.tires.type);
    const auto alternate = std::find_if(tires.begin(), tires.end(), [&](const auto& item) {
        return ResourceStore::canonical(item.name) != original_tires;
    });
    require(alternate != tires.end(), "No alternative original tire type");
    transact(garage, named_product(garage, GarageCategory::tires, alternate->name, "Best"), error);
    const auto drivetrain = garage.items(GarageCategory::drivetrain);
    for (const auto& item : drivetrain)
        transact(garage, named_product(garage, GarageCategory::drivetrain, item.name, "Best"),
                 error);
    for (const auto& slot : std::array<std::pair<std::string_view, std::string_view>, 3>{
             {{"Front", "Machine Gun"}, {"Side", "Gatling Gun"}, {"Rear", "Missile Jammer"}}})
        transact(garage, named_product(garage, GarageCategory::weapons, slot.first, slot.second),
                 error);
    bool armored{};
    for (const auto& item : garage.items(GarageCategory::armor)) {
        if (ResourceStore::canonical(item.name) == "NONE")
            continue;
        transact(garage, named_product(garage, GarageCategory::armor, item.name, "Best"), error);
        armored = true;
        break;
    }
    require(armored, "No original armor purchase");
    // With missing internal and all side HP, the most expensive available repair is full/all.
    GarageSelection repair;
    std::int32_t highest_price{-1};
    const auto repairs = garage.items(GarageCategory::repair);
    for (std::size_t item = 0; item < repairs.size(); ++item)
        for (std::size_t product = 0; product < repairs[item].products.size(); ++product) {
            GarageQuote quote;
            const GarageSelection candidate{GarageCategory::repair, item, product};
            require(garage.quote(candidate, quote, error) == Result::ok, error);
            if (quote.action == GarageAction::repair && quote.price > highest_price) {
                repair = candidate;
                highest_price = quote.price;
            }
        }
    require(highest_price > 0, "Damaged fixture has no paid repair");
    transact(garage, repair, error);
    require(garage.state().condition.internal_hp == garage.state().condition.maximum_internal_hp &&
                garage.state().condition.armor_hp == garage.state().condition.maximum_armor_hp,
            "Full garage repair did not restore actual condition");
    bool painted{};
    for (std::size_t item = 0; item < garage.items(GarageCategory::paint).size(); ++item) {
        GarageQuote quote;
        const GarageSelection selection{GarageCategory::paint, item, 0};
        require(garage.quote(selection, quote, error) == Result::ok, error);
        if (quote.action == GarageAction::buy) {
            transact(garage, selection, error);
            painted = true;
            break;
        }
    }
    require(painted && garage.state().loadout.paint_job > 0, "No original paint purchase");
}
void verify(const RaceSession& session, const GarageVehicleState& expected) {
    require(session.racers().size() == 6, "Original Single must initialize five opponents");
    const auto& racer = session.racers()[session.player_index()];
    const auto& vehicle = *racer.vehicle;
    const auto& actual = racer.loadout;
    require(actual.weapons == expected.loadout.weapons &&
                actual.paint_job == expected.loadout.paint_job &&
                actual.tires.type == expected.loadout.tires.type &&
                actual.tires.quality == expected.loadout.tires.quality &&
                actual.armor.type == expected.loadout.armor.type &&
                actual.armor.quality == expected.loadout.armor.quality,
            "Race changed the committed garage equipment");
    require(vehicle.parameters().engine_upgrades == expected.condition.engine_upgrades &&
                vehicle.equipment().engine_upgrades == expected.condition.engine_upgrades &&
                std::any_of(expected.condition.engine_upgrades.begin(),
                            expected.condition.engine_upgrades.end(),
                            [](auto value) { return value != 0; }),
            "Committed drivetrain did not reach actual physics parameters");
    require(vehicle.parameters().tires.type == actual.tires.type &&
                vehicle.parameters().tires.quality == actual.tires.quality,
            "Committed tires did not reach actual physics parameters");
    require(vehicle.equipment().maximum_armor_hp == expected.condition.maximum_armor_hp &&
                vehicle.equipment().armor_hp == expected.condition.armor_hp &&
                vehicle.equipment().internal_hp == expected.condition.internal_hp,
            "Committed armor/repair differs from vehicle health");
    require(vehicle.combat() && vehicle.damage_visuals(), "Missing actual combat or damage owner");
    for (std::size_t slot = 0; slot < 3; ++slot) {
        const auto weapon = vehicle.combat()->slot(static_cast<WeaponSlot>(slot));
        require(weapon.descriptor &&
                    ResourceStore::canonical(weapon.descriptor->name) ==
                        ResourceStore::canonical(actual.weapons[slot]) &&
                    weapon.ammunition == expected.weapon_ammunition[slot],
                "Committed weapon/ammunition did not reach actual combat owner: slot=" +
                    std::to_string(slot) + " selected=" + actual.weapons[slot] + " actual=" +
                    (weapon.descriptor ? std::string(weapon.descriptor->name) : "<null>") +
                    " expected_ammo=" + std::to_string(expected.weapon_ammunition[slot]) +
                    " actual_ammo=" + std::to_string(weapon.ammunition));
    }
    const auto expected_paint = "/PAINT/PAINT JOB " + std::to_string(actual.paint_job) + "/";
    require(
        ResourceStore::canonical(vehicle.damage_visuals()->texture_path()).find(expected_paint) !=
            std::string::npos,
        "Committed paint did not reach actual damage texture owner");
}
void run(const std::filesystem::path& root) {
    Files files(root);
    ResourceStore resources(files);
    require(resources.open() == Result::ok, "Cannot open original asset archive");
    const auto characters = selections(resources, "Screens/SelectCharacter/Data.ini");
    const auto tracks = selections(resources, "Screens/SelectTrack/Data.ini");
    std::map<std::string, VehicleParameters> parameters;
    std::string error;
    for (const auto& character : characters) {
        auto& value = parameters[character];
        require(load_vehicle_metrics(resources, character, value, error) == Result::ok, error);
        require(load_vehicle_geometry(resources, character, value, error) == Result::ok, error);
    }
    std::size_t combinations{};
    for (const auto& track : tracks) {
        // Collision is immutable while sessions borrow it; cache exactly once per track.
        std::vector<std::byte> bytes;
        MeshObject world;
        TrackCollision collision;
        Configuration config;
        require(resources.read("Tracks/" + track + "/World/SpatialObject.msh", bytes) == Result::ok,
                "Cannot read original world: " + track);
        require(decode_msh(bytes, world, error) == Result::ok, error);
        require(collision.load(resources, track, world, error) == Result::ok, error);
        require(config.load(resources, "Tracks/" + track + "/World/Data.txt") == Result::ok,
                "Cannot load original track configuration");
        float laps{};
        require(config.number("DATA", "NUMLAPS", laps) == Result::ok && laps >= 1 && laps <= 10 &&
                    std::floor(laps) == laps,
                "Invalid original track lap count");
        for (const auto& character : characters) {
            const auto context = character + "/" + track;
            GarageSession garage(resources);
            require(garage.start_single(character, track, error) == Result::ok,
                    context + ": " + error);
            upgrade(garage, track, error);
            RaceOptions options;
            options.car = character;
            options.track = track;
            options.laps = static_cast<int>(laps);
            options.player_loadout = garage.race_loadout();
            VehicleDynamics player;
            require(player.initialize(parameters.at(character), collision, error) == Result::ok,
                    context + ": " + error);
            RaceSession session;
            require(session.load(resources, collision, options, player, error) == Result::ok,
                    context + ": " + error);
            verify(session, garage.state());
            for (unsigned tick = 0; session.phase() == RacePhase::countdown && tick < 1200; ++tick)
                require(session.advance(1.f / 60, {}, error) == Result::ok, context + ": " + error);
            require(session.phase() == RacePhase::racing, context + ": countdown never ended");
            require(session.advance(1.f / 60, {}, error) == Result::ok && session.ticks() > 0,
                    context + ": first racing tick failed: " + error);
            require(std::isfinite(player.state().position.x) &&
                        std::isfinite(player.state().position.y) &&
                        std::isfinite(player.state().position.z),
                    context + ": nonfinite physics");
            require(session.restart(error) == Result::ok &&
                        session.phase() == RacePhase::countdown && session.ticks() == 0,
                    context + ": restart failed: " + error);
            verify(session, garage.state());
            ++combinations;
            std::cout << "SINGLE_CATALOG_CASE character=" << character << " track=" << track
                      << " laps=" << options.laps << " opponents=5 countdown_tick_restart=ok\n";
        }
    }
    require(combinations == characters.size() * tracks.size() && combinations > 0,
            "Character/track matrix is incomplete");
    std::cout << "SINGLE_CATALOG_OK characters=" << characters.size() << " tracks=" << tracks.size()
              << " combinations=" << combinations << " completed_races=0\n";
}
} // namespace
int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "SINGLE_CATALOG_SKIPPED external installation argument required\n";
        return 77;
    }
    try {
        run(argv[1]);
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "SINGLE_CATALOG_FAILED " << exception.what() << '\n';
        return 1;
    }
}
