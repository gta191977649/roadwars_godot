#include "game_src/unresolved/race_start.hpp"
#include <algorithm>
#include <cmath>

namespace roadwars {
namespace {
bool find_camera(const MeshObject& node, Vec3 parent, std::string_view name, Vec3& result) {
    parent = {parent.x + node.position.x, parent.y + node.position.y, parent.z + node.position.z};
    if (node.name == name) {
        result = parent;
        return true;
    }
    for (const auto& child : node.children)
        if (find_camera(child, parent, name, result))
            return true;
    return false;
}
CameraFrame looking_at(Vec3 eye, Vec3 target) {
    const float x = target.x - eye.x, y = target.y - eye.y, z = target.z - eye.z;
    return original_camera_frame(eye, target, std::atan2(x, z) * 57.295779513f,
                                 std::atan2(y, std::hypot(x, z)) * 57.295779513f);
}
} // namespace
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004bfb94
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00487fbe
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004848ee
Result RaceStart::load(ResourceStore& resources, const MeshObject& world, std::string_view car,
                       std::span<const TrackSpawn> spawns, const TrackCollision&,
                       std::string& error) {
    cameras_ = {};
    center_ = {};
    if (spawns.empty())
        return Result::invalid_argument;
    for (int i = 0; i < 3; ++i)
        find_camera(world, {}, "StartCam" + std::to_string(i + 1), cameras_[i]);
    for (const auto& spawn : spawns) {
        center_.x += spawn.position.x;
        center_.y += spawn.position.y;
        center_.z += spawn.position.z;
    }
    center_.x /= spawns.size();
    center_.y /= spawns.size();
    center_.z /= spawns.size();
    if (chase_.load(resources, car, error) != Result::ok)
        return Result::unavailable;
    Configuration config;
    if (config.load(resources, "Ini/RoadWars.ini") != Result::ok ||
        config.number("ROADWARS", "SFX", volume_) != Result::ok || !std::isfinite(volume_) ||
        volume_ < 0 || volume_ > 1) {
        error = "Original countdown SFX volume unavailable";
        return Result::unavailable;
    }
    constexpr const char* paths[]{"Sfx/3.wav", "Sfx/2.wav", "Sfx/1.wav", "Sfx/Go.wav"};
    for (int i = 0; i < 4; ++i) {
        std::vector<std::byte> bytes;
        if (resources.read(paths[i], bytes) != Result::ok ||
            (decode_wave_pcm(bytes, samples_[i]) != Result::ok &&
             decode_ms_adpcm_wave(bytes, samples_[i]) != Result::ok)) {
            error =
                "Original countdown sample unavailable or unsupported: " + std::string(paths[i]);
            return Result::unavailable;
        }
    }
    reset();
    return Result::ok;
}
void RaceStart::reset() {
    eye_ = cameras_[0];
    velocity_ = {};
    sounded_ = {};
    pending_sounds_.clear();
    acquired_ = false;
    remaining_ = 0;
    chase_.reset();
    camera_ = looking_at(eye_, center_);
}
int RaceStart::digit(float remaining) {
    return remaining > 0 && remaining < 4 ? static_cast<int>(remaining) : -1;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0048b120
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00484bcb
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004094fb
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004e553d
Result RaceStart::update(float remaining, float dt, const VehicleDynamics& vehicle,
                         const TrackCollision& track) {
    if (!std::isfinite(remaining) || !std::isfinite(dt) || dt < 0)
        return Result::invalid_argument;
    remaining_ = remaining;
    const int number = digit(remaining);
    if (number >= 0 && !sounded_[3 - number]) {
        sounded_[3 - number] = true;
        pending_sounds_.push_back(3 - number);
    }
    if (remaining >= 3) {
        const auto ground = track.sample(eye_);
        if (ground.hit)
            eye_.y = std::max(eye_.y, ground.point.y + 1);
        const auto target = cameras_[remaining < 5 ? 2 : remaining < 7 ? 1 : 0];
        // Original mode9 uses doubled dt and scalar1 from004e2cc7.
        const auto axis = [dt](float& eye, float& speed, float target) {
            const float difference = target - eye;
            if (std::abs(difference) / 2 <= std::abs(speed))
                speed = difference / 2;
            else
                speed += difference * (dt + dt);
            eye += speed * (dt + dt);
        };
        axis(eye_.x, velocity_.x, target.x);
        axis(eye_.y, velocity_.y, target.y);
        axis(eye_.z, velocity_.z, target.z);
        camera_ = looking_at(eye_, center_);
    } else {
        CameraFrame chase;
        if (chase_.update(vehicle.state(), vehicle.parameters(), &track, dt, chase) != Result::ok)
            return Result::unavailable;
        if (!acquired_) {
            const auto target = chase.transform.origin;
            const float x = target.x - eye_.x, y = target.y - eye_.y, z = target.z - eye_.z;
            const float distance = std::sqrt(x * x + y * y + z * z);
            const float divisor = remaining == 0 ? 0 : (remaining > 1 ? remaining - 1 : .1f);
            const float step = remaining == 0
                                   ? std::max(30.f, vehicle.state().cached_speed + 10) * dt
                                   : distance / divisor * dt;
            if (distance <= step || distance == 0) {
                eye_ = target;
                acquired_ = true;
            } else {
                eye_.x += x * step / distance;
                eye_.y += y * step / distance;
                eye_.z += z * step / distance;
            }
            camera_ = looking_at(eye_, chase.pivot);
        } else {
            camera_ = chase;
            eye_ = chase.transform.origin;
        }
    }
    return Result::ok;
}
} // namespace roadwars
