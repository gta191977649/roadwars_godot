#include "game_src/unresolved/garage_session.hpp"
#include "game_src/unresolved/race_music.hpp"
#include "game_src/unresolved/vehicle_dynamics.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <source_location>
#include <stdexcept>

using namespace roadwars;
namespace {
void require(bool condition, std::string message,
             std::source_location location = std::source_location::current()) {
    if (!condition)
        throw std::runtime_error(message + " [line " + std::to_string(location.line()) + "]");
}
class Memory final : public ResourceReader {
  public:
    std::map<std::string, std::string> files;
    void put(std::string_view path, std::string text) {
        files[ResourceStore::canonical(path)] = std::move(text);
    }
    Result read(std::string_view path, std::vector<std::byte>& output) override {
        const auto found = files.find(ResourceStore::canonical(path));
        if (found == files.end())
            return Result::not_found;
        output.clear();
        for (unsigned char c : found->second)
            output.push_back(static_cast<std::byte>(c));
        return Result::ok;
    }
};
class Files final : public ResourceReader {
  public:
    explicit Files(std::filesystem::path path) : root(std::move(path)) {}
    Result read(std::string_view path, std::vector<std::byte>& output) override {
        std::ifstream file(root / std::filesystem::path(path), std::ios::binary | std::ios::ate);
        if (!file)
            return Result::not_found;
        const auto count = file.tellg();
        if (count < 0)
            return Result::invalid_argument;
        output.resize(static_cast<std::size_t>(count));
        file.seekg(0);
        file.read(reinterpret_cast<char*>(output.data()), count);
        return file ? Result::ok : Result::invalid_argument;
    }
    std::filesystem::path root;
};
void ordering(Memory& memory, const std::string& path,
              std::initializer_list<std::string_view> names) {
    std::string text = "[ORDERING]\nNUMITEMS=" + std::to_string(names.size()) + "\n";
    int i{};
    for (const auto name : names)
        text += "ITEM" + std::to_string(i++) + "=" + std::string(name) + "\n";
    memory.put(path + "/Data.ini", text);
}
void price(Memory& memory, const std::string& path, int amount, std::string extra = "") {
    memory.put(path + "/Data.ini",
               "[PRICE]\nPRICE($)=" + std::to_string(amount) +
                   "\nISAVAILABLE=1\nDEFAULTAMMO=12\n[DESCRIPTION]\nTEXT=Get #ammo rounds\n" +
                   extra);
}
void fixture(Memory& memory) {
    memory.put("Characters/Test/Data.ini",
               "[INFO]\nVEHICLE=Test\n[PRICE_SCALARS]\nTIRES=.75\nDRIVETRAIN=.75\n"
               "REPAIR=.99\nWEAPONS=.75\nARMOR=.75\nSPIKES=.5\nNITRO=1\nTRACTION=1\n"
               "INTERNAL_REPAIR_PER_SEC=0\n");
    memory.put("Cars/Test/Data/Data.ini",
               "[DAMAGE]\nINTERNALHP=100\nFRONTHP=100\nRIGHTHP=100\nREARHP=100\nLEFTHP=100\n");
    memory.put("Tracks/Test/World/Data.txt", "[AI]\nTIRES=OFF ROAD\n");
    const std::string base = "Cars/Shared/";
    ordering(memory, base + "Tires", {"Street", "Off Road"});
    for (const auto item : {"Street", "Off Road"}) {
        const auto path = base + "Tires/" + item;
        ordering(memory, path, {"Poor", "Best"});
        price(memory, path + "/Poor", 101);
        price(memory, path + "/Best", 401);
    }
    ordering(memory, base + "Drive Train", {"Air Filter", "Nitrous Boost"});
    for (const auto item : {"Air Filter", "Nitrous Boost"}) {
        const auto path = base + "Drive Train/" + item;
        ordering(memory, path, {"Poor", "Best"});
        price(memory, path + "/Poor", 101);
        price(memory, path + "/Best", 201);
    }
    ordering(memory, base + "Weapons", {"Front", "Side", "Rear"});
    for (const auto item : {"Front", "Side", "Rear"}) {
        const auto path = base + "Weapons/" + item;
        ordering(memory, path, {"Machine Gun", "Battering Spikes"});
        price(memory, path + "/Machine Gun", 101);
        price(memory, path + "/Battering Spikes", 101);
    }
    ordering(memory, base + "Armor", {"Steel"});
    ordering(memory, base + "Armor/Steel", {"Poor", "Best"});
    price(memory, base + "Armor/Steel/Poor", 101, "[HP]\nHPSCALE=.25\n");
    price(memory, base + "Armor/Steel/Best", 201, "[HP]\nHPSCALE=1\n");
    ordering(memory, base + "Repair",
             {"All Damage", "Internal Damage", "Front Damage", "Right Damage", "Rear Damage",
              "Left Damage"});
    for (const auto item : {"All Damage", "Internal Damage", "Front Damage", "Right Damage",
                            "Rear Damage", "Left Damage"}) {
        const auto path = base + "Repair/" + item;
        ordering(memory, path, {"Minor", "Medium", "Major", "Complete"});
        for (const auto quality : {"Minor", "Medium", "Major", "Complete"})
            price(memory, path + "/" + quality, 10);
    }
    ordering(memory, base + "Paint Job", {"Paint Job 1", "Paint Job 2"});
    price(memory, base + "Paint Job/Paint Job 1", 0);
    price(memory, base + "Paint Job/Paint Job 2", 0);
}
void contracts() {
    Memory memory;
    fixture(memory);
    ResourceStore resources(memory);
    GarageSession garage(resources);
    std::string error;
    require(garage.start_single("Test", "Test", error) == Result::ok, error);
    require(garage.state().cash == 25000 && garage.state().loadout.paint_job == 0,
            "Original fresh Single configuration");
    auto invalid_identity = garage.state();
    invalid_identity.vehicle = "AnotherVehicle";
    require(garage.restore(invalid_identity, "Test", error) == Result::invalid_argument &&
                garage.state().vehicle == "Test" && garage.state().cash == 25000,
            "Restore rejects a mismatched vehicle identity without changing the session");
    GarageQuote quote;
    GarageSelection drive{GarageCategory::drivetrain, 0, 0};
    {
        GarageSession tiny_repair = garage;
        auto tiny = garage.state();
        tiny.cash = 0;
        tiny.condition.internal_hp = 99.99f;
        require(tiny_repair.restore(tiny, "Test", error) == Result::ok, error);
        GarageQuote free_quote;
        require(tiny_repair.quote({GarageCategory::repair, 1, 0}, free_quote, error) ==
                        Result::ok &&
                    free_quote.price == 0 && free_quote.action == GarageAction::repair,
                "Rounded zero-price repair remains executable");
        bool changed{};
        require(tiny_repair.transact({GarageCategory::repair, 1, 0}, true, changed, error) ==
                        Result::ok &&
                    changed && tiny_repair.state().cash == 0 &&
                    tiny_repair.state().condition.internal_hp > tiny.condition.internal_hp &&
                    tiny_repair.take_sound_events() ==
                        std::vector<std::string>{"ModifyVehicle/buy.wav"},
                "Free repair restores fractional missing HP and emits purchase sound");
        require(tiny_repair.restore(garage.state(), "Test", error) == Result::ok, error);
        require(tiny_repair.transact({GarageCategory::repair, 0, 3}, true, changed, error) ==
                        Result::ok &&
                    !changed && tiny_repair.take_sound_events().empty(),
                "Fully healthy repair is silent and leaves undamaged state unchanged");
    }
    auto warning_state = garage.state();
    for (const auto [funds, warned] : {std::pair{100, false}, std::pair{101, true},
                                       std::pair{475, true}, std::pair{476, false}}) {
        warning_state.cash = funds;
        require(garage.restore(warning_state, "Test", error) == Result::ok, error);
        require(garage.quote(drive, quote, error) == Result::ok &&
                    (!quote.confirmations.empty()) == warned,
                "Tire warning uses raw 101/401 prices and strict post-purchase boundary");
    }
    warning_state.cash = 25000;
    require(garage.restore(warning_state, "Test", error) == Result::ok, error);
    require(garage.quote(drive, quote, error) == Result::ok && quote.price == 75,
            "Price truncation reference 101*.75");
    bool changed{};
    require(garage.transact(drive, true, changed, error) == Result::ok && changed &&
                garage.state().cash == 24925 && garage.state().condition.engine_upgrades[0] == 2,
            "Install drivetrain and debit once");
    require(garage.quote(drive, quote, error) == Result::ok && quote.action == GarageAction::sell &&
                quote.trade_in == 37 && quote.balance_change == 37,
            "Resale truncates half of truncated character price");
    const auto cash = garage.state().cash;
    require(garage.transact(drive, false, changed, error) == Result::ok && !changed &&
                garage.state().cash == cash,
            "Unconfirmed sale preserves state");
    require(garage.transact(drive, true, changed, error) == Result::ok && changed &&
                garage.state().cash == cash + 37 && garage.state().loadout.drivetrain.empty() &&
                garage.state().condition.engine_upgrades[0] == 0,
            "Sale clears effect and returns trade value");
    GarageSelection weapon{GarageCategory::weapons, 0, 1};
    require(garage.quote(weapon, quote, error) == Result::ok && quote.price == 37 &&
                quote.description == "Get 12 rounds",
            "Contact weapon scalar and #ammo substitution");
    require(garage.transact(weapon, true, changed, error) == Result::ok && changed &&
                garage.state().loadout.weapons[0] == "Battering Spikes" &&
                garage.state().weapon_ammunition[0] == 12,
            "Weapon install retains slot and ammo");
    {
        GarageSession replacement = garage;
        const auto original_cash = replacement.state().cash;
        const GarageSelection machine_gun{GarageCategory::weapons, 0, 0};
        require(replacement.quote(machine_gun, quote, error) == Result::ok &&
                    quote.action == GarageAction::buy && quote.trade_in == 18 &&
                    quote.balance_change == -57 && !quote.confirmations.empty(),
                "Weapon replacement quotes the installed contact weapon trade-in");
        require(replacement.transact(machine_gun, false, changed, error) == Result::ok &&
                    !changed && replacement.state().cash == original_cash &&
                    replacement.state().loadout.weapons[0] == "Battering Spikes",
                "Declined weapon replacement preserves funds and equipment");
        require(replacement.transact(machine_gun, true, changed, error) == Result::ok && changed &&
                    replacement.state().cash == original_cash - 57 &&
                    replacement.state().loadout.weapons[0] == "Machine Gun" &&
                    replacement.state().condition.front_weapon == "Machine Gun",
                "Confirmed replacement commits one trade and one purchase");
        require(replacement.quote(machine_gun, quote, error) == Result::ok &&
                    quote.action == GarageAction::sell && quote.trade_in == 37,
                "Selecting the installed weapon offers sell-back");
        require(replacement.transact(machine_gun, true, changed, error) == Result::ok && changed &&
                    replacement.state().cash == original_cash - 20 &&
                    replacement.state().loadout.weapons[0].empty() &&
                    replacement.state().condition.front_weapon.empty() &&
                    replacement.state().weapon_ammunition[0] == 0,
                "Weapon sell-back clears actual slot effects and ammunition");
    }
    require(garage.quote({GarageCategory::tires, 0, 0}, quote, error) == Result::ok &&
                quote.action == GarageAction::installed,
            "Installed tires cannot be sold");
    auto state = garage.state();
    state.condition.internal_hp = 60;
    require(garage.restore(state, "Test", error) == Result::ok, error);
    GarageSelection repair{GarageCategory::repair, 1, 0};
    require(garage.quote(repair, quote, error) == Result::ok && quote.price == 390,
            "Repair unit truncation .99*40=39");
    require(garage.transact(repair, true, changed, error) == Result::ok && changed &&
                garage.state().condition.internal_hp == 70,
            "Minor repairs a quarter of missing HP");
    require(garage.quote(repair, quote, error) == Result::ok && quote.price == 293,
            "Repeated minor diminishes and rounds 292.5 to 293");
    state = garage.state();
    state.cash = 292;
    require(garage.restore(state, "Test", error) == Result::ok, error);
    require(garage.transact(repair, true, changed, error) == Result::ok && !changed &&
                garage.state().cash == 292 && garage.state().condition.internal_hp == 70 &&
                error == "YOU DO NOT HAVE ENOUGH MONEY!",
            "Insufficient funds change nothing");
    state.cash = 293;
    require(garage.restore(state, "Test", error) == Result::ok, error);
    require(garage.transact(repair, true, changed, error) == Result::ok && changed &&
                garage.state().cash == 0 && garage.state().condition.internal_hp == 77.5f,
            "Exact repair balance accepted");
    state = garage.state();
    state.cash = 25000;
    state.condition.armor_hp = {90, 80, 70, 60};
    require(garage.restore(state, "Test", error) == Result::ok, error);
    require(garage.transact({GarageCategory::repair, 3, 1}, true, changed, error) == Result::ok &&
                garage.state().condition.armor_hp == std::array<float, 4>{90, 90, 70, 60},
            "Right side repair does not touch other sides");
    require(garage.transact({GarageCategory::armor, 0, 0}, true, changed, error) == Result::ok &&
                changed && garage.state().condition.armor_hp[0] == 125,
            "Armor installation follows sell-stock-reset then rescale");
    state = garage.state();
    state.condition.armor_hp = {62.5f, 62.5f, 62.5f, 62.5f};
    require(garage.restore(state, "Test", error) == Result::ok, error);
    require(garage.quote({GarageCategory::armor, 0, 0}, quote, error) == Result::ok &&
                quote.trade_in == 19,
            "Damaged armor resale rounds 37*.5+.5");
    require(garage.transact({GarageCategory::paint, 1, 0}, true, changed, error) == Result::ok &&
                changed && garage.state().loadout.paint_job == 2,
            "Paint selection reaches race configuration");
    require(garage.quote({GarageCategory::paint, 1, 0}, quote, error) == Result::ok &&
                quote.action == GarageAction::installed,
            "Current paint cannot be bought again");
    GarageQuote untouched;
    untouched.price = 42;
    require(garage.quote({GarageCategory::weapons, 50, 0}, untouched, error) ==
                    Result::invalid_argument &&
                untouched.price == 42,
            "Invalid selection rejected before indexing");
    resources.set_configuration_override("Cars/Shared/Weapons/Front/Machine Gun/Data.ini", "PRICE",
                                         "ISAVAILABLE", "0");
    require(garage.restore(garage.state(), "Test", error) == Result::ok, error);
    require(garage.quote({GarageCategory::weapons, 0, 0}, quote, error) == Result::ok &&
                quote.action == GarageAction::unavailable,
            "Unavailable original products cannot be bought");
}
void installed_assets(const std::filesystem::path& root) {
    Files files(root);
    ResourceStore resources(files);
    require(resources.open() == Result::ok, "Open original assets");
    for (const auto path :
         {"ModifyVehicle/buy.wav", "ModifyVehicle/select.wav", "ModifyVehicle/door.wav"}) {
        std::vector<std::byte> bytes;
        WavePcm sample;
        require(resources.read(path, bytes) == Result::ok &&
                    (decode_wave_pcm(bytes, sample) == Result::ok ||
                     decode_ms_adpcm_wave(bytes, sample) == Result::ok) &&
                    sample.frame_count() > 0,
                "Decode the actual Garage sound formats");
    }
    Configuration characters, tracks;
    require(characters.load(resources, "Screens/SelectCharacter/Data.ini") == Result::ok &&
                tracks.load(resources, "Screens/SelectTrack/Data.ini") == Result::ok,
            "Load original selection tables");
    float count{}, track_count{};
    require(characters.number("INFO", "NUMSELECTIONS", count) == Result::ok && count == 12,
            "Twelve original characters");
    require(tracks.number("INFO", "NUMSELECTIONS", track_count) == Result::ok,
            "Original track count");
    std::string error;
    std::size_t products{};
    for (int c = 0; c < count; ++c) {
        const auto character = characters.value("SELECTION" + std::to_string(c), "VALUE");
        for (int t = 0; t < track_count; ++t) {
            const auto track = tracks.value("SELECTION" + std::to_string(t), "VALUE");
            float enabled{};
            if (tracks.number("SELECTION" + std::to_string(t), "ISAVAILABLE", enabled) !=
                    Result::ok ||
                enabled == 0)
                continue;
            GarageSession garage(resources);
            require(garage.start_single(character, track, error) == Result::ok,
                    character + "/" + track + ": " + error);
            const auto fresh = garage.state();
            auto funded = fresh;
            funded.cash = 2000000;
            funded.condition.internal_hp *= .5f;
            for (auto& hp : funded.condition.armor_hp)
                hp *= .5f;
            require(garage.restore(funded, track, error) == Result::ok, error);
            for (int cat = 0; cat < 6; ++cat) {
                const auto category = static_cast<GarageCategory>(cat);
                const auto items = garage.items(category);
                for (std::size_t i = 0; i < items.size(); ++i)
                    for (std::size_t j = 0; j < items[i].products.size(); ++j) {
                        GarageQuote quote;
                        require(garage.quote({category, i, j}, quote, error) == Result::ok, error);
                        require(quote.price >= -1, "Valid original quote");
                        GarageSession purchase = garage;
                        bool changed{};
                        require(purchase.transact({category, i, j}, true, changed, error) ==
                                    Result::ok,
                                character + "/" + track + "/" + items[i].name + ": " + error);
                        require(
                            changed == (quote.action == GarageAction::buy ||
                                        quote.action == GarageAction::sell ||
                                        quote.action == GarageAction::repair),
                            "Every available original product executes its real business action");
                        require(purchase.state().cash == funded.cash + quote.balance_change,
                                "Original quote and transaction balance agree");
                        ++products;
                    }
            }
            require(garage.state().cash == funded.cash, "Browsing cannot mutate vehicle funds");
            if (t == 0) {
                VehicleDamageVisuals visuals;
                VehicleParameters geometry;
                const auto visual_result = visuals.load(resources, character, 0, error);
                require(visual_result == Result::ok, character + ": " + error);
                const auto metrics_result =
                    load_vehicle_metrics(resources, character, geometry, error);
                require(metrics_result == Result::ok, character + ": " + error);
                const auto geometry_result =
                    load_vehicle_geometry(resources, character, geometry, error);
                require(geometry_result == Result::ok, character + ": " + error);
                require(visuals.mark_pixel_pair(0, 0, {0, 0, 100, 100}, {0, 0}) == Result::ok,
                        "Original two-pixel texture damage producer");
                auto damaged = funded;
                damaged.body_width = geometry.width;
                damaged.body_length = geometry.length;
                damaged.visuals = visuals.capture_state();
                require(garage.restore(damaged, track, error) == Result::ok, error);
                bool changed{};
                require(garage.transact({GarageCategory::paint, 1, 0}, true, changed, error) ==
                                Result::ok &&
                            changed && garage.state().visuals &&
                            !garage.state().visuals->pixel_pairs.empty(),
                        "Garage repaint replays existing damage on new livery");
                require(garage.transact({GarageCategory::repair, 0, 3}, true, changed, error) ==
                                Result::ok &&
                            changed && garage.state().visuals->pixel_pairs.empty() &&
                            garage.state().condition.internal_hp ==
                                garage.state().condition.maximum_internal_hp,
                        "Garage complete repair restores health and actual damage pixels");
                require(visuals.load(resources, character, 2, error) == Result::ok &&
                            visuals.mark_pixel_pair(0, 0, {0, 0, 100, 100}, {0, 0}) == Result::ok,
                        "Prepare visual-only damage with fully healthy equipment");
                auto healthy = garage.state();
                healthy.visuals = visuals.capture_state();
                require(garage.restore(healthy, track, error) == Result::ok, error);
                require(garage.transact({GarageCategory::repair, 0, 0}, true, changed, error) ==
                                Result::ok &&
                            changed && garage.state().visuals->pixel_pairs.empty() &&
                            garage.state().cash == healthy.cash &&
                            garage.take_sound_events().empty(),
                        "Healthy free repair still restores paint and emits no purchase sound");
            }
        }
    }
    std::cout << "GARAGE_ASSETS_OK quotes=" << products << '\n';
}
} // namespace
int main(int argc, char** argv) {
    try {
        contracts();
        if (argc > 1)
            installed_assets(argv[1]);
        std::cout << "GARAGE_SESSION_OK\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
