#include "game_src/unresolved/contact_weapon_runtime.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>
using namespace roadwars;
namespace {
void require(bool value) {
    if (!value)
        throw std::runtime_error("Original contact weapon contract mismatch");
}
} // namespace
int main() {
    std::string error;
    ContactWeaponRuntime spikes, ram;
    require(contact_weapon_front_hit({0, 0, 1}, {}, {0, 0, 10}));
    require(!contact_weapon_front_hit({0, 0, 1}, {}, {10, 0, 0}));
    require(!contact_weapon_front_hit({0, 0, 1}, {}, {0, 0, -10}));
    require(!contact_weapon_front_hit({0, 0, 1}, {}, {0, 20, 1}));
    require(spikes.initialize("Battering Spikes", 2, error) == Result::ok);
    Vec3 output;
    require(spikes.modify_contact_force({0, 0, 10}, output) == Result::ok && output.z == 1010);
    require(spikes.modify_contact_force({0, -10, 0}, output) == Result::ok && output.y == -1010);
    require(spikes.modify_contact_force({}, output) == Result::ok && output.x == 0 &&
            output.z == 0);
    require(spikes.front_extension() == 2 && spikes.ready());
    require(ram.initialize("Hydraulic Ram", 2, error) == Result::ok);
    require(ram.modify_contact_force({0, 0, 10}, output) == Result::ok && output.z == 1010);
    require(ram.begin({{}, 0, 0, 30}) && !ram.ready());
    require(ram.modify_contact_force({0, 0, 10}, output) == Result::ok && output.z == 2010);
    require(ram.advance(.5f) == Result::ok && ram.pose().local_z == 1 && ram.pose().extending);
    require(ram.front_extension() == 2); // The collision extension is not the ram animation offset.
    require(ram.advance(.01f) == Result::ok && ram.pose().local_z == 1 && !ram.pose().extending);
    require(ram.modify_contact_force({0, 0, 10}, output) == Result::ok && output.z == 1010);
    require(ram.advance(.5f) == Result::ok && ram.pose().local_z == 0 && ram.ready());
    require(ram.begin({{}, 0, 0, 30}));
    require(ram.advance(.1f) == Result::ok && ram.pose().local_z == .2f);
    require(!ram.begin({{}, 0, 0, 30}) && ram.pose().local_z == 0);
    require(ram.advance(.1f) == Result::ok && ram.pose().local_z == .2f);
    ram.cancel();
    require(ram.ready());
    require(ram.advance(.1f) == Result::ok && ram.pose().local_z == .4f);
    require(ram.advance(std::numeric_limits<float>::quiet_NaN()) == Result::invalid_argument);
    require(ram.initialize("Machine Gun", 2, error) == Result::unavailable);
    require(ram.initialize("Hydraulic Ram", 0, error) == Result::invalid_argument);
    require(ram.modify_contact_force({std::numeric_limits<float>::infinity(), 0, 0}, output) ==
            Result::invalid_argument);
}
