#include "game_src/unresolved/tire_audio.hpp"
#include "game_src/unresolved/vehicle_dynamics.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace roadwars;
namespace {
void require(bool condition) {
    if (!condition)
        throw std::runtime_error("Recovered tire audio contract failed");
}
class DirectoryReader final : public ResourceReader {
  public:
    explicit DirectoryReader(std::filesystem::path root) : root_(std::move(root)) {}
    Result read(std::string_view path, std::vector<std::byte>& out) override {
        std::ifstream file(root_ / std::filesystem::path(path), std::ios::binary | std::ios::ate);
        if (!file)
            return Result::not_found;
        const auto size = file.tellg();
        if (size < 0)
            return Result::invalid_argument;
        out.resize(static_cast<std::size_t>(size));
        file.seekg(0);
        file.read(reinterpret_cast<char*>(out.data()), size);
        return file ? Result::ok : Result::invalid_argument;
    }

  private:
    std::filesystem::path root_;
};
} // namespace
int main(int argc, char** argv) {
    TireAudio voice;
    EngineAudioCommand command;
    TireAudioInput input{true, .9f, "Sfx/skid.wav", {1, 2, 3}};
    require(voice.initialize(.8f) == Result::ok);
    require(voice.update(input, command) == Result::ok);
    require(command.action == EngineVoiceAction::start && command.loop && command.set_volume);
    require(!command.set_frequency && command.pitch_scale == 1);
    require(command.qmdx_volume == 26213 && command.velocity.x == 0);
    require(command.position.x == 1 && voice.playing());
    input.sample_path = "Sfx/another.wav";
    input.first_tire_longitudinal = .1f;
    require(voice.update(input, command) == Result::ok);
    require(command.action == EngineVoiceAction::update && !command.set_volume);
    require(voice.active_sample_path() == "Sfx/skid.wav");
    input.any_skid = false;
    require(voice.update(input, command) == Result::ok);
    require(command.action == EngineVoiceAction::stop && !voice.playing());
    require(voice.update(input, command) == Result::ok);
    require(command.action == EngineVoiceAction::none);
    input.any_skid = true;
    input.first_tire_longitudinal = .6f;
    require(voice.update(input, command) == Result::ok);
    require(command.action == EngineVoiceAction::none && !voice.playing());
    input.first_tire_longitudinal = .9f;
    require(voice.update(input, command) == Result::ok);
    require(command.action == EngineVoiceAction::none); // Latch already set; no late start.
    input.any_skid = false;
    require(voice.update(input, command) == Result::ok);
    input.any_skid = true;
    require(voice.update(input, command) == Result::ok);
    require(command.action == EngineVoiceAction::start);
    require(voice.active_sample_path() == "Sfx/another.wav");
    voice.reset();
    input.sample_path.clear();
    require(voice.update(input, command) == Result::ok);
    require(!voice.playing());
    input.position.x = std::numeric_limits<float>::quiet_NaN();
    require(voice.update(input, command) == Result::invalid_argument);
    require(voice.initialize(-1) == Result::invalid_argument);
    if (argc > 1) {
        DirectoryReader reader(argv[1]);
        ResourceStore resources(reader);
        require(resources.open() == Result::ok);
        TireAudioResources sounds;
        std::string error;
        require(load_tire_audio(resources, "Dark Forest", sounds, error) == Result::ok);
        require(sounds.samples.size() == 1);
        const auto& sample = sounds.samples.at("Sfx/skid.wav");
        require(sample.channels == 1 && sample.bits_per_sample == 8 && sample.sample_rate == 22050);
        require(sample.frame_count() == 21376);
        // Original asset driving must reach the gate before any injected PCM fixture.
        std::vector<std::byte> bytes;
        MeshObject scene;
        TrackCollision track;
        VehicleParameters parameters;
        require(resources.read("Tracks/Dark Forest/World/SpatialObject.msh", bytes) == Result::ok);
        require(decode_msh(bytes, scene, error) == Result::ok);
        require(track.load(resources, "Dark Forest", scene, error) == Result::ok);
        require(load_vehicle_metrics(resources, "Animal", parameters, error) == Result::ok);
        require(load_vehicle_geometry(resources, "Animal", parameters, error) == Result::ok);
        VehicleDynamics vehicle;
        require(vehicle.initialize(parameters, track, error) == Result::ok);
        require(voice.initialize(sounds.sfx_volume) == Result::ok);
        std::uint32_t actual_starts{};
        for (int frame = 0; frame < 180; ++frame) {
            InputState driving;
            driving.throttle = frame < 60 ? 1.0f : 0.0f;
            driving.handbrake = frame >= 60 && frame < 120;
            vehicle.update(1.0f / 60, driving);
            require(voice.update(vehicle.tire_audio_input(), command) == Result::ok);
            if (command.action == EngineVoiceAction::start) {
                ++actual_starts;
                require(frame == 60);
                require(vehicle.state().cached_speed > 9);
                require(vehicle.tire_audio_input().first_tire_longitudinal == .9f);
                require(voice.active_sample_path() == "Sfx/skid.wav");
            }
        }
        require(actual_starts == 1 && !voice.playing());
        std::cout << "Actual original-asset tire onset frame=60, grip=0.9, starts=" << actual_starts
                  << '\n';
    }
    std::cout << "Recovered tire audio contracts passed\n";
}
