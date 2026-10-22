#pragma once
#include "game_src/interfaces/host.hpp"
#include <array>
#include <functional>
#include <optional>
#include <span>
#include <string>

namespace roadwars {
// Portable views of the original weapon fields; original source/class names are unresolved.
struct MissileJammerState {
    bool active_270{}, ready_540{true};
    float elapsed_53c{}, radius_538{30};
};
struct JammerWeaponView {
    std::uint64_t chassis_identity{};
    std::span<const Vec3> projectile_positions;
};
struct PassiveJammerWeaponView {
    std::uint64_t chassis_identity{}, target_identity{};
    bool active{};
    std::span<const Vec3> projectile_positions;
};
std::vector<std::size_t> character_missile_jam_indices(std::uint64_t owner, Vec3 position,
                                                       std::span<const PassiveJammerWeaponView>);
struct MissileJammerFrame {
    std::uint64_t owner_chassis_identity{};
    Vec3 owner_position{};
    float effect_heading{};
    std::span<const JammerWeaponView> side_weapons;
};
struct MissileJammerDraws {
    std::uint32_t grey{}, sign{}, heading{};
};
struct MissileJammerOutput {
    std::uint32_t color_argb{};
    float heading{};
    bool deactivate{}, remove_visual{}, stop_audio{};
    std::vector<std::size_t> jam_weapon_indices;
};
bool missile_jammer_can_activate(const MissileJammerState&);
// Call after successful common activation004cf5ed; audio/visual attachment is host-owned.
void begin_missile_jammer(MissileJammerState&);
void reset_missile_jammer(MissileJammerState&);
Result advance_missile_jammer(MissileJammerState&, const MissileJammerFrame&, MissileJammerDraws,
                              float dt, MissileJammerOutput&, std::string& error);
void set_guided_missile_jam_timers(std::span<float> timers, float value);

struct OilSlickPatchView {
    int route_node{};
    std::span<const std::array<Vec3, 3>> triangles;
};
struct OilSlickPatch {
    int route_node{};
    std::vector<std::array<Vec3, 3>> triangles;
};
struct OilSlickState {
    bool active_270{};
    float elapsed_2d0{}, sample_time_2d4{};
    unsigned patch_count_2d8{}, current_patch_2dc{};
    Vec3 previous_position_4994{};
    std::array<OilSlickPatch, 5> patches;
};
struct OilSlickOutput {
    bool deactivate{}, remove_callback{};
    std::optional<std::array<Vec3, 3>> visual_triangle;
};
bool oil_slick_can_activate(const OilSlickState&);
void begin_oil_slick(OilSlickState&, int owner_route_node, Vec3 original_position);
Result advance_oil_slick(OilSlickState&, Vec3 original_position, Vec3 original_forward, float dt,
                         const std::function<Result(std::array<Vec3, 3>&)>& project_to_ground,
                         OilSlickOutput&, std::string& error);
Result oil_slick_wheel_contacts(std::span<const OilSlickPatchView>, int vehicle_route_node,
                                int main_node_count, const std::array<Vec3, 4>& wheels,
                                unsigned& contacts, std::string& error);
// Apply before original health/difficulty multipliers in0042b31a.
void apply_oil_slick_grip(unsigned contacts, float cached_speed, std::array<float, 4>& longitudinal,
                          std::array<float, 4>& lateral, std::array<float, 4>& rolling);

struct SmokeScreenState {
    bool active_270{}, sound_present_c84{};
    float elapsed_c88{};
};
struct SmokeScreenOutput {
    bool deactivate{}, remove_callback{}, emit{};
};
void begin_smoke_screen(SmokeScreenState&);
Result advance_smoke_screen(SmokeScreenState&, float dt, SmokeScreenOutput&, std::string& error);

enum class RearDeployKind { grenades, land_mines, reactive_mines, arachnoids };
struct RearDeployPoolState {
    bool active_270{};
    float timer_2d0{};
    unsigned next_2d4{};
};
struct RearDeployOutput {
    bool deactivate{}, remove_callback{};
};
// Only the original owning weapon's deployment dispatch. Launch callback must
// implement the distinct recovered instance behavior; there is no generic physics fallback.
Result advance_rear_deployment(RearDeployKind, RearDeployPoolState&, float dt,
                               const std::function<Result(unsigned)>& launch, RearDeployOutput&,
                               std::string& error);
} // namespace roadwars
