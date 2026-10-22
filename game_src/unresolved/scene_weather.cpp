#include "game_src/unresolved/scene_weather.hpp"
#include <cmath>

namespace roadwars {
namespace {
constexpr float kRadians = .017455555498600006f;
using Matrix = std::array<std::array<float, 3>, 3>;
Matrix product(const Matrix& a, const Matrix& b) {
    Matrix result{};
    for (unsigned i = 0; i < 3; ++i)
        for (unsigned j = 0; j < 3; ++j)
            for (unsigned k = 0; k < 3; ++k)
                result[i][j] += a[i][k] * b[k][j];
    return result;
}
float normalize_once(float value) {
    if (value < 0)
        return value + 360;
    if (value >= 360)
        return value - 360;
    return value;
}
//004d1130 uses reverse-order helper004d74e3: its Y/Z/X calls yield X*Z*Y.
Vec3 spawn_offset(Vec3 p, const SceneWeatherCamera& camera, float previous_heading, float dt) {
    const float turn = std::abs(normalize_once(previous_heading) - normalize_once(camera.heading));
    const float yaw = (turn / dt + camera.heading) * kRadians;
    const float roll = camera.roll * kRadians, pitch = -camera.pitch * kRadians;
    const float cy = std::cos(yaw), sy = std::sin(yaw);
    const float cz = std::cos(roll), sz = std::sin(roll);
    const float cx = std::cos(pitch), sx = std::sin(pitch);
    const Matrix y{{{cy, 0, -sy}, {0, 1, 0}, {sy, 0, cy}}};
    const Matrix z{{{cz, sz, 0}, {-sz, cz, 0}, {0, 0, 1}}};
    const Matrix x{{{1, 0, 0}, {0, cx, sx}, {0, -sx, cx}}};
    const auto m = product(x, product(z, y));
    return {p.z * m[2][0] + p.y * m[1][0] + p.x * m[0][0],
            p.z * m[2][1] + p.y * m[1][1] + p.x * m[0][1],
            p.z * m[2][2] + p.y * m[1][2] + p.x * m[0][2]};
}
bool finite(Vec3 p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }
} // namespace
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004d0bf0
// Actual track-selected64-slot snow mesh configuration; other parameterizations remain separate.
Result SceneWeather::load(ResourceStore& resources, std::string_view track, std::string& error) {
    Configuration config;
    float weather{};
    if (config.load(resources, "Tracks/" + std::string(track) + "/World/Data.txt") != Result::ok ||
        config.number("DATA", "WEATHER", weather) != Result::ok || std::floor(weather) != weather) {
        error = "Original track WEATHER configuration unavailable";
        return Result::unavailable;
    }
    SceneWeather next;
    next.enabled_ = weather == 1;
    if (next.enabled_) {
        std::vector<std::byte> bytes;
        if (resources.read("Meshes/snowflake.msh", bytes) != Result::ok ||
            decode_msh(bytes, next.mesh_, error) != Result::ok) {
            error = "Original WEATHER snowflake mesh unavailable";
            return Result::unavailable;
        }
        next.mesh_.position = {};
        next.mesh_.dummy_heading_degrees = 0;
    }
    *this = std::move(next);
    return Result::ok;
}
void SceneWeather::observe_callbacks(std::function<void(std::uint64_t, bool)> observer) {
    if (enabled_ && observer)
        observer(1, true);
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004d1385
// Actual camera/grid/ground spawn. Initial uninitialized native camera history remains explicit.
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004d15bf
// Actual snow callback motion/visibility and one random spawn attempt per strict timer threshold.
Result SceneWeather::advance(float dt, const SceneWeatherCamera& camera,
                             const TrackCollision& track,
                             const std::function<std::uint32_t()>& random, std::string& error) {
    if (!enabled_)
        return Result::ok;
    if (!std::isfinite(dt) || dt < 0 || !finite(camera.position) ||
        !std::isfinite(camera.heading) || !std::isfinite(camera.pitch) ||
        !std::isfinite(camera.roll))
        return Result::invalid_argument;
    if (!seeded_) {
        if (dt != 0) {
            error = "Original weather camera history must be supplied before the first spawn";
            return Result::unavailable;
        }
        seed_camera_history(camera);
    }
    elapsed_ += dt;
    if (elapsed_ > .014999999664723873f) {
        if (!random || dt == 0) {
            error = "Original weather shared RNG/spawn delta unavailable";
            return Result::unavailable;
        }
        elapsed_ = 0;
        std::size_t selected = random() % particles_.size();
        std::size_t scanned{};
        while (scanned < particles_.size() && particles_[selected].visible) {
            selected = (selected + 1) % particles_.size();
            ++scanned;
        }
        if (scanned < particles_.size()) {
            auto& p = particles_[selected];
            Vec3 local{static_cast<float>(selected % 8) * 4 - 16 + .1f, 3,
                       static_cast<float>(selected / 8) * 4 + .30000001192092896f};
            const auto delta = Vec3{camera.position.x - previous_.position.x,
                                    camera.position.y - previous_.position.y,
                                    camera.position.z - previous_.position.z};
            local.z += std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z) / dt;
            const auto transformed = spawn_offset(local, camera, previous_.heading, dt);
            p.position = {transformed.x + camera.position.x,
                          transformed.y + camera.position.y + 10000,
                          transformed.z + camera.position.z};
            p.ground_height = track.sample(p.position).point.y;
            p.position.y = p.ground_height + 6;
            p.spawn_position = p.position;
            p.age = 0;
            p.visible = true;
        }
    }
    for (auto& p : particles_) {
        if (!p.visible)
            continue;
        p.heading = camera.heading + 180;
        p.pitch = -camera.pitch;
        p.position.y += dt * -4.900000095367432f;
        p.position.x += p.positive_x ? dt : -dt;
        if ((p.positive_x && p.spawn_position.x + .5f < p.position.x) ||
            (!p.positive_x && p.position.x < p.spawn_position.x - .5f))
            p.positive_x = !p.positive_x;
        p.position.z += p.positive_z ? dt : -dt;
        if ((p.positive_z && p.spawn_position.z + .5f < p.position.z) ||
            (!p.positive_z && p.position.z < p.spawn_position.z - .5f))
            p.positive_z = !p.positive_z;
        p.age += dt;
        if (p.position.y < p.ground_height || p.age > 3)
            p.visible = false;
    }
    previous_ = camera;
    return Result::ok;
}
} // namespace roadwars
