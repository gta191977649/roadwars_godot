#include "game_src/unresolved/race_replay.hpp"
#include "game_src/unresolved/original_camera.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace roadwars {
namespace {
bool finite(Vec3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
int angle_integer(float value) {
    if (!std::isfinite(value) || std::abs(value) > 10000000)
        return 0;
    int result = static_cast<int>(value);
    while (result < 0)
        result += 360;
    while (result > 360)
        result -= 360;
    return result;
}
Vec3 lerp(Vec3 a, Vec3 b, float t) {
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t};
}
} // namespace
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004600b5
std::uint32_t pack_replay_angles(float h, float p, float r) {
    return (static_cast<std::uint32_t>(angle_integer(h)) << 20) |
           (static_cast<std::uint32_t>(angle_integer(p)) << 10) |
           static_cast<std::uint32_t>(angle_integer(r));
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004601b7
std::array<float, 3> unpack_replay_angles(std::uint32_t value) {
    return {static_cast<float>(value >> 20), static_cast<float>((value >> 10) & 1023),
            static_cast<float>(value & 1023)};
}
int ReplayEvent::native_type() const {
    constexpr std::array<int, 6> types{0, 1, 2, 3, 4, 6};
    return types[data.index()];
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0049d705
Result RaceReplayTrack::configure(float seconds) {
    if (!std::isfinite(seconds) || seconds < 0 || seconds > 36000)
        return Result::invalid_argument;
    capacity_ = static_cast<std::size_t>(static_cast<int>(seconds)) * 10;
    duration_ = static_cast<float>(static_cast<int>(seconds));
    reset();
    return Result::ok;
}
void RaceReplayTrack::reset() {
    records_.clear();
    events_.clear();
    elapsed_ = sample_elapsed_ = cursor_ = 0;
    event_cursor_ = 0;
    playing_ = false;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0049e007
Result RaceReplayTrack::record(float dt, const ReplayPose& pose) {
    if (!std::isfinite(dt) || dt < 0 || !finite(pose.position) || !finite(pose.velocity) ||
        !std::isfinite(pose.heading) || !std::isfinite(pose.pitch) || !std::isfinite(pose.roll) ||
        !std::isfinite(pose.steering) || !std::isfinite(pose.wheel_spin) ||
        std::abs(pose.steering) > 10000000 || std::abs(pose.wheel_spin) > 10000000)
        return Result::invalid_argument;
    if (!capacity_ || playing_)
        return Result::ok;
    elapsed_ += dt;
    sample_elapsed_ += dt;
    if (sample_elapsed_ >= .1f || records_.empty()) {
        sample_elapsed_ = 0;
        records_.push_back({elapsed_, pose.position, pose.velocity,
                            pack_replay_angles(pose.heading, pose.pitch, pose.roll),
                            static_cast<int>(pose.steering * 10),
                            static_cast<int>(pose.wheel_spin * 10), pose.nitrous ? 1 : 0,
                            pose.route_node});
        if (records_.size() > capacity_)
            records_.pop_front();
    }
    return Result::ok;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0049e709
bool RaceReplayTrack::record_event(ReplayEventData event) {
    if (!capacity_ || playing_ || events_.size() >= capacity_)
        return false;
    events_.push_back({elapsed_, std::move(event)});
    return true;
}
Result RaceReplayTrack::begin(const ReplayTarget& target) {
    if (!ready() || !target.pose || !target.event || !target.reset_deformations)
        return Result::unavailable;
    playing_ = true;
    cursor_ = last_time();
    event_cursor_ = events_.size();
    const auto result = seek(first_time(), target);
    if (result != Result::ok)
        playing_ = false;
    return result;
}
void RaceReplayTrack::end() { playing_ = false; }
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0048ee23
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0048ef10
Result RaceReplayTrack::step(bool forward, const ReplayTarget& target) {
    if (!playing_ || records_.empty())
        return Result::unavailable;
    auto index = static_cast<std::size_t>(
        std::upper_bound(records_.begin(), records_.end(), cursor_,
                         [](float t, const auto& record) { return t < record.time; }) -
        records_.begin());
    if (index)
        --index;
    const auto start = records_[index].time;
    while (forward ? index + 1 < records_.size() : index > 0) {
        if (std::abs(records_[index].time - start) >= .25f)
            break;
        if (forward)
            ++index;
        else
            --index;
    }
    return seek(records_[index].time, target);
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0049e232
Result RaceReplayTrack::apply_pose(float time, const ReplayTarget& target) const {
    if (!target.pose || records_.empty())
        return Result::unavailable;
    auto after = std::lower_bound(records_.begin(), records_.end(), time,
                                  [](const auto& a, float t) { return a.time < t; });
    if (after == records_.end())
        after = std::prev(records_.end());
    const auto before = after == records_.begin() ? after : std::prev(after);
    const float fraction =
        after->time == before->time ? 1.f : (time - before->time) / (after->time - before->time);
    const auto a = unpack_replay_angles(before->packed_angles);
    const auto b = unpack_replay_angles(after->packed_angles);
    auto pitch = interpolate_original_camera_angle(a[1], b[1], fraction);
    auto roll = interpolate_original_camera_angle(a[2], b[2], fraction);
    if (pitch > 180)
        pitch -= 360;
    if (roll > 180)
        roll -= 360;
    return target.pose({lerp(before->position, after->position, fraction),
                        lerp(before->velocity, after->velocity, fraction),
                        interpolate_original_camera_angle(a[0], b[0], fraction), pitch, roll,
                        before->steering_tenths / 10.f, before->wheel_spin_tenths / 10.f,
                        before->nitrous != 0, after->route_node});
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0049da23
Result RaceReplayTrack::seek(float time, const ReplayTarget& target) {
    if (!playing_ || !std::isfinite(time) || !target.event || !target.reset_deformations)
        return Result::invalid_argument;
    time = std::clamp(time, first_time(), last_time());
    if (time < cursor_) {
        while (event_cursor_ && events_[event_cursor_ - 1].time > time) {
            const auto result =
                target.event(events_[--event_cursor_], ReplayEventMode::seek_backward);
            if (result != Result::ok)
                return result;
        }
        const auto result = target.reset_deformations();
        if (result != Result::ok)
            return result;
        for (std::size_t i = 0; i < event_cursor_; ++i)
            if (events_[i].native_type() == 2) {
                const auto applied = target.event(events_[i], ReplayEventMode::seek_forward);
                if (applied != Result::ok)
                    return applied;
            }
    } else {
        while (event_cursor_ < events_.size() && events_[event_cursor_].time <= time) {
            const auto result =
                target.event(events_[event_cursor_++], ReplayEventMode::seek_forward);
            if (result != Result::ok)
                return result;
        }
    }
    cursor_ = time;
    return apply_pose(time, target);
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0049dd5e
Result RaceReplayTrack::advance(float dt, const ReplayTarget& target) {
    if (!playing_ || !std::isfinite(dt) || dt < 0 || !target.event)
        return Result::invalid_argument;
    const auto candidate = cursor_ + dt;
    // Original0049e232 guards absolute recorder time, including after ring rollover.
    if (candidate >= duration_) {
        cursor_ = candidate;
        return Result::ok;
    }
    const auto next = std::min(last_time(), candidate);
    if (apply_pose(next, target) != Result::ok)
        return Result::unavailable;
    while (event_cursor_ < events_.size() && events_[event_cursor_].time <= next) {
        const auto result = target.event(events_[event_cursor_++], ReplayEventMode::playback);
        if (result != Result::ok)
            return result;
    }
    cursor_ = next;
    return Result::ok;
}
} // namespace roadwars
