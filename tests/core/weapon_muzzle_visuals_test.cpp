#include "game_src/unresolved/weapon_projectiles.hpp"
#include <cassert>
#include <cmath>
using namespace roadwars;
int main() {
    std::string error;
    std::optional<WeaponSalvoShot> shot;
    WeaponSalvoRuntime machine;
    assert(machine.initialize("Machine Gun", error) == Result::ok);
    assert(machine.begin({{}, 0, 0, 3}));
    assert(machine.advance(.1f, 0, 0, shot, 5) == Result::ok && shot);
    assert(machine.muzzle_flashes().size() == 2);
    assert(machine.muzzle_flashes()[0].visible && !machine.muzzle_flashes()[1].visible);
    assert(machine.muzzle_flashes()[0].color_argb == 0xffffffff);
    assert(machine.muzzle_flashes()[0].local_roll ==
           23); // World getter adds parent's roll each tick.
    assert(machine.advance(.1f, 0, 0, shot, 5) == Result::ok && !shot);
    assert(machine.muzzle_flashes()[0].local_roll == 46);
    assert((machine.muzzle_flashes()[0].color_argb >> 24) == 178);
    assert(machine.advance(.1f, 0, 0, shot, 5) == Result::ok && shot->muzzle_index == 1);
    assert(machine.muzzle_flashes()[1].visible && machine.muzzle_flashes()[1].remaining == 1);
    machine.cancel();
    assert(machine.advance(.1f, 0, 0, shot, 5) == Result::ok);
    assert(!machine.muzzle_flashes()[0].visible && !machine.muzzle_flashes()[1].visible);
    assert(machine.muzzle_flashes()[0].local_roll == 69); // Inactive callback does not rotate.
    for (const auto name : {"20mm Cannon", "Plasma Blaster", "Vulcanized Gun"}) {
        WeaponSalvoRuntime single;
        assert(single.initialize(name, error) == Result::ok);
        assert(single.begin({{}, 0, 0, 3}));
        assert(single.advance(.1f, 0, 0, shot) == Result::ok && shot);
        assert(single.muzzle_flashes().size() == 1 && single.muzzle_flashes()[0].visible);
        assert((single.muzzle_flashes()[0].color_argb >> 24) == 178);
        assert(single.advance(.3f, 0, 0, shot) == Result::ok && !shot);
        assert(!single.muzzle_flashes()[0].visible && single.muzzle_flashes()[0].local_roll == 0);
    }
    WeaponSalvoRuntime gatling;
    assert(gatling.initialize("Gatling Gun", error) == Result::ok);
    assert(gatling.begin({{}, 0, 0, 3}));
    assert(gatling.advance(.01f, 0, 0, shot, 100) == Result::ok && shot->muzzle_index == 0);
    assert(gatling.muzzle_flashes().size() == 5 && gatling.muzzle_flashes()[0].visible);
    assert(gatling.muzzle_flashes()[0].scale == 1); // Scale update precedes first launch.
    assert(std::abs(gatling.muzzle_flashes()[0].local_roll - 1.8f) < .0001f);
    assert(gatling.advance(.01f, 0, 0, shot, 100) == Result::ok && !shot);
    assert(std::abs(gatling.muzzle_flashes()[0].scale - 1.06f) < .0001f);
    assert((gatling.muzzle_flashes()[0].color_argb >> 24) == 247);
    for (unsigned i = 0; i < 200; ++i)
        assert(gatling.advance(.01f, 0, 0, shot) == Result::ok);
    for (const auto& flash : gatling.muzzle_flashes())
        assert(!flash.visible);
    assert(gatling.ready());
}
