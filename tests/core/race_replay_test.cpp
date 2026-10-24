#include "game_src/unresolved/race_replay.hpp"
#include <cassert>
#include <cmath>
#include <limits>
#include <vector>
using namespace roadwars;
int main() {
    assert((unpack_replay_angles(pack_replay_angles(-1.9f, 360, 721.9f)) ==
            std::array<float, 3>{359, 360, 1}));
    RaceReplayTrack track;
    assert(track.configure(0) == Result::ok && !track.ready());
    assert(!track.record_event(ReplayNitrousEvent{3}));
    assert(track.configure(-1) == Result::invalid_argument);
    assert(track.configure(1) == Result::ok && track.capacity() == 10);
    ReplayPose pose;
    pose.heading = 359;
    assert(track.record(.01f, pose) == Result::ok && track.records().size() == 1);
    assert(track.record(.0625f, pose) == Result::ok && track.records().size() == 1);
    assert(track.record_event(ReplayWeaponEvent{WeaponSlot::front, 12}));
    assert(track.record_event(ReplayHealthEvent{50, {10, 20, 30, 40}}));
    pose.heading = 1;
    pose.position.x = 10;
    pose.velocity.x = 2;
    assert(track.record(.0625f, pose) == Result::ok && track.records().size() == 2);
    assert(track.record_event(ReplayDeformationEvent{{{1, 2, 3}, {4, 5, 6}}}));
    ReplayPose displayed;
    std::vector<std::pair<int, ReplayEventMode>> events;
    unsigned resets{};
    ReplayTarget target{[&](const ReplayPose& p) {
                            displayed = p;
                            return Result::ok;
                        },
                        [&](const ReplayEvent& e, ReplayEventMode mode) {
                            events.emplace_back(e.native_type(), mode);
                            return Result::ok;
                        },
                        [&] {
                            ++resets;
                            return Result::ok;
                        }};
    assert(track.begin(target) == Result::ok && resets == 1);
    assert(displayed.position.x == 0 && displayed.heading == 359);
    events.clear();
    assert(track.advance((track.last_time() - track.first_time()) * .5f, target) == Result::ok);
    assert(std::abs(displayed.position.x - 5) < .001f);
    assert(displayed.heading == 0 || std::abs(displayed.heading - 360) < .001f);
    assert(events.size() == 2 && events[0].first == 0 && events[1].first == 3);
    assert(events[0].second == ReplayEventMode::playback);
    events.clear();
    assert(track.seek(track.first_time(), target) == Result::ok);
    assert(events.size() == 2 && events[0].first == 3 && events[1].first == 0);
    assert(events[0].second == ReplayEventMode::seek_backward);
    events.clear();
    assert(track.seek(track.last_time(), target) == Result::ok);
    assert(events.size() == 3 && events.back().first == 2);
    for (const auto& e : events)
        assert(e.second == ReplayEventMode::seek_forward);
    assert(!track.record_event(ReplayNitrousEvent{2}));
    assert(track.advance(std::numeric_limits<float>::quiet_NaN(), target) ==
           Result::invalid_argument);
    track.end();
    assert(track.configure(1) == Result::ok);
    for (int i = 0; i < 15; ++i) {
        pose.position.x = static_cast<float>(i);
        assert(track.record(.1f, pose) == Result::ok);
        track.record_event(ReplayNitrousEvent{15 - i});
    }
    assert(track.records().size() == 10 && track.events().size() == 10);
    assert(track.records().front().position.x == 5 && track.records().back().position.x == 14);
    assert(track.begin(target) == Result::ok);
    assert(track.advance(1, target) == Result::ok && track.time() > 1);
    //0049e232's absolute configured-duration guard leaves the prior pose unchanged.
    assert(displayed.position.x == 5);
    track.end();
    ReplayTarget missing;
    assert(track.begin(missing) == Result::unavailable && !track.playing());
    target.event = [](const ReplayEvent&, ReplayEventMode) { return Result::unavailable; };
    // Rewinding from the last pose must surface missing native event support.
    assert(track.begin(target) == Result::unavailable && !track.playing());
}
