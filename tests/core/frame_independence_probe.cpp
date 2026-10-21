#include "game_src/core/simulation_clock.hpp"
#include "game_src/unresolved/vehicle_dynamics.hpp"
#include <bit>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace roadwars;
void require(bool value) {
    if (!value)
        throw std::runtime_error("Render-rate physics divergence");
}
class Reader : public ResourceReader {
  public:
    explicit Reader(std::filesystem::path root) : root_(std::move(root)) {}
    Result read(std::string_view path, std::vector<std::byte>& out) override {
        std::ifstream stream(root_ / std::filesystem::path(path), std::ios::binary | std::ios::ate);
        if (!stream)
            return Result::not_found;
        const auto length = stream.tellg();
        if (length < 0)
            return Result::invalid_argument;
        out.resize(static_cast<std::size_t>(length));
        stream.seekg(0);
        stream.read(reinterpret_cast<char*>(out.data()), length);
        return stream ? Result::ok : Result::invalid_argument;
    }

  private:
    std::filesystem::path root_;
};
InputState recorded_input(std::uint64_t tick) {
    InputState input;
    if (tick >= 60 && tick < 360)
        input.throttle = 1;
    if (tick >= 240 && tick < 360)
        input.steering = 1;
    if (tick >= 360 && tick < 480)
        input.brake = 1;
    if (tick == 600)
        input.reset_requested = true;
    if (tick >= 660)
        input.throttle = 1;
    return input;
}
std::vector<std::uint32_t> run(const VehicleParameters& parameters, const TrackCollision& track,
                               const std::vector<double>& frames) {
    VehicleDynamics vehicle;
    std::string error;
    require(vehicle.initialize(parameters, track, error) == Result::ok);
    SimulationClock clock;
    std::vector<std::uint32_t> trace;
    auto number = [&](float value) { trace.push_back(std::bit_cast<std::uint32_t>(value)); };
    auto vector = [&](Vec3 v) {
        number(v.x);
        number(v.y);
        number(v.z);
    };
    auto tick = [&](float dt, const InputState&) {
        vehicle.update(dt, recorded_input(clock.ticks()));
        const auto& s = vehicle.state();
        vector(s.position);
        vector(s.velocity);
        for (float value : {s.heading_degrees, s.pitch_degrees, s.roll_degrees, s.yaw_rate,
                            s.pitch_rate, s.roll_rate, s.rpm, s.cached_speed, s.steering_degrees,
                            s.cached_slip_angle, s.engine_throttle, s.brake_amount})
            number(value);
        trace.push_back(static_cast<std::uint32_t>(s.gear_index));
        trace.push_back(static_cast<std::uint32_t>(vehicle.wall_contact_count()));
        for (float value : s.air_time)
            number(value);
        for (float value : vehicle.local_wheel_heights())
            number(value);
        for (const auto& source : vehicle.tire_particle_sources()) {
            vector(source.position);
            trace.push_back(source.skid_enabled);
            trace.push_back(source.emitting);
        }
        trace.push_back(static_cast<std::uint32_t>(vehicle.collision_audio_events().size()));
        for (const auto& event : vehicle.collision_audio_events()) {
            trace.push_back(static_cast<std::uint32_t>(event.sample));
            vector(event.position);
        }
        return Result::ok;
    };
    for (double frame : frames)
        require(clock.advance(frame, {}, tick) == Result::ok);
    while (clock.pending_seconds() + 1e-10 >= 1.0 / 60)
        require(clock.advance(0, {}, tick) == Result::ok);
    require(clock.ticks() == 1200);
    return trace;
}
int main(int argc, char** argv) {
    if (argc != 2)
        return 2;
    Reader reader(argv[1]);
    ResourceStore resources(reader);
    require(resources.open() == Result::ok);
    std::vector<std::byte> bytes;
    require(resources.read("Tracks/Dark Forest/World/SpatialObject.msh", bytes) == Result::ok);
    MeshObject mesh;
    std::string error;
    require(decode_msh(bytes, mesh, error) == Result::ok);
    TrackCollision track;
    require(track.load(resources, "Dark Forest", mesh, error) == Result::ok);
    VehicleParameters p;
    require(load_vehicle_metrics(resources, "Animal", p, error) == Result::ok);
    require(load_vehicle_geometry(resources, "Animal", p, error) == Result::ok);
    const auto reference = run(p, track, std::vector<double>(1200, 1.0 / 60));
    for (int fps : {10, 15, 30, 60, 75, 120, 144, 240}) {
        require(run(p, track, std::vector<double>(fps * 20, 1.0 / fps)) == reference);
        std::cout << fps << " render FPS: 1200 physics ticks, bit-identical complete trace\n";
    }
    std::vector<double> jitter;
    double total = 0;
    std::size_t i = 0;
    const std::array<double, 6> pattern{1.0 / 240, 1.0 / 30, .35, 1.0 / 144, .002, .9};
    while (total < 20) {
        const auto delta = std::min(pattern[i++ % pattern.size()], 20 - total);
        jitter.push_back(delta);
        total += delta;
    }
    require(run(p, track, jitter) == reference);
    std::cout
        << "Jitter and 350/900ms stalls: bit-identical complete trace; no elapsed time dropped\n";
}
