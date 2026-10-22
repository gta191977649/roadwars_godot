#include "game_src/unresolved/mesh_scene.hpp"
#include "game_src/unresolved/race_ai_combat.hpp"
#include "game_src/unresolved/resource_store.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <map>
#include <vector>

using namespace roadwars;
namespace {
struct Fixture {
    std::array<RaceAiCombatVehicleView, 3> cars{};
    std::array<RaceAiCombatWeaponView, 3> weapons{};
    RaceAiCombatFrame frame;
    RaceAiCombatState state;
    RaceAiCombatServices services;
    RaceAiCombatTrace trace;
    std::string error;
    std::vector<int> calls;
    unsigned draws{}, refreshes{}, wall_queries{};
    std::uint32_t random_value{14};
    int target{1};
    bool blocked{};
    Fixture() {
        cars[0].chassis_identity = 1;
        cars[0].route_node = 5;
        cars[0].route_lateral = {1, 0, 0};
        cars[1].chassis_identity = 2;
        cars[1].route_node = 6;
        cars[1].route_lateral = {1, 0, 0};
        cars[1].position = {0, 0, -10};
        cars[2] = cars[1];
        cars[2].chassis_identity = 3;
        frame = {cars, 0, 100, true, false, {0, 0, 1}};
        services.weapon = [this](std::size_t slot, RaceAiCombatWeaponView& out) {
            out = weapons[slot];
            return Result::ok;
        };
        services.refresh_side_target = [this](RaceAiCombatWeaponView& out) {
            ++refreshes;
            out = weapons[1];
            out.target_index = target;
            return Result::ok;
        };
        services.side_wall_blocked = [this](bool& out) {
            ++wall_queries;
            out = blocked;
            return Result::ok;
        };
        services.activate = [this](std::size_t slot) {
            calls.push_back(static_cast<int>(slot));
            return Result::ok;
        };
        services.random = [this] {
            ++draws;
            calls.push_back(10);
            return random_value;
        };
    }
    Result step(float dt = .25f) {
        return advance_race_ai_combat(frame, state, services, dt, trace, error);
    }
    void side() { weapons[1] = {true, true, false, -1, "Guided Missile"}; }
    void rear(std::string_view name = "Oil Slick") { weapons[2] = {true, false, false, -1, name}; }
};
void side_tests() {
    Fixture f;
    f.side();
    assert(f.step() == Result::ok);
    assert(f.calls == std::vector<int>({1, 10}));
    assert(f.draws == 1 && f.refreshes == 1 && f.wall_queries == 1);
    assert(f.state.side_delay_4ad4 == 24 && f.trace.side_target == 1);
    assert(f.trace.activation_attempts[1] == 1);
    assert(f.step(1) == Result::ok && f.state.side_delay_4ad4 == 23 && f.draws == 1);
    f.weapons[1].guided_kind_28 = false;
    assert(f.step(1) == Result::ok && f.state.side_delay_4ad4 == 23);

    Fixture disabled;
    disabled.side();
    disabled.frame.enabled_4de0 = false;
    disabled.state = {10, 2};
    disabled.services = {};
    assert(disabled.step() == Result::ok);
    assert(disabled.state.side_delay_4ad4 == 10 && disabled.state.rear_timer_0c == 2);

    Fixture same_node;
    same_node.side();
    same_node.cars[1].route_node = 5;
    assert(same_node.step() == Result::ok && same_node.calls.empty());
    assert(same_node.refreshes == 1 && same_node.wall_queries == 0);
    same_node.cars[1].route_node = 9;
    assert(same_node.step() == Result::ok && same_node.draws == 1);
    Fixture middle;
    middle.side();
    middle.cars[1].route_node = 10;
    assert(middle.step() == Result::ok && middle.calls.empty() && middle.wall_queries == 0);
    middle.cars[0].route_node = 0;
    middle.cars[1].route_node = 95;
    assert(middle.step() == Result::ok && middle.draws == 1);

    Fixture wall;
    wall.side();
    wall.blocked = true;
    assert(wall.step() == Result::ok && wall.calls.empty());
    assert(wall.trace.side_branch == "WallBlocked" && wall.wall_queries == 1);
    Fixture curved;
    curved.side();
    curved.cars[1].route_lateral = {0, 0, 1};
    assert(curved.step() == Result::ok && curved.calls.empty());
    assert(curved.trace.side_branch == "RouteAngle");
    curved.cars[1].route_lateral = {};
    assert(curved.step() == Result::ok && curved.calls.empty());
    Fixture absent;
    absent.side();
    absent.target = -1;
    assert(absent.step() == Result::ok && absent.wall_queries == 0 && absent.calls.empty());
}
void direct_tests() {
    Fixture f;
    f.frame.direct_fire_4aec = true;
    f.weapons[0].present = true;
    f.weapons[1].present = true;
    f.state.side_delay_4ad4 = 7;
    assert(f.step() == Result::ok);
    assert(f.calls == std::vector<int>({0, 1}));
    assert(f.state.side_delay_4ad4 == 7 && f.refreshes == 0 && f.draws == 0);
    f.weapons[1].guided_kind_28 = true;
    f.calls.clear();
    assert(f.step() == Result::ok && f.calls == std::vector<int>({0}));
}
void rear_tests() {
    Fixture f;
    f.rear();
    f.state.rear_timer_0c = 1.25f;
    assert(f.step() == Result::ok && f.calls.empty()); // Strict >1.5, not >=.
    assert(f.state.rear_timer_0c == 1.5f);
    assert(f.step() == Result::ok);
    assert(f.calls == std::vector<int>({2, 10}));
    assert(f.state.rear_timer_0c == -9); // rand14%10+5, after scan reset.

    Fixture no_target;
    no_target.rear();
    no_target.cars[1].position = {0, 0, 10};
    no_target.cars[2].disabled_134 = 1;
    no_target.state.rear_timer_0c = 2;
    assert(no_target.step() == Result::ok && no_target.calls.empty());
    assert(no_target.state.rear_timer_0c == 0); // Failed scan still resets.
    for (float distance : {5.0f, 30.0f}) {
        Fixture edge;
        edge.rear();
        edge.cars[1].position.z = -distance;
        edge.cars[2].disabled_134 = 1;
        edge.state.rear_timer_0c = 2;
        assert(edge.step() == Result::ok && edge.calls.empty());
    }

    Fixture jammer;
    jammer.rear("mIsSiLe JaMmEr");
    jammer.cars[1].side_weapon = {true, true, true, 0, "Guided Missile"};
    jammer.state.rear_timer_0c = 2.75f;
    assert(jammer.step() == Result::ok && jammer.calls.empty()); // Strict >3.
    assert(jammer.step() == Result::ok && jammer.calls == std::vector<int>({2}));
    assert(jammer.draws == 0 && jammer.state.rear_timer_0c == 0);
    Fixture no_guidance;
    no_guidance.rear("Missile Jammer");
    no_guidance.state.rear_timer_0c = 4;
    no_guidance.cars[1].side_weapon = {true, true, false, 0, "Machine Gun"};
    assert(no_guidance.step() == Result::ok && no_guidance.calls.empty());
    assert(no_guidance.state.rear_timer_0c == 0);
    no_guidance.cars[1].side_weapon.active_270 = true;
    no_guidance.cars[1].side_weapon.target_index = 2;
    no_guidance.state.rear_timer_0c = 4;
    assert(no_guidance.step() == Result::ok && no_guidance.calls.empty());

    Fixture same_identity;
    same_identity.rear();
    same_identity.state.rear_timer_0c = 4;
    same_identity.cars[1].chassis_identity = 1;
    same_identity.cars[2].disabled_134 = .01f;
    assert(same_identity.step() == Result::ok && same_identity.calls.empty());
}
void failures() {
    Fixture f;
    assert(f.step(-1) == Result::invalid_argument);
    assert(f.step(std::numeric_limits<float>::quiet_NaN()) == Result::invalid_argument);
    f.frame.self_index = 3;
    assert(f.step() == Result::invalid_argument);
    Fixture missing;
    missing.side();
    missing.services.side_wall_blocked = {};
    assert(missing.step() == Result::unavailable && missing.calls.empty());
    Fixture query_fail;
    query_fail.services.weapon = [](std::size_t, RaceAiCombatWeaponView&) {
        return Result::unavailable;
    };
    assert(query_fail.step() == Result::unavailable);
    Fixture activation_fail;
    activation_fail.side();
    activation_fail.services.activate = [](std::size_t) { return Result::unavailable; };
    assert(activation_fail.step() == Result::unavailable && activation_fail.draws == 0);
    Fixture bad_target;
    bad_target.side();
    bad_target.target = 3;
    assert(bad_target.step() == Result::invalid_argument);
}
void setup_tests() {
    MeshObject root;
    root.name = "Root";
    for (const auto* name :
         {"ULRORMount1", "ULRORMount2", "ULRORMount3", "ULRORMount4", "FrontWeaponMount",
          "RearWeaponMount", "ULRORMount1", "frontweaponmount"}) {
        MeshObject object;
        object.name = name;
        root.children.push_back(object);
    }
    assert(race_weapon_mount_count(root) == 5);
    assert(race_weapon_mount_count({}) == 0);
    assert(!race_weapon_activation_allowed(1, 100, 0));
    assert(!race_weapon_activation_allowed(0, 0, 0));
    assert(race_weapon_activation_allowed(0, .01f, 0));
    assert(!race_weapon_activation_allowed(0, 20, 20));
    assert(race_world_weapons_unlocked(0, 20, 20));
    assert(!race_world_weapons_unlocked(0, 19.99f, 20));
    assert(!race_world_weapons_unlocked(1, 20, 20));
    assert(race_weapon_activation_allowed(0, 20.01f, 20));
    assert(!race_weapon_activation_allowed(0, 21, 20, .01f));
    assert(!race_weapon_activation_allowed(std::numeric_limits<float>::quiet_NaN(), 21, 20));
    RaceAiCombatState state;
    reset_race_ai_side_delay(state, 59);
    reset_race_ai_rear_timer(state, 2999);
    assert(state.side_delay_4ad4 == 44 && state.rear_timer_0c == 1.499f);
}
void input_tests() {
    RaceWeaponInput input;
    RaceWeaponInputState state;
    RaceWeaponInputServices services;
    std::vector<int> calls;
    services.activate = [&](std::size_t slot) {
        calls.push_back(static_cast<int>(slot));
        return Result::ok;
    };
    services.nitrous = [&] {
        calls.push_back(3);
        return Result::ok;
    };
    services.cycle = [&](int& selected) {
        calls.push_back(4);
        selected = 4;
        return Result::ok;
    };
    std::string error;
    input.weapon_down = {true, true, true};
    input.nitrous_down = true;
    assert(dispatch_race_weapon_input(input, state, {true, false, true}, 0, services, error) ==
           Result::ok);
    assert(calls == std::vector<int>({0, 2, 3}));
    for (int i = 0; i < 20; ++i)
        assert(dispatch_race_weapon_input(input, state, {true, true, true}, 0, services, error) ==
               Result::ok);
    assert(calls.size() == 3); // Held input cannot repeat during catch-up.
    input = {};
    assert(dispatch_race_weapon_input(input, state, {true, true, true}, 0, services, error) ==
           Result::ok);
    input.current_pressed = true; // A host-latched short press can be released before a tick.
    assert(dispatch_race_weapon_input(input, state, {}, 5, services, error) == Result::ok);
    assert(calls.back() == 2); // Current item dispatch intentionally has no presence guard.
    input.current_pressed = false;
    assert(dispatch_race_weapon_input(input, state, {}, 5, services, error) == Result::ok);
    assert(calls.size() == 4);
    input.current_down = true;
    input.cycle_down = true;
    assert(dispatch_race_weapon_input(input, state, {}, 3, services, error) == Result::ok);
    assert(calls[calls.size() - 2] == 4 && calls.back() == 1); // Cycle before current activation.
    input = {};
    assert(dispatch_race_weapon_input(input, state, {}, 0, services, error) == Result::ok);
    input.current_down = true;
    assert(dispatch_race_weapon_input(input, state, {}, 2, services, error) == Result::ok);
    assert(calls.back() == 3);
    input = {};
    assert(dispatch_race_weapon_input(input, state, {}, 0, services, error) == Result::ok);
    input.weapon_pressed[0] = true;
    assert(dispatch_race_weapon_input(input, state, {true, false, false}, 0, {}, error) ==
           Result::unavailable);
}
class MemoryFiles : public ResourceReader {
  public:
    std::map<std::string, std::vector<std::byte>> files;
    Result read(std::string_view path, std::vector<std::byte>& bytes) override {
        const auto it = files.find(std::string(path));
        if (it == files.end())
            return Result::not_found;
        bytes = it->second;
        return Result::ok;
    }
    void text(std::string path, std::string_view value, bool encoded = false) {
        std::vector<std::byte> bytes;
        for (unsigned char c : value)
            bytes.push_back(
                static_cast<std::byte>(encoded ? static_cast<unsigned char>(c + 1) : c));
        files[std::move(path)] = std::move(bytes);
    }
};
void binding_tests() {
    MemoryFiles files;
    const std::string values =
        "[INPUTS]\nACTIVATE_WEAPON=29\nACTIVATE_FRONT_WEAPON=2\nACTIVATE_SIDE_WEAPON=3\n"
        "ACTIVATE_REAR_WEAPON=4\nACTIVATE_NITROUS_BOOST=5\nDISPLAY=15\n";
    files.text("Controllers/Keyboard/Data.pse", values, true);
    files.text("Controllers/Keyboard/Data.ini", "[INPUTS]\nACTIVATE_WEAPON=57\n");
    ResourceStore resources(files);
    OriginalWeaponKeys keys;
    std::string error;
    assert(load_original_weapon_keys(resources, keys, error) == Result::ok);
    assert(keys.current == 29 && keys.front == 2 && keys.side == 3 && keys.rear == 4 &&
           keys.nitrous == 5 && keys.cycle == 15);
    files.text("Controllers/Keyboard/Data.pse", values + "ACTIVATE_WEAPON=2.5\n", true);
    assert(load_original_weapon_keys(resources, keys, error) == Result::invalid_argument);
    assert(keys.current == 29); // Failed loads do not partially replace a valid key map.
    files.files.erase("Controllers/Keyboard/Data.pse");
    assert(load_original_weapon_keys(resources, keys, error) == Result::invalid_argument);
    files.files.clear();
    assert(load_original_weapon_keys(resources, keys, error) == Result::not_found);
}
void target_tests() {
    Fixture f;
    std::string error;
    int target{};
    f.cars[0].route_node = 10;
    f.cars[1].route_node = 12;
    f.cars[2].route_node = 11;
    assert(select_race_missile_target(f.cars, 0, 100, false, false, target, error) == Result::ok);
    assert(target == 2);
    assert(select_race_missile_target(f.cars, 0, 100, true, false, target, error) == Result::ok);
    assert(target == -1);
    f.cars[1].route_node = 9;
    f.cars[2].route_node = 8;
    assert(select_race_missile_target(f.cars, 0, 100, true, false, target, error) == Result::ok);
    assert(target == 1);
    assert(select_race_missile_target(f.cars, 0, 100, false, true, target, error) == Result::ok);
    assert(target == 1);
    f.cars[0].route_node = 99;
    f.cars[1].route_node = 0;
    f.cars[2].route_node = 1;
    assert(select_race_missile_target(f.cars, 0, 100, false, false, target, error) == Result::ok);
    assert(target == 1); // Route wrap, not Euclidean nearest vehicle.
    for (auto& car : f.cars) {
        car.route_node = 10;
        car.route_sample = {};
    }
    f.cars[0].position = {0, 999, 5};
    f.cars[1].position = {0, -999, 9};
    f.cars[2].position = {0, 0, 6};
    assert(select_race_missile_target(f.cars, 0, 100, false, false, target, error) == Result::ok);
    assert(target == 2); // Same node uses XZ-only route-sample distances.
    f.cars[2].disabled_134 = 1;
    assert(select_race_missile_target(f.cars, 0, 100, false, false, target, error) == Result::ok);
    assert(target == 2); // Original selector contains no destroyed-car filter.
    assert(select_race_missile_target(f.cars, 3, 100, false, false, target, error) ==
           Result::invalid_argument);
    assert(!race_missile_owner_wrong_way({0, 0, 1}, {}, {0, 0, 10}));
    assert(race_missile_owner_wrong_way({0, 0, -1}, {}, {0, 0, 10}));
}
} // namespace
int main() {
    const auto reference = race_ai_rear_reference({4, 5, 6}, {1, 2, 3}, {10, 20, 30});
    assert(reference.x == 13 && reference.y == 23 && reference.z == 33);
    assert(race_ai_combat_angle({1, 0, 0}, {1, 0, 0}) == 0);
    assert(std::abs(race_ai_combat_angle({1, 0, 0}, {0, 1, 0}) - 89.988f) < .02f);
    assert(std::isnan(race_ai_combat_angle({}, {1, 0, 0})));
    side_tests();
    direct_tests();
    rear_tests();
    failures();
    setup_tests();
    input_tests();
    binding_tests();
    target_tests();
    std::cout << "Original AI combat predicate contracts passed\n";
}
