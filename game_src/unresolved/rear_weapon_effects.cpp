#include "game_src/unresolved/rear_weapon_effects.hpp"
#include <cmath>
#include <cstdint>
#include <limits>

namespace roadwars {
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00415761
std::vector<std::size_t>
character_missile_jam_indices(std::uint64_t owner, Vec3 position,
                              std::span<const PassiveJammerWeaponView> weapons) {
    std::vector<std::size_t> result;
    for (std::size_t index = 0; index < weapons.size(); ++index) {
        const auto& weapon = weapons[index];
        if (weapon.chassis_identity == owner || weapon.target_identity != owner || !weapon.active)
            continue;
        for (const auto p : weapon.projectile_positions) {
            const auto x = p.x - position.x, z = p.z - position.z;
            if (std::sqrt(x * x + z * z) < 30)
                result.push_back(index);
        }
    }
    return result;
}
namespace {
bool valid_dt(float dt) { return std::isfinite(dt) && dt >= 0; }
float distance_xz(Vec3 a, Vec3 b) {
    const float x = a.x - b.x, z = a.z - b.z;
    return std::sqrt(z * z + x * x);
}
bool contains_xz(Vec3 point, const std::array<Vec3, 3>& triangle) {
    unsigned result{}, previous = 2;
    for (unsigned i = 0; i < 3; ++i) {
        const auto& a = triangle[i];
        const auto& b = triangle[previous];
        if (((a.z <= point.z && point.z < b.z) &&
             (point.x - a.x) * (b.z - a.z) < (point.z - a.z) * (b.x - a.x)) ||
            ((b.z <= point.z && point.z < a.z) &&
             (point.z - a.z) * (b.x - a.x) < (point.x - a.x) * (b.z - a.z)))
            result ^= 1;
        previous = i;
    }
    return result != 0;
}
} // namespace

// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00465217
bool missile_jammer_can_activate(const MissileJammerState& state) {
    return !state.active_270 && state.ready_540;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00465217
void begin_missile_jammer(MissileJammerState& state) {
    state.active_270 = true;
    state.elapsed_53c = 0;
    state.ready_540 = false;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00465038
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0046517e
void reset_missile_jammer(MissileJammerState& state) {
    state.active_270 = false;
    state.ready_540 = true;
    state.elapsed_53c = 0;
}

// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00465395
Result advance_missile_jammer(MissileJammerState& state, const MissileJammerFrame& frame,
                              MissileJammerDraws draws, float dt, MissileJammerOutput& output,
                              std::string& error) {
    output = {};
    if (!valid_dt(dt) || !std::isfinite(state.elapsed_53c) || !std::isfinite(state.radius_538) ||
        state.radius_538 < 0) {
        error = "Invalid original Missile Jammer update";
        return Result::invalid_argument;
    }
    const float grey = static_cast<float>(draws.grey % 10) * .1f;
    const auto component = static_cast<std::uint32_t>(grey * 255.0f);
    output.color_argb = 0xff000000u | component << 16 | component << 8 | component;
    const float angle = static_cast<float>(draws.heading % 270);
    output.heading =
        draws.sign % 2 == 0 ? frame.effect_heading - angle : frame.effect_heading + angle;
    for (std::size_t i = 0; i < frame.side_weapons.size(); ++i) {
        const auto& weapon = frame.side_weapons[i];
        if (weapon.chassis_identity == frame.owner_chassis_identity)
            continue;
        for (const auto position : weapon.projectile_positions)
            if (distance_xz(frame.owner_position, position) < state.radius_538)
                output.jam_weapon_indices.push_back(i);
    }
    if (!state.active_270) {
        output.remove_visual = output.stop_audio = true;
        state.ready_540 = true;
        state.elapsed_53c = 0;
    } else {
        state.elapsed_53c += dt;
        if (state.elapsed_53c > 5) {
            output.deactivate = true;
            state.active_270 = false;
        }
    }
    return Result::ok;
}

// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0044f81d
void set_guided_missile_jam_timers(std::span<float> timers, float value) {
    for (auto& timer : timers)
        timer = value;
}

// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00474472
bool oil_slick_can_activate(const OilSlickState& state) {
    return !state.active_270 && state.current_patch_2dc < 4;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00474472
void begin_oil_slick(OilSlickState& state, int node, Vec3 position) {
    state.sample_time_2d4 = state.elapsed_2d0 = 0;
    ++state.patch_count_2d8;
    state.active_270 = true;
    state.patches[state.current_patch_2dc].route_node = node;
    state.previous_position_4994 = position;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004746b7
Result advance_oil_slick(OilSlickState& state, Vec3 position, Vec3 forward, float dt,
                         const std::function<Result(std::array<Vec3, 3>&)>& project,
                         OilSlickOutput& output, std::string& error) {
    output = {};
    if (!valid_dt(dt) || !std::isfinite(state.elapsed_2d0) ||
        !std::isfinite(state.sample_time_2d4) || state.patch_count_2d8 > 4 ||
        state.current_patch_2dc > 4 || (state.active_270 && state.current_patch_2dc >= 4)) {
        error = "Invalid original Oil Slick emission state";
        return Result::invalid_argument;
    }
    state.elapsed_2d0 += dt;
    state.sample_time_2d4 += dt;
    if (state.elapsed_2d0 > .9f && state.active_270) {
        state.active_270 = false;
        ++state.current_patch_2dc;
        output.deactivate = true;
    }
    if (!state.active_270) {
        output.remove_callback = true;
        return Result::ok;
    }
    const float travel = std::abs(state.previous_position_4994.x - position.x) +
                         std::abs(state.previous_position_4994.z - position.z);
    if (!(state.sample_time_2d4 > .2f || travel > 4))
        return Result::ok;
    if (!project) {
        error = "Original Oil Slick ground projection unavailable";
        return Result::unavailable;
    }
    state.previous_position_4994 = position;
    state.sample_time_2d4 = 0;
    const float a = (6.0f * forward.z - 6.0f * forward.x) / 2.0f;
    const float b = (-6.0f * forward.z - 6.0f * forward.x) / 2.0f;
    const float c = (6.0f * forward.x + -6.0f * forward.z) / 2.0f;
    std::array<Vec3, 3> triangle{{position,
                                  {position.x + a, position.y, position.z + b},
                                  {position.x + b, position.y, position.z + c}}};
    const auto result = project(triangle);
    if (result != Result::ok) {
        error = "Original Oil Slick ground projection failed";
        return result;
    }
    auto& patch = state.patches[state.current_patch_2dc];
    if (patch.triangles.size() < 50) {
        patch.triangles.push_back(triangle);
        // Original collision triangle is stored before the later render offsets.
        triangle[1].x += a;
        triangle[1].z += b;
        triangle[2].x += b;
        triangle[2].z += c;
        for (auto& vertex : triangle)
            vertex.y += .09f; // 005f5800.
        output.visual_triangle = triangle;
    }
    return Result::ok;
}

// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004d59cf
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00474bb5
Result oil_slick_wheel_contacts(std::span<const OilSlickPatchView> patches, int node, int count,
                                const std::array<Vec3, 4>& wheels, unsigned& contacts,
                                std::string& error) {
    contacts = 0;
    if (count <= 0 || node < 0 || node >= count || patches.size() > 5) {
        error = "Invalid original Oil Slick contact query";
        return Result::invalid_argument;
    }
    for (const auto& patch : patches) {
        if (patch.route_node < 0 || patch.route_node >= count || patch.triangles.size() > 50) {
            error = "Invalid original Oil Slick patch";
            return Result::invalid_argument;
        }
        const auto difference = std::abs(static_cast<std::int64_t>(node) - patch.route_node);
        if (difference >= 3 && difference <= count - 4)
            continue;
        for (const auto wheel : wheels)
            for (const auto& triangle : patch.triangles)
                if (contains_xz(wheel, triangle))
                    ++contacts;
    }
    return Result::ok;
}

// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0042b31a
void apply_oil_slick_grip(unsigned contacts, float speed, std::array<float, 4>& longitudinal,
                          std::array<float, 4>& lateral, std::array<float, 4>& rolling) {
    if (contacts > 0 && speed > 5) {
        longitudinal.fill(.05f);
        lateral.fill(.05f);
        rolling.fill(.05f);
    }
}

// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004a622b
void begin_smoke_screen(SmokeScreenState& state) {
    state.elapsed_c88 = 0;
    state.active_270 = true;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004a62b5
Result advance_smoke_screen(SmokeScreenState& state, float dt, SmokeScreenOutput& output,
                            std::string& error) {
    output = {};
    if (!valid_dt(dt) || !std::isfinite(state.elapsed_c88)) {
        error = "Invalid original Smoke Screen update";
        return Result::invalid_argument;
    }
    state.elapsed_c88 += dt;
    if (state.elapsed_c88 > 3 && state.active_270) {
        output.deactivate = true;
        state.active_270 = false;
    }
    output.remove_callback = state.sound_present_c84 && !state.active_270;
    output.emit = state.active_270;
    return Result::ok;
}

// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00406567
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0044dda1
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00456617
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0049d0b7
Result advance_rear_deployment(RearDeployKind kind, RearDeployPoolState& state, float dt,
                               const std::function<Result(unsigned)>& launch,
                               RearDeployOutput& output, std::string& error) {
    output = {};
    if (!valid_dt(dt) || !std::isfinite(state.timer_2d0) || state.next_2d4 > 5 ||
        (kind == RearDeployKind::grenades && state.next_2d4 >= 5) ||
        static_cast<unsigned>(kind) > static_cast<unsigned>(RearDeployKind::arachnoids)) {
        error = "Invalid original rear deployment state";
        return Result::invalid_argument;
    }
    const bool grenade = kind == RearDeployKind::grenades;
    state.timer_2d0 += grenade ? -dt : dt;
    if (!state.active_270)
        return Result::ok;
    if (grenade || state.next_2d4 < 5) {
        if (!launch) {
            error = "Original rear weapon instance launch unavailable";
            return Result::unavailable;
        }
        const auto result = launch(state.next_2d4);
        if (result != Result::ok) {
            error = "Original rear weapon instance launch failed";
            return result;
        }
        ++state.next_2d4;
        if (grenade && state.next_2d4 > 4)
            state.next_2d4 = 0;
    }
    output.deactivate = output.remove_callback = true;
    state.active_270 = false;
    if (grenade)
        state.timer_2d0 = 1;
    return Result::ok;
}
} // namespace roadwars
