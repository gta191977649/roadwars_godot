#include "game_src/unresolved/vehicle_equipment_effects.hpp"
#include <cassert>
#include <cmath>
#include <limits>
#include <map>

using namespace roadwars;
namespace {
bool close(float a, float b) { return std::abs(a - b) < .0001f; }
VehicleEquipmentEffects stock() {
    VehicleEquipmentEffects state;
    state.internal_hp = state.maximum_internal_hp = 100;
    state.armor_hp = state.maximum_armor_hp = state.base_armor_hp = {100, 80, 60, 40};
    return state;
}
struct Files final : ResourceReader {
    std::map<std::string, std::string> files;
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
} // namespace
int main() {
    std::int32_t upgrade = -1;
    assert(drivetrain_upgrade("Air Filter", "POOR", upgrade) == Result::ok && upgrade == 2);
    assert(drivetrain_upgrade("CAM SHAFT", "GOOD", upgrade) == Result::ok && upgrade == 9);
    assert(drivetrain_upgrade("EXHAUST", "GOOD", upgrade) == Result::ok && upgrade == 7);
    assert(drivetrain_upgrade("SUPER CHARGER", "BEST", upgrade) == Result::ok && upgrade == 30);
    assert(drivetrain_upgrade("NITROUS BOOST", "BEST", upgrade) == Result::ok && upgrade == 0);
    assert(drivetrain_upgrade("INVENTED", "BEST", upgrade) == Result::invalid_argument);
    assert(drivetrain_upgrade("AIR FILTER", "INVENTED", upgrade) == Result::invalid_argument);

    auto state = stock();
    state.armor_hp[0] = 25;
    assert(rescale_vehicle_armor(state, .5f) == Result::ok);
    assert(state.maximum_armor_hp[0] == 150 && state.armor_hp[0] == 37.5f);
    assert(state.maximum_armor_hp[2] == 90 && state.armor_hp[2] == 90);
    assert(rescale_vehicle_armor(state, 1) == Result::ok);
    assert(state.maximum_armor_hp[0] == 200 && state.armor_hp[0] == 50);
    const auto before = state;
    assert(rescale_vehicle_armor(state, -1) == Result::invalid_argument);
    assert(state.armor_hp == before.armor_hp && state.maximum_armor_hp == before.maximum_armor_hp);

    state = stock();
    assert(damage_vehicle_contact(state, 1, 80) == Result::ok);
    assert(state.armor_hp[2] == -20 && state.internal_hp == 100);
    assert(damage_vehicle_contact(state, 1, 30) == Result::ok);
    assert(state.internal_hp == 70 && state.armor_hp[2] == -20);
    assert(damage_vehicle_contact(state, 2, 10) == Result::ok && state.armor_hp[3] == 30);
    assert(damage_vehicle_contact(state, 3, 10) == Result::ok && state.armor_hp[1] == 70);
    state.front_weapon = "Battering Spikes";
    assert(damage_vehicle_contact(state, 0, 20) == Result::ok && state.armor_hp[0] == 90);
    state.front_weapon = "Hydraulic Ram";
    assert(damage_vehicle_contact(state, 0, 20) == Result::ok && state.armor_hp[0] == 85);
    assert(damage_vehicle_armor(state, 0, 100, {true, false, true}) == Result::ok);
    assert(state.armor_hp[0] == 85);
    assert(damage_vehicle_internal(state, 100, {false, true, true}) == Result::ok);
    assert(state.internal_hp == 70);
    assert(damage_vehicle_internal(state, 100, {false, false, false}) == Result::ok);
    assert(state.internal_hp == 70);
    assert(damage_vehicle_internal(state, 100) == Result::ok && state.internal_hp == 0);
    assert(damage_vehicle_contact(state, 4, 10) == Result::invalid_argument);
    assert(damage_vehicle_internal(state, -1) == Result::invalid_argument);

    state = stock();
    assert(!activate_vehicle_nitrous(state));
    state.nitrous_ammo = 2;
    state.nitrous_duration_scalar = 1.25f;
    assert(activate_vehicle_nitrous(state) && state.nitrous_ammo == 1);
    assert(state.nitrous_timer == .01f && !activate_vehicle_nitrous(state));
    NitrousStep step;
    assert(advance_vehicle_nitrous(state, .5f, {0, 0, 2}, 3, step) == Result::ok);
    assert(step.emitting && !step.stopped && close(step.velocity_delta.z, 6));
    state.nitrous_timer = 3.75f;
    assert(advance_vehicle_nitrous(state, 0, {0, 0, 1}, 0, step) == Result::ok);
    assert(!step.emitting && !step.stopped && state.nitrous_timer == 3.75f);
    assert(advance_vehicle_nitrous(state, .01f, {0, 0, 1}, 1, step) == Result::ok);
    assert(!step.emitting && !step.stopped && state.nitrous_timer > 3.75f);
    assert(advance_vehicle_nitrous(state, .01f, {0, 0, 1}, 0, step) == Result::ok);
    assert(step.stopped && state.nitrous_timer == 0);
    state.nitrous_timer = 3.74f;
    bool emitter_called{};
    const auto actual_emitter = [&](const NitrousStep& current, std::size_t& count) {
        assert(current.emitting && close(current.velocity_delta.z, .24f));
        assert(state.nitrous_timer == 3.74f); // Emission precedes timer increment.
        count = 0;
        emitter_called = true;
        return Result::ok;
    };
    assert(advance_vehicle_nitrous(state, .02f, {0, 0, 1}, actual_emitter, step) == Result::ok);
    assert(emitter_called && step.stopped && state.nitrous_timer == 0);
    assert(activate_vehicle_nitrous(state) && state.nitrous_ammo == 0);
    const float timer = state.nitrous_timer;
    assert(advance_vehicle_nitrous(state, .1f, {}, 0, step) == Result::invalid_argument);
    assert(state.nitrous_timer == timer);
    assert(advance_vehicle_nitrous(state, std::numeric_limits<float>::quiet_NaN(), {0, 0, 1}, 0,
                                   step) == Result::invalid_argument);
    stop_vehicle_nitrous(state);
    assert(state.nitrous_timer == 0 && !activate_vehicle_nitrous(state));

    Files files;
    files.files["CARS/TEST/DATA/DATA.INI"] =
        "[DAMAGE]\nFRONTHP=100\nRIGHTHP=80\nREARHP=60\nLEFTHP=40\nINTERNALHP=120\n";
    files.files["CHARACTERS/TEST/DATA.INI"] =
        "[PRICE_SCALARS]\nNITRO=1.25\nINTERNAL_REPAIR_PER_SEC=0\nTRACTION=1.5\n";
    files.files["CARS/SHARED/ARMOR/STEEL/BEST/DATA.INI"] = "[HP]\nHPSCALE=.5\n";
    files.files["CARS/SHARED/DRIVE TRAIN/NITROUS BOOST/BEST/DATA.INI"] =
        "[PRICE]\nDEFAULTAMMO=10\n";
    ResourceStore resources(files);
    const std::array<std::pair<std::string, std::string>, 3> drivetrain{
        std::pair{"Air Filter", "BEST"}, std::pair{"EXHAUST", "GOOD"},
        std::pair{"NITROUS BOOST", "BEST"}};
    std::string error;
    assert(load_vehicle_equipment_effects(resources, "Test", "Test", "Steel", "BEST", drivetrain,
                                          "Hydraulic Ram", state, error) == Result::ok);
    assert(state.internal_hp == 120 && state.maximum_armor_hp[0] == 150);
    assert(state.traction_multiplier == 1.5f);
    assert(state.engine_upgrades[0] == 4 && state.engine_upgrades[3] == 7);
    assert(state.nitrous_ammo == 10 && state.nitrous_duration_scalar == 1.25f);
    state.internal_hp = 110;
    state.internal_repair_per_second = 4;
    assert(advance_vehicle_health(state, 1) == Result::ok && state.internal_hp == 114);
    assert(advance_vehicle_health(state, 10) == Result::ok && state.internal_hp == 120);
    state.internal_hp = 0;
    assert(advance_vehicle_health(state, .5f) == Result::ok && state.internal_hp == 0 &&
           state.dying_timer == .5f); // The original repair ability never revives a destroyed car.
    state.internal_hp = 1;
    assert(advance_vehicle_health(state, .25f) == Result::ok && state.dying_timer == 0);
    const auto stable_hp = state.internal_hp;
    assert(advance_vehicle_health(state, -1) == Result::invalid_argument &&
           state.internal_hp == stable_hp);
    files.files["CARS/SHARED/DRIVE TRAIN/NITROUS BOOST/BEST/DATA.INI"] =
        "[PRICE]\nDEFAULTAMMO=-1\n";
    assert(load_vehicle_equipment_effects(resources, "Test", "Test", "Steel", "BEST", drivetrain,
                                          "Hydraulic Ram", state,
                                          error) == Result::invalid_argument);
    assert(state.nitrous_ammo == 10); // Failed loads never replace the live state.
}
