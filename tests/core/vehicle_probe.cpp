#include "game_src/unresolved/vehicle_dynamics.hpp"
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace roadwars;
class TrackProbeReader final : public ResourceReader {
  public:
    explicit TrackProbeReader(std::filesystem::path root) : root_(std::move(root)) {}
    Result read(std::string_view path, std::vector<std::byte>& output) override {
        std::ifstream stream(root_ / std::filesystem::path(path), std::ios::binary | std::ios::ate);
        if (!stream)
            return Result::not_found;
        auto size = stream.tellg();
        if (size < 0)
            return Result::invalid_argument;
        output.resize(static_cast<std::size_t>(size));
        stream.seekg(0);
        stream.read(reinterpret_cast<char*>(output.data()), size);
        return stream ? Result::ok : Result::invalid_argument;
    }

  private:
    std::filesystem::path root_;
};
int main(int argc, char** argv) {
    std::cout << std::unitbuf;
    if (argc != 2)
        return 2;
    TrackProbeReader reader(argv[1]);
    ResourceStore resources(reader);
    if (resources.open() != Result::ok)
        return 1;
    std::vector<std::byte> bytes;
    if (resources.read("Tracks/Dark Forest/World/SpatialObject.msh", bytes) != Result::ok)
        return 1;
    std::string error;
    MeshObject scene;
    if (decode_msh(bytes, scene, error) != Result::ok) {
        std::cerr << error;
        return 1;
    }
    TrackCollision track;
    if (track.load(resources, "Dark Forest", scene, error) != Result::ok) {
        std::cerr << error;
        return 1;
    }
    VehicleParameters parameters;
    if (load_vehicle_metrics(resources, "Animal", parameters, error) != Result::ok ||
        load_vehicle_geometry(resources, "Animal", parameters, error) != Result::ok) {
        std::cerr << error;
        return 1;
    }
    VehicleDynamics vehicle;
    if (vehicle.initialize(parameters, track, error) != Result::ok) {
        std::cerr << error;
        return 1;
    }
    std::cout << "Geometry mass=" << parameters.mass_kg << " length=" << parameters.length
              << " width=" << parameters.width << " height=" << parameters.height
              << " tire radius=" << parameters.rear_wheel_radius
              << " wheelbase=" << parameters.wheelbase << '\n';
    const auto speed = [](Vec3 v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); };
    float heading_before_turn{};
    std::uint64_t prior_wall_contacts{};
    ParticleRandom particle_random;
    Configuration original_settings;
    float particle_scalar{};
    assert(original_settings.load(resources, "Ini/RoadWars.ini") == Result::ok);
    assert(original_settings.number("ROADWARS", "SPECIAL_EFFECTS_SCALAR", particle_scalar) ==
           Result::ok);
    ParticleEmitter sparks;
    assert(sparks.configure(vehicle_spark_settings(particle_scalar)) == Result::ok);
    std::array<TireParticleEmitter, 4> tire_particles;
    for (auto& emitter : tire_particles)
        assert(emitter.configure(particle_scalar, particle_random) == Result::ok);
    std::uint32_t spark_bursts{}, peak_tire_particles{}, wall_effect_events{};
    CollisionAudioResources collision_resources;
    assert(load_collision_audio(resources, collision_resources, error) == Result::ok);
    std::uint32_t crash_sounds{}, landing_sounds{};
    for (int frame = 0; frame < 1200; ++frame) {
        InputState input;
        if (frame >= 60 && frame < 360)
            input.throttle = 1;
        if (frame >= 240 && frame < 360)
            input.steering = 1;
        if (frame >= 360 && frame < 480)
            input.brake = 1;
        if (frame == 600)
            input.reset_requested = true;
        if (frame >= 660)
            input.throttle = 1;
        vehicle.update(1.0f / 60, input);
        for (const auto& event : vehicle.collision_audio_events()) {
            if (event.sample == CollisionSample::crash)
                ++crash_sounds;
            else
                ++landing_sounds;
            const auto command = collision_audio_command(event, collision_resources);
            assert(command.action == EngineVoiceAction::start && !command.loop &&
                   command.pitch_scale == 1 && command.frequency_hz > 0);
        }
        if (input.reset_requested) {
            sparks.reset();
            for (auto& emitter : tire_particles)
                emitter.reset();
        }
        wall_effect_events += static_cast<std::uint32_t>(vehicle.particle_impacts().size());
        for (const auto& impact : vehicle.particle_impacts())
            if (sparks.begin_burst(impact))
                ++spark_bursts;
        assert(sparks.advance(1.0f / 60, true, {}, {}, particle_random) == Result::ok);
        std::uint32_t active_tire_particles{};
        for (std::size_t wheel = 0; wheel < 4; ++wheel) {
            assert(tire_particles[wheel].advance(
                       1.0f / 60, vehicle.tire_particle_sources()[wheel]) == Result::ok);
            active_tire_particles += tire_particles[wheel].active_count();
        }
        peak_tire_particles = std::max(peak_tire_particles, active_tire_particles);
        const auto& state = vehicle.state();
        assert(std::isfinite(state.position.x) && std::isfinite(state.position.y) &&
               std::isfinite(state.position.z));
        assert(std::isfinite(state.velocity.x) && std::isfinite(state.velocity.y) &&
               std::isfinite(state.velocity.z));
        if (frame == 239) {
            assert(speed(state.velocity) > 15);
            heading_before_turn = state.heading_degrees;
        }
        if (frame == 359) {
            assert(std::abs(state.heading_degrees - heading_before_turn) > 1);
            assert(std::abs(state.steering_degrees) > 30);
        }
        if (frame == 479)
            assert(state.reverse_engaged && state.engine_throttle == -1 && state.gear_index == 0);
        if (frame == 599)
            prior_wall_contacts = vehicle.wall_contact_count();
        if (frame == 600) {
            assert(speed(state.velocity) == 0 && state.yaw_rate == 0 &&
                   state.steering_degrees == 0);
            assert(state.position.x == track.spawn().position.x &&
                   state.position.y == track.spawn().position.y &&
                   state.position.z == track.spawn().position.z);
        }
        if (frame % 60 == 0)
            std::cout << "t=" << frame / 60 << " position=" << state.position.x << ','
                      << state.position.y << ',' << state.position.z
                      << " heading=" << state.heading_degrees << " velocity=" << state.velocity.x
                      << ',' << state.velocity.y << ',' << state.velocity.z
                      << " steering=" << state.steering_degrees << " rpm=" << state.rpm
                      << " gear=" << state.gear_index + 1 << " throttle=" << state.engine_throttle
                      << " brake=" << state.brake_amount << '\n';
    }
    const auto wall_contacts = prior_wall_contacts + vehicle.wall_contact_count();
    std::cout << "Actual wall contacts=" << wall_contacts << '\n';
    assert(wall_contacts > 0);
    std::cout << "Spark bursts=" << spark_bursts << " peak tire particles=" << peak_tire_particles
              << '\n';
    assert(spark_bursts > 0 && peak_tire_particles > 0);
    assert(crash_sounds == wall_effect_events);
    std::cout << "Collision audio crash=" << crash_sounds << " land=" << landing_sounds
              << " crash_frames=" << collision_resources.samples[0].frame_count()
              << " land_frames=" << collision_resources.samples[1].frame_count() << '\n';
    assert(vehicle.initialize(parameters, track, error) == Result::ok);
    const auto measurement = [&](int step) {
        const auto& state = vehicle.state();
        std::cout << "Drivetrain t=" << step / 60.0f << " speed=" << speed(state.velocity)
                  << " rpm=" << state.rpm << " gear=" << state.gear_index + 1
                  << " position=" << state.position.x << ',' << state.position.y << ','
                  << state.position.z << " wall_contacts=" << vehicle.wall_contact_count() << '\n';
    };
    measurement(0);
    for (int step = 1; step <= 600; ++step) {
        InputState input;
        input.throttle = 1;
        vehicle.update(1.0f / 60, input);
        if (step == 180 || step == 360 || step == 600)
            measurement(step);
    }
    return 0;
}
