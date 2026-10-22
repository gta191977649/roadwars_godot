#include "game_src/unresolved/rear_weapon_instances.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
using namespace roadwars;
namespace {
bool near(float a, float b) { return std::abs(a - b) < .0001f; }
RearInstanceTarget target(std::size_t index, std::uint64_t identity, Vec3 position) {
    RearInstanceTarget t;
    t.participant = index;
    t.chassis_identity = identity;
    t.position = position;
    t.footprint = {{{-2, 0, -2}, {2, 0, -2}, {2, 0, 2}, {-2, 0, 2}}};
    return t;
}
} // namespace
int main() {
    std::string error;
    RearInstanceWorld world;
    world.ground = [](Vec3 p, GroundContact& g) {
        g.hit = true;
        g.point = {p.x, 0, p.z};
        return Result::ok;
    };
    world.arachnoid_walls = [](Vec3, std::vector<WallRecord>&) { return Result::ok; };
    const RearInstanceParameters params{40, 3, 2};
    RearWeaponInstance grenade;
    grenade.heading = 17;
    bool launched;
    assert(launch_rear_weapon_instance(grenade, {0, 1, 0}, 99, launched, error) == Result::ok &&
           launched);
    assert(grenade.heading == 17);
    assert(launch_rear_weapon_instance(grenade, {9, 9, 9}, 99, launched, error) == Result::ok &&
           !launched);
    RearInstanceOutput out;
    assert(advance_rear_weapon_instance(grenade, params, 1, {}, world, .25f, out, error) ==
           Result::ok);
    assert(near(grenade.position.y, -.175f) && !out.exploded);
    assert(advance_rear_weapon_instance(grenade, params, 1, {}, world, .25f, out, error) ==
           Result::ok);
    assert(grenade.position.y == 0 && !out.exploded);
    std::array<RearInstanceTarget, 4> targets{{target(0, 1, {0, 0, 0}), target(1, 2, {2, 3, 0}),
                                               target(2, 3, {0, 6, 0}), target(3, 4, {0, 0, 6})}};
    assert(advance_rear_weapon_instance(grenade, params, 1, targets, world, .25f, out, error) ==
           Result::ok);
    assert(out.exploded && out.removed && !grenade.active && out.impacts.size() == 2);
    assert(out.impacts[0].participant == 0 && out.impacts[0].internal_damage == 40);
    assert(out.impacts[1].force.x == 12 && out.impacts[1].force.y == 18 &&
           out.impacts[1].apply_yaw);
    assert(out.impacts[1].application.y == 0);
    for (auto kind : {RearInstanceKind::land_mine, RearInstanceKind::reactive_mine}) {
        RearWeaponInstance mine;
        mine.kind = kind;
        assert(launch_rear_weapon_instance(mine, {}, 0, launched, error) == Result::ok);
        auto owner = target(0, 1, {});
        assert(advance_rear_weapon_instance(mine, params, 1, {&owner, 1}, world, 2, out, error) ==
               Result::ok);
        assert(!out.exploded && mine.age == 2); // Test sees previous age, not age+dt.
        assert(advance_rear_weapon_instance(mine, params, 1, {&owner, 1}, world, .01f, out,
                                            error) == Result::ok);
        assert(out.exploded && out.impacts.size() == 1 && !out.impacts[0].apply_yaw);
        assert(out.impacts[0].force.y == 6 && out.impacts[0].force.x == 0);
        assert(out.extra_reactive_effect == (kind == RearInstanceKind::reactive_mine));
    }
    RearWeaponInstance mine;
    mine.kind = RearInstanceKind::land_mine;
    launch_rear_weapon_instance(mine, {}, 0, launched, error);
    auto airborne = target(0, 2, {});
    airborne.air_time = {1, 1, 1, 1};
    assert(advance_rear_weapon_instance(mine, params, 1, {&airborne, 1}, world, 100, out, error) ==
           Result::ok);
    assert(!out.exploded && mine.active); // No guessed lifetime expiry.
    airborne.air_time[3] = 0;
    assert(advance_rear_weapon_instance(mine, params, 1, {&airborne, 1}, world, 0, out, error) ==
           Result::ok);
    assert(out.exploded);
    RearWeaponInstance arach;
    arach.kind = RearInstanceKind::arachnoid;
    launch_rear_weapon_instance(arach, {}, 180, launched, error);
    assert(near(arach.direction.z, 1));
    assert(advance_rear_weapon_instance(arach, params, 1, {}, world, .1f, out, error) ==
           Result::ok);
    assert(near(arach.position.z, .15f) && arach.position.y == 0);
    assert(near(arach.left_leg_heading, -6) && near(arach.right_leg_heading, 6));
    unsigned draws{};
    Vec3 queried;
    world.arachnoid_walls = [&](Vec3 p, std::vector<WallRecord>& walls) {
        queried = p;
        walls.push_back({{-1, 0, .5f}, {1, 0, .5f}, {}, 0});
        return Result::ok;
    };
    world.random = [&]() -> std::optional<std::uint32_t> {
        ++draws;
        return 90;
    };
    const auto previous = arach.position;
    assert(advance_rear_weapon_instance(arach, params, 1, {}, world, .1f, out, error) ==
           Result::ok);
    assert(draws == 1 && arach.heading == 90 && arach.position.z == previous.z);
    assert(near(queried.z, .3f));
    RearWeaponInstance unavailable;
    unavailable.kind = RearInstanceKind::arachnoid;
    launch_rear_weapon_instance(unavailable, {}, 180, launched, error);
    world.random = {};
    assert(advance_rear_weapon_instance(unavailable, params, 1, {}, world, .1f, out, error) ==
           Result::unavailable);
    stop_rear_weapon_instance(arach);
    assert(!arach.active && !arach.callback_running);
    std::cout << "rear weapon instance tests passed\n";
}
