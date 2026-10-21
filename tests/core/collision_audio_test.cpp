#include "game_src/unresolved/collision_audio.hpp"
#include "game_src/unresolved/contact_orientation.hpp"
#include "game_src/unresolved/vehicle_dynamics.hpp"
#include <cassert>
#include <cmath>
#include <limits>
using namespace roadwars;
int main() {
    CollisionAudio audio;
    assert(audio.advance(0) == Result::ok);
    assert(!audio.wall_contact(3, {}));
    assert(audio.wall_contact(3.01f, {1, 2, 3}));
    assert(!audio.wall_contact(100, {}));
    assert(audio.events().size() == 1 && audio.events()[0].position.y == 2);
    assert(audio.advance(.499f) == Result::ok);
    assert(!audio.wall_contact(100, {}));
    assert(audio.advance(.002f) == Result::ok);
    assert(audio.wall_contact(100, {}));
    // Independent original car/car timer, not the wall timer or relative speed.
    assert(!audio.vehicle_contact(1000, {}));
    assert(audio.vehicle_contact(1001, {4, 5, 6}));
    assert(!audio.vehicle_contact(10000, {}));
    assert(audio.advance(.2f) == Result::ok);
    assert(audio.vehicle_contact(1001, {}));
    assert(!audio.wheel_landing(.5f, {}));
    for (unsigned wheel = 0; wheel < 4; ++wheel)
        assert(audio.wheel_landing(.501f, {}));
    assert(audio.events().size() == 5);
    audio.reset();
    assert(audio.events().empty());
    assert(audio.wall_contact(4, {}));
    assert(audio.advance(0) == Result::ok && audio.events().empty());
    assert(audio.advance(std::numeric_limits<float>::quiet_NaN()) == Result::invalid_argument);
    CollisionAudioResources resources;
    resources.sfx_volume = .7f;
    resources.samples[0].sample_rate = 22050;
    resources.samples[1].sample_rate = 11025;
    const auto command = collision_audio_command({CollisionSample::land, {7, 8, 9}}, resources);
    assert(command.action == EngineVoiceAction::start && !command.loop);
    assert(command.frequency_hz == 11025 && command.pitch_scale == 1);
    assert(command.qmdx_volume == 22936 && command.position.z == 9);
    assert(command.velocity.x == 0 && command.velocity.y == 0 && command.velocity.z == 0);
    // The actual suspension landing branch must dispatch before resetting air time.
    VehicleParameters p;
    VehicleState state;
    state.position = {10, 20, 30};
    state.air_time = {.51f, .5f, 1, 1};
    ContactOrientationState body;
    std::array<float, 4> heights{};
    std::array<Vec3, 4> points{};
    std::array<GroundContact, 4> contacts{};
    for (auto& contact : contacts) {
        contact.point = {};
        contact.normal = {0, 1, 0};
    }
    audio.reset();
    update_wheel_suspension(p, state, body, heights, points, contacts, &audio);
    assert(audio.events().size() == 3 && audio.events()[0].position.y == 20);
    update_wheel_suspension(p, state, body, heights, points, contacts, &audio);
    assert(audio.events().size() == 3); // Same contact must not re-trigger.
}
