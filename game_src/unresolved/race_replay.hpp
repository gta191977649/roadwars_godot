#pragma once
#include "game_src/interfaces/host.hpp"
#include "game_src/unresolved/vehicle_damage_visuals.hpp"
#include "game_src/unresolved/weapon_runtime.hpp"
#include <deque>
#include <functional>
#include <variant>

namespace roadwars {
// Typed portable forms of the48-byte record and event pointers owned by0049d420.
// Names and ownership boundaries are reconstruction interfaces, not original class claims.
struct ReplayPose {
    Vec3 position{}, velocity{};
    float heading{}, pitch{}, roll{}, steering{}, wheel_spin{};
    bool nitrous{};
    int route_node{};
};
struct ReplayPoseRecord {
    float time{};
    Vec3 position{}, velocity{};
    std::uint32_t packed_angles{};
    std::int32_t steering_tenths{}, wheel_spin_tenths{}, nitrous{}, route_node{};
};
std::uint32_t pack_replay_angles(float heading, float pitch, float roll);
std::array<float, 3> unpack_replay_angles(std::uint32_t);
struct ReplayWeaponEvent {
    WeaponSlot slot{};
    int ammunition_before{};
};
struct ReplayTextureEvent {
    VehicleTextureMark mark;
};
struct ReplayDeformationEvent {
    VehicleDeformation deformation;
};
struct ReplayHealthEvent {
    float internal_hp{};
    std::array<float, 4> armor_hp{};
};
struct ReplayNitrousEvent {
    int ammunition_before{};
};
struct ReplayImpactEvent {
    Vec3 force{}, position{};
};
using ReplayEventData = std::variant<ReplayWeaponEvent, ReplayTextureEvent, ReplayDeformationEvent,
                                     ReplayHealthEvent, ReplayNitrousEvent, ReplayImpactEvent>;
struct ReplayEvent {
    float time{};
    ReplayEventData data;
    int native_type() const;
};
enum class ReplayEventMode { playback, seek_forward, seek_backward };
struct ReplayTarget {
    std::function<Result(const ReplayPose&)> pose;
    std::function<Result(const ReplayEvent&, ReplayEventMode)> event;
    std::function<Result()> reset_deformations;
};
class RaceReplayTrack {
  public:
    Result configure(float duration_seconds);
    void reset();
    Result record(float delta, const ReplayPose&);
    // Same bounded event insertion as0049e709. False means disabled/full/playback.
    bool record_event(ReplayEventData);
    Result begin(const ReplayTarget&);
    void end();
    Result advance(float delta, const ReplayTarget&);
    Result seek(float time, const ReplayTarget&);
    Result step(bool forward, const ReplayTarget&);
    bool ready() const { return records_.size() >= 2; }
    bool playing() const { return playing_; }
    float time() const { return cursor_; }
    float recorded_time() const { return elapsed_; }
    float first_time() const { return records_.empty() ? 0 : records_.front().time; }
    float last_time() const { return records_.empty() ? 0 : records_.back().time; }
    std::size_t capacity() const { return capacity_; }
    const std::deque<ReplayPoseRecord>& records() const { return records_; }
    std::span<const ReplayEvent> events() const { return events_; }

  private:
    Result apply_pose(float, const ReplayTarget&) const;
    std::deque<ReplayPoseRecord> records_;
    std::vector<ReplayEvent> events_;
    std::size_t capacity_{}, event_cursor_{};
    float elapsed_{}, sample_elapsed_{}, cursor_{}, duration_{};
    bool playing_{};
};
} // namespace roadwars
