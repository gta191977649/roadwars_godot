#include "game_src/unresolved/rear_weapon_effects.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

using namespace roadwars;
int main() {
    std::string error;
    const std::array<Vec3, 3> incoming{{{0, 9999, 29.99f}, {0, 0, 30}, {0, 0, 2}}};
    const std::array<PassiveJammerWeaponView, 4> passive{{{2, 1, true, incoming},
                                                          {1, 1, true, incoming},
                                                          {3, 2, true, incoming},
                                                          {4, 1, false, incoming}}};
    assert(character_missile_jam_indices(1, {}, passive) == std::vector<std::size_t>({0, 0}));
    MissileJammerState jammer;
    assert(jammer.radius_538 == 30 && missile_jammer_can_activate(jammer));
    begin_missile_jammer(jammer);
    assert(!missile_jammer_can_activate(jammer));
    const std::array<Vec3, 3> near{{{0, 9999, 29}, {0, 0, 30}, {0, 0, 2}}};
    const std::array<Vec3, 1> own{{{0, 0, 2}}};
    const std::array<JammerWeaponView, 3> weapons{{{1, own}, {2, near}, {3, {}}}};
    const MissileJammerFrame frame{1, {}, 100, weapons};
    MissileJammerOutput out;
    assert(advance_missile_jammer(jammer, frame, {4, 0, 269}, 5, out, error) == Result::ok);
    assert(out.color_argb == 0xff666666u && out.heading == -169);
    assert(out.jam_weapon_indices == std::vector<std::size_t>({1, 1}));
    assert(jammer.active_270 && !out.deactivate); // Strict >5 boundary.
    assert(advance_missile_jammer(jammer, frame, {0, 1, 270}, .01f, out, error) == Result::ok);
    assert(out.deactivate && !jammer.active_270 && !jammer.ready_540);
    assert(!out.remove_visual && !out.stop_audio && out.heading == 100);
    assert(advance_missile_jammer(jammer, frame, {9, 1, 271}, .01f, out, error) == Result::ok);
    assert(jammer.ready_540 && jammer.elapsed_53c == 0 && out.remove_visual && out.stop_audio);
    assert(out.jam_weapon_indices.size() == 2); // Original final cleanup update still scans.
    assert(out.heading == 101);
    std::array<float, 3> jam_timers{2, 0, .5f};
    set_guided_missile_jam_timers(jam_timers, 1);
    assert(jam_timers == (std::array<float, 3>{1, 1, 1}));
    jammer.radius_538 = 40;
    reset_missile_jammer(jammer);
    assert(jammer.radius_538 == 40 && missile_jammer_can_activate(jammer));
    assert(advance_missile_jammer(jammer, frame, {}, -1, out, error) == Result::invalid_argument);

    const std::array<Vec3, 3> triangle{{{0, 0, 0}, {4, 0, 0}, {0, 0, 4}}};
    const std::array<std::array<Vec3, 3>, 2> triangles{triangle, triangle};
    const std::array<OilSlickPatchView, 1> patches{{{10, triangles}}};
    const std::array<Vec3, 4> wheels{{{1, 999, 1}, {10, 0, 10}, {2, -999, 1}, {-1, 0, -1}}};
    unsigned contacts{};
    assert(oil_slick_wheel_contacts(patches, 12, 100, wheels, contacts, error) == Result::ok);
    assert(contacts == 4); // Both overlapping triangles count each of two wheels.
    assert(oil_slick_wheel_contacts(patches, 13, 100, wheels, contacts, error) == Result::ok);
    assert(contacts == 0); // Strict diff<3.
    const std::array<OilSlickPatchView, 1> wrap{{{99, triangles}}};
    assert(oil_slick_wheel_contacts(wrap, 0, 100, wheels, contacts, error) == Result::ok);
    assert(contacts == 4);
    assert(oil_slick_wheel_contacts(patches, -1, 100, wheels, contacts, error) ==
           Result::invalid_argument);
    OilSlickState oil;
    OilSlickOutput oil_out;
    assert(oil_slick_can_activate(oil));
    begin_oil_slick(oil, 10, {});
    assert(!oil_slick_can_activate(oil));
    unsigned projections{};
    const auto project = [&](std::array<Vec3, 3>& vertices) {
        ++projections;
        for (auto& vertex : vertices)
            vertex.y = 100;
        return Result::ok;
    };
    assert(advance_oil_slick(oil, {}, {0, 0, 1}, .2f, project, oil_out, error) == Result::ok);
    assert(projections == 0 && !oil_out.visual_triangle); // Strict >.2.
    assert(advance_oil_slick(oil, {}, {0, 0, 1}, .01f, project, oil_out, error) == Result::ok);
    assert(projections == 1 && oil_out.visual_triangle);
    assert(oil.patches[0].triangles.size() == 1 && oil.patches[0].route_node == 10);
    assert(oil.patches[0].triangles[0][1].x == 3 && oil.patches[0].triangles[0][1].z == -3);
    assert((*oil_out.visual_triangle)[1].x == 6 && (*oil_out.visual_triangle)[1].z == -6);
    assert(oil.patches[0].triangles[0][1].y == 100 && (*oil_out.visual_triangle)[1].y == 100.09f);
    assert(advance_oil_slick(oil, {5, 0, 0}, {0, 0, 1}, .01f, project, oil_out, error) ==
           Result::ok);
    assert(projections == 2); // Travel>4 also emits, even with a short interval.
    oil.patches[0].triangles.resize(50);
    assert(advance_oil_slick(oil, {10, 0, 0}, {0, 0, 1}, .01f, project, oil_out, error) ==
           Result::ok);
    assert(projections == 3 && !oil_out.visual_triangle && oil.patches[0].triangles.size() == 50);
    assert(advance_oil_slick(oil, {}, {0, 0, 1}, 1, project, oil_out, error) == Result::ok);
    assert(oil_out.deactivate && oil_out.remove_callback && oil.current_patch_2dc == 1);
    for (unsigned i = 1; i < 4; ++i) {
        assert(oil_slick_can_activate(oil));
        begin_oil_slick(oil, static_cast<int>(i), {});
        assert(advance_oil_slick(oil, {}, {0, 0, 1}, 1, project, oil_out, error) == Result::ok);
    }
    assert(!oil_slick_can_activate(oil) && oil.patch_count_2d8 == 4 && oil.current_patch_2dc == 4);
    OilSlickState no_projection;
    begin_oil_slick(no_projection, 0, {});
    assert(advance_oil_slick(no_projection, {}, {0, 0, 1}, .3f, {}, oil_out, error) ==
           Result::unavailable);
    std::array<float, 4> longitudinal{1, 2, 3, 4}, lateral{2, 3, 4, 5}, rolling{3, 4, 5, 6};
    apply_oil_slick_grip(1, 5, longitudinal, lateral, rolling);
    assert(longitudinal[0] == 1 && lateral[0] == 2 && rolling[0] == 3);
    apply_oil_slick_grip(0, 6, longitudinal, lateral, rolling);
    assert(longitudinal[0] == 1);
    apply_oil_slick_grip(1, 5.01f, longitudinal, lateral, rolling);
    for (unsigned i = 0; i < 4; ++i)
        assert(longitudinal[i] == .05f && lateral[i] == .05f && rolling[i] == .05f);

    SmokeScreenState smoke;
    SmokeScreenOutput smoke_out;
    begin_smoke_screen(smoke);
    smoke.sound_present_c84 = true;
    assert(advance_smoke_screen(smoke, 3, smoke_out, error) == Result::ok);
    assert(smoke_out.emit && !smoke_out.deactivate && !smoke_out.remove_callback);
    assert(advance_smoke_screen(smoke, .01f, smoke_out, error) == Result::ok);
    assert(!smoke_out.emit && smoke_out.deactivate && smoke_out.remove_callback);
    assert(advance_smoke_screen(smoke, -1, smoke_out, error) == Result::invalid_argument);

    for (const auto kind : {RearDeployKind::grenades, RearDeployKind::land_mines,
                            RearDeployKind::reactive_mines, RearDeployKind::arachnoids}) {
        RearDeployPoolState pool;
        RearDeployOutput deploy_out;
        std::vector<unsigned> launched;
        const auto launch = [&](unsigned i) {
            launched.push_back(i);
            return Result::ok;
        };
        for (unsigned i = 0; i < 5; ++i) {
            pool.active_270 = true;
            assert(advance_rear_deployment(kind, pool, .1f, launch, deploy_out, error) ==
                   Result::ok);
            assert(deploy_out.deactivate && deploy_out.remove_callback && !pool.active_270);
            assert(launched.back() == i);
        }
        pool.active_270 = true;
        assert(advance_rear_deployment(kind, pool, .1f, launch, deploy_out, error) == Result::ok);
        if (kind == RearDeployKind::grenades)
            assert(launched.size() == 6 && launched.back() == 0 && pool.timer_2d0 == 1);
        else
            assert(launched.size() == 5 && pool.next_2d4 == 5);
    }
    RearDeployPoolState missing{true};
    RearDeployOutput deploy_out;
    assert(advance_rear_deployment(RearDeployKind::land_mines, missing, .1f, {}, deploy_out,
                                   error) == Result::unavailable);
    assert(missing.active_270); // Missing actual instance code cannot pretend success.
    assert(advance_rear_deployment(RearDeployKind::land_mines, missing,
                                   std::numeric_limits<float>::quiet_NaN(), {}, deploy_out,
                                   error) == Result::invalid_argument);
    std::cout << "Original rear weapon predicate and effect kernels passed\n";
}
