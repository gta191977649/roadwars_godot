#include "game_src/unresolved/original_camera.hpp"
#include <bit>
#include <cmath>
#include <map>
#include <source_location>
#include <stdexcept>
using namespace roadwars;
namespace {
void require(bool b, std::source_location where = std::source_location::current()) {
    if (!b)
        throw std::runtime_error("Original camera contract failed at line " +
                                 std::to_string(where.line()));
}
bool near(float a, float b) { return std::abs(a - b) < .001f; }
float radius(const CameraFrame& frame) {
    const auto p = frame.transform.origin, c = frame.pivot;
    return std::sqrt((p.x - c.x) * (p.x - c.x) + (p.y - c.y) * (p.y - c.y) +
                     (p.z - c.z) * (p.z - c.z));
}
float target_screen_y(const CameraFrame& frame) {
    const auto p = frame.transform.origin, c = frame.pivot;
    const Vec3 offset{c.x - p.x, c.y - p.y, c.z - p.z};
    const auto up = frame.transform.basis[1], backward = frame.transform.basis[2];
    return (offset.x * up.x + offset.y * up.y + offset.z * up.z) /
           -(offset.x * backward.x + offset.y * backward.y + offset.z * backward.z);
}

void u32(std::vector<std::byte>& b, std::uint32_t v) {
    for (unsigned i = 0; i < 32; i += 8)
        b.push_back(std::byte((v >> i) & 255));
}
void name(std::vector<std::byte>& b, std::string_view n) {
    for (unsigned i = 0; i < 32; ++i)
        b.push_back(i < n.size() ? std::byte(n[i]) : std::byte{});
}
class Reader : public ResourceReader {
  public:
    std::map<std::string, std::vector<std::byte>> files;
    Result read(std::string_view path, std::vector<std::byte>& out) override {
        auto i = files.find(std::string(path));
        if (i == files.end())
            return Result::not_found;
        out = i->second;
        return Result::ok;
    }
};
} // namespace
int main() {
    {
        OriginalTrackCamera camera;
        MeshObject world;
        world.position = {10, 0, 0};
        MeshObject first;
        first.name = "Camera1";
        first.position = {0, 5, 0};
        MeshObject second;
        second.name = "Camera2";
        second.position = {100, 9, 0};
        MeshObject gap;
        gap.name = "Camera4";
        gap.position = {200, 0, 0};
        world.children = {first, second, gap};
        std::string error;
        CameraFrame frame;
        require(camera.load(world, error) == Result::ok && camera.anchors().size() == 2);
        require(camera.update({100, 2, 0}, frame) == Result::ok);
        require(frame.transform.origin.x == 110 && frame.transform.origin.y == 9);
        require(camera.update({60, 2, 0}, frame) == Result::ok && frame.transform.origin.x == 10);
        require(camera.update({10, 5, 0}, frame) == Result::ok &&
                std::isfinite(frame.heading_degrees));
    }

    Reader reader;
    const std::string ini =
        "[CAMERAINFO]\nHOODZ=.7\nHOODX=.1\nHOODY=.6\nINCARX=.33\nINCARY=.2\nINCARZ=.6\n";
    for (char c : ini)
        reader.files["Cars/Animal/Data/Data.ini"].push_back(std::byte(c));
    auto& mesh = reader.files["Cars/Animal/Body/SpatialObject.msh"];
    name(mesh, "0Root");
    u32(mesh, 1);
    u32(mesh, 1);
    u32(mesh, std::bit_cast<std::uint32_t>(99.0f));
    u32(mesh, 0);
    u32(mesh, 0);
    u32(mesh, 0);
    name(mesh, "1024Body");
    u32(mesh, 1);
    u32(mesh, 1);
    u32(mesh, 0);
    u32(mesh, std::bit_cast<std::uint32_t>(.584f));
    u32(mesh, 0);
    u32(mesh, 0);
    name(mesh, "HoodPoint");
    u32(mesh, 1);
    u32(mesh, 0);
    u32(mesh, std::bit_cast<std::uint32_t>(.2f));
    u32(mesh, std::bit_cast<std::uint32_t>(.5f));
    u32(mesh, std::bit_cast<std::uint32_t>(1.2f));
    u32(mesh, 0);
    ResourceStore resources(reader);
    OriginalChaseCamera camera;
    std::string error;
    require(camera.load(resources, "Animal", error) == Result::ok);
    VehicleState vehicle;
    VehicleParameters params;
    CameraFrame frame;
    require(camera.update(vehicle, params, nullptr, .25f, frame) == Result::ok);
    require(near(frame.transform.origin.z, -6.5f) && near(frame.transform.origin.y, 1.884f));
    require(frame.vertical_fov_degrees == 50 && frame.transform.basis[2].z == -1);
    {
        const auto& settings = camera.settings();
        require(near(settings.in_car_local_position.x, -.33f) &&
                near(settings.in_car_local_position.y, .6f) &&
                near(settings.in_car_local_position.z, .2f));
        require(near(settings.hood_local_position.y, .7f) &&
                near(settings.hood_local_position.z, .6f));
        // Root's exported x99 is ignored; child HoodPoint includes body translation.
        require(near(settings.hood_point_local_position.x, .2f) &&
                near(settings.hood_point_local_position.y, 1.084f) &&
                near(settings.hood_point_local_position.z, 1.2f));
        CameraFrame view;
        auto selected = CameraView::near_chase;
        for (int i = 0; i < 4; ++i)
            selected = next_camera_view(selected);
        require(selected == CameraView::near_chase);
        require(camera_view_frame(selected, vehicle, params, settings, nullptr, frame, view) ==
                Result::ok);
        require(view.transform.origin.z == frame.transform.origin.z &&
                view.transform.basis[2].z == frame.transform.basis[2].z &&
                view.vertical_fov_degrees == frame.vertical_fov_degrees);
        require(camera_view_frame(CameraView::far_chase, vehicle, params, settings, nullptr,
                                  frame, view) == Result::ok);
        require(view.transform.origin.z < frame.transform.origin.z - 2 &&
                view.transform.origin.y > frame.transform.origin.y + .5f);
        require(camera_view_frame(CameraView::hood, vehicle, params, settings, nullptr, frame,
                                  view) == Result::ok);
        require(near(view.transform.origin.x, .1f) && near(view.transform.origin.y, 1.284f) &&
                near(view.transform.origin.z, .6f));
        auto tilted = vehicle;
        tilted.position = {11, 2, -5};
        tilted.heading_degrees = 90;
        tilted.pitch_degrees = 20;
        tilted.roll_degrees = -15;
        const auto basis = original_vehicle_transform(tilted.position, 90, 20, -15);
        const auto p = settings.hood_point_local_position;
        require(camera_view_frame(CameraView::cockpit, tilted, params, settings, nullptr, frame,
                                  view) == Result::ok);
        require(near(view.transform.origin.x,
                     11 + basis.basis[0].x * p.x + basis.basis[1].x * p.y + basis.basis[2].x * p.z));
        require(near(view.transform.origin.y,
                     2 + basis.basis[0].y * p.x + basis.basis[1].y * p.y + basis.basis[2].y * p.z));
        require(view.transform.basis[1].x == basis.basis[1].x &&
                view.transform.basis[0].x == -basis.basis[0].x &&
                view.transform.basis[2].z == -basis.basis[2].z);
        require(tilted.heading_degrees == 90 && tilted.pitch_degrees == 20 &&
                tilted.roll_degrees == -15 && tilted.position.x == 11);
        require(camera_view_frame(static_cast<CameraView>(99), vehicle, params, settings, nullptr,
                                  frame, view) == Result::invalid_argument);
    }
    vehicle.engine_throttle = 1;
    require(camera.update(vehicle, params, nullptr, .1f, frame) == Result::ok);
    require(near(frame.transform.origin.z, -6.6f));
    vehicle.engine_throttle = 0;
    vehicle.brake_amount = 1;
    require(camera.update(vehicle, params, nullptr, .1f, frame) == Result::ok);
    require(near(frame.transform.origin.z, -6.5f));
    camera.reset();
    vehicle = {};
    vehicle.cached_speed = 3;
    vehicle.velocity = {3, 0, 0};
    require(camera.update(vehicle, params, nullptr, 0, frame) == Result::ok);
    require(std::abs(frame.heading_degrees - 90) < .02f);
    camera.reset();
    vehicle.cached_speed = 1;
    require(camera.update(vehicle, params, nullptr, 0, frame) == Result::ok &&
            frame.heading_degrees == 0);
    camera.reset();
    vehicle.cached_speed = 3;
    require(camera.update(vehicle, params, nullptr, 0, frame, OriginalCameraView{false, true}) ==
                Result::ok &&
            frame.heading_degrees == 0);
    require(interpolate_original_camera_angle(10, 30, .5f) == 20);
    require(interpolate_original_camera_angle(350, 10, .5f) == 360);
    require(interpolate_original_camera_angle(10, 350, .5f) == 0);
    const WallRecord wall{{-10, 0, -3}, {10, 0, -3}, {0, 0, 1}, 0};
    require(near(original_camera_wall_distance({0, 0, -6.5f}, 0, {}, 1, std::span(&wall, 1)), 3));
    MouseOrbitCamera orbit;
    CameraFrame result;
    const auto original = original_camera_frame({0, 2, -6}, {0, 1, 0}, 0, 0);
    require(orbit.update(original, .1f, {}, result) == Result::ok &&
            result.transform.origin.z == -6);
    require(orbit.update(original, .1f, {100, 20}, result) == Result::ok);
    require(near(orbit.yaw_offset_degrees(), 15) && near(orbit.pitch_offset_degrees(), -3));
    require(result.transform.origin.x < 0 && result.transform.origin.y > 2);
    require(orbit.update(original, 2.9f, {}, result) == Result::ok &&
            near(orbit.yaw_offset_degrees(), 15));
    require(orbit.update(original, .2f, {}, result) == Result::ok &&
            orbit.yaw_offset_degrees() < 15 && orbit.yaw_offset_degrees() > 10);
    for (int i = 0; i < 100; ++i)
        require(orbit.update(original, .1f, {}, result) == Result::ok);
    require(orbit.yaw_offset_degrees() == 0 && result.transform.origin.z == -6);
    require(orbit.state() == MouseOrbitState::original);
    require(orbit.update(original, 0, {100, 20}, result) == Result::ok);
    const auto held = result;
    const auto turned = original_camera_frame({4, 2, 5}, {10, 1, 5}, 90, 10);
    require(orbit.update(turned, 2.5f, {}, result) == Result::ok);
    require(orbit.state() == MouseOrbitState::manual_hold);
    require(near(result.heading_degrees, held.heading_degrees) &&
            near(result.pitch_degrees, held.pitch_degrees));
    require(near(result.transform.origin.x - turned.pivot.x,
                 held.transform.origin.x - original.pivot.x));
    require(near(result.transform.origin.z - turned.pivot.z,
                 held.transform.origin.z - original.pivot.z));
    require(orbit.update(turned, .6f, {}, result) == Result::ok &&
            orbit.state() == MouseOrbitState::returning);
    require(result.heading_degrees > held.heading_degrees &&
            result.heading_degrees < turned.heading_degrees);
    const auto during_return = result;
    require(orbit.update(turned, 0, {1, 0}, result) == Result::ok);
    require(orbit.state() == MouseOrbitState::manual_hold && orbit.idle_seconds() == 0);
    require(near(result.heading_degrees, during_return.heading_degrees + .15f));
    require(std::abs(result.transform.origin.x - during_return.transform.origin.x) < .03f);
    require(orbit.update(original, 2.9f, {}, result) == Result::ok &&
            orbit.state() == MouseOrbitState::manual_hold);
    require(near(result.heading_degrees, during_return.heading_degrees + .15f));
    for (int i = 0; i < 150; ++i)
        require(orbit.update(original, .1f, {}, result) == Result::ok);
    require(orbit.state() == MouseOrbitState::original &&
            result.transform.origin.z == original.transform.origin.z);
    orbit.reset();
    const auto wrapped_pitch = original_camera_frame({0, 2, -6}, {0, 1, 0}, 350, 350);
    require(orbit.update(wrapped_pitch, 0, {1, 0}, result) == Result::ok);
    const auto wrapped_forward = wrapped_pitch.transform.basis[2];
    const float wrapped_heading =
        std::atan2(-wrapped_forward.x, -wrapped_forward.z) * 57.29577951308232f;
    require(near(result.heading_degrees, wrapped_heading + .15f));
    require(orbit.update(original, -1, {}, result) == Result::invalid_argument);

    // The target keeps its original screen composition throughout a manual orbit.
    orbit.reset();
    const float baseline_radius = radius(original), baseline_screen_y = target_screen_y(original);
    for (int step = 0; step < 24; ++step) {
        require(orbit.update(original, 0, {100, step % 2 ? -8.0f : 8.0f}, result) == Result::ok);
        require(near(radius(result), baseline_radius));
        require(near(target_screen_y(result), baseline_screen_y));
        const auto position = result.transform.origin;
        require(std::isfinite(position.x) && std::isfinite(position.y) &&
                std::isfinite(position.z));
    }
    // A 180-degree return must travel around the pivot, not collapse its radius
    // along the chord between opposing eye positions.
    orbit.reset();
    require(orbit.update(original, 0, {1200, 0}, result) == Result::ok);
    require(orbit.update(original, 3, {}, result) == Result::ok);
    require(orbit.update(original, std::log(2.0f) / 3.0f, {}, result) == Result::ok);
    require(near(radius(result), baseline_radius));
    require(std::abs(result.transform.origin.x - original.pivot.x) > 5.9f);
    const auto halfway = result;
    require(orbit.update(original, 0, {1, 0}, result) == Result::ok);
    require(near(radius(result), baseline_radius));
    require(near(result.heading_degrees, halfway.heading_degrees + .15f));
    require(near(target_screen_y(result), target_screen_y(halfway)));
    require(std::abs(result.transform.origin.x - halfway.transform.origin.x) < .02f);
    // Pivot translation transports the same world-oriented sphere and view.
    const auto stationary = result;
    auto moved = original;
    moved.pivot = {11, -3, 7};
    moved.transform.origin = {11, -2, 1};
    require(orbit.update(moved, .1f, {}, result) == Result::ok);
    require(near(result.transform.origin.x - moved.pivot.x,
                 stationary.transform.origin.x - original.pivot.x));
    require(near(result.transform.origin.y - moved.pivot.y,
                 stationary.transform.origin.y - original.pivot.y));
    require(near(target_screen_y(result), target_screen_y(stationary)));
    require(orbit.update(moved, 0, {0, 100000}, result) == Result::ok);
    require(std::isfinite(target_screen_y(result)) && near(radius(result), baseline_radius));
    orbit.reset();
    require(orbit.update(original, 0, {2399, 0}, result) == Result::ok);
    const auto before_wrap = result;
    require(orbit.update(original, 0, {2, 0}, result) == Result::ok);
    require(std::abs(std::remainder(result.heading_degrees - before_wrap.heading_degrees, 360.0f) -
                     .3f) < .001f);
    require(near(radius(result), baseline_radius));
}
