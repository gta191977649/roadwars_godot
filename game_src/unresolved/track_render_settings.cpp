#include "game_src/unresolved/track_render_settings.hpp"
#include "game_src/unresolved/binary_reader.hpp"
#include <algorithm>
#include <cmath>

namespace roadwars {
namespace {
bool contains(Vec3 point, const std::array<Vec3, 3>& triangle) {
    bool inside = false;
    std::size_t previous = 2;
    for (std::size_t current = 0; current < 3; ++current) {
        const auto a = triangle[current], b = triangle[previous];
        if (((a.z <= point.z && point.z < b.z) &&
             (point.x - a.x) * (b.z - a.z) < (point.z - a.z) * (b.x - a.x)) ||
            ((b.z <= point.z && point.z < a.z) &&
             (point.z - a.z) * (b.x - a.x) < (point.x - a.x) * (b.z - a.z)))
            inside = !inside;
        previous = current;
    }
    return inside;
}
bool valid_track(std::string_view track) {
    return !track.empty() && track != "." && track != ".." &&
           track.find_first_of("/\\:\r\n") == std::string_view::npos &&
           track.find('\0') == std::string_view::npos;
}
} // namespace

// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004b7439
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004db487
// Rendering-configuration subset only; race/AI/audio/resource ownership remain outside this helper.
Result load_track_render_settings(ResourceStore& resources, std::string_view track,
                                  TrackRenderSettings& settings, std::string& error) {
    if (!valid_track(track)) {
        error = "Invalid track name";
        return Result::invalid_argument;
    }
    Configuration config;
    const auto loaded = config.load(resources, "Tracks/" + std::string(track) + "/World/Data.txt");
    if (loaded != Result::ok) {
        error = "Track rendering configuration unavailable";
        return loaded;
    }
    TrackRenderSettings result;
    Configuration global;
    if (global.load(resources, "Ini/RoadWars.ini") != Result::ok ||
        global.number("ROADWARS", "VIEWDISTANCESCALAR", result.view_distance_scalar) !=
            Result::ok ||
        result.view_distance_scalar <= 0) {
        error = "Invalid global view-distance scalar";
        return Result::invalid_argument;
    }
    float red{}, green{}, blue{}, sky{};
    for (auto item :
         {std::pair{"VIEWDISTANCE(M)", &result.configured_view_distance}, std::pair{"FOGR", &red},
          std::pair{"FOGG", &green}, std::pair{"FOGB", &blue}, std::pair{"USESKY", &sky}}) {
        if (config.number("DATA", item.first, *item.second) != Result::ok) {
            error = "Missing track rendering setting: " + std::string(item.first);
            return Result::invalid_argument;
        }
    }
    for (auto value : {red, green, blue}) {
        if (value < 0 || value > 255 || std::floor(value) != value) {
            error = "Invalid track fog color";
            return Result::invalid_argument;
        }
    }
    if (result.configured_view_distance <= result.near_distance || std::floor(sky) != sky) {
        error = "Invalid track render distance or sky switch";
        return Result::invalid_argument;
    }
    result.fog_color = {red / 255.0f, green / 255.0f, blue / 255.0f};
    result.use_sky = sky != 0;
    // 004f267d -> 004fe105/004fef80: yaw45, pitch35, roll0, then rotate +Z.
    constexpr float kLightDegreesToRadians = 0.01745329238474369f;
    const float yaw = 45.0f * kLightDegreesToRadians;
    const float pitch = 35.0f * kLightDegreesToRadians;
    const float cy = static_cast<float>(std::cos(static_cast<double>(yaw * .5f)));
    const float sy = static_cast<float>(std::sin(static_cast<double>(yaw * .5f)));
    const float cx = static_cast<float>(std::cos(static_cast<double>(pitch * .5f)));
    const float sx = static_cast<float>(std::sin(static_cast<double>(pitch * .5f)));
    const float qx = cy * sx, qy = sy * cx, qz = -sy * sx, qw = cy * cx;
    const float tx = qx * 2, ty = qy * 2, tz = qz * 2;
    result.directional_light = {tz * qx + ty * qw, tz * qy - tx * qw, (1.0f - tx * qx) - ty * qy};
    result.sky_path = "Tracks/" + std::string(track) + "/Sky/Sky.msh";
    settings = std::move(result);
    error.clear();
    return Result::ok;
}

Result ViewDistanceMap::load(ResourceStore& resources, std::string_view track, std::string& error) {
    if (!valid_track(track)) {
        error = "Invalid track name";
        return Result::invalid_argument;
    }
    std::vector<std::byte> bytes;
    const auto result =
        resources.read("Tracks/" + std::string(track) + "/World/ViewDistanceMap.bin", bytes);
    if (result != Result::ok) {
        error = "Original view-distance map unavailable";
        return result;
    }
    return decode(bytes, error);
}

// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004cd7b0
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004cdd7e
Result ViewDistanceMap::decode(std::span<const std::byte> bytes, std::string& error) {
    try {
        BinaryReader reader(bytes);
        const auto count = reader.u32();
        if (count == 0 || count > 100000 || count > reader.remaining() / 8)
            throw std::runtime_error("Invalid view-distance region count");
        std::vector<Region> regions;
        float maximum{};
        for (std::uint32_t i = 0; i < count; ++i) {
            Region region;
            const auto triangles = reader.u32();
            if (triangles > 500000 || triangles > reader.remaining() / 36)
                throw std::runtime_error("Invalid view-distance triangle count");
            for (std::uint32_t j = 0; j < triangles; ++j) {
                std::array<Vec3, 3> triangle;
                for (auto& point : triangle) {
                    const float x = reader.f32(), y = reader.f32(), z = reader.f32();
                    point = {-x, y, -z};
                }
                region.triangles.push_back(triangle);
            }
            region.distance = reader.f32();
            if (region.distance <= 1)
                throw std::runtime_error("Invalid view-distance value");
            maximum = std::max(maximum, region.distance);
            regions.push_back(std::move(region));
        }
        if (reader.remaining())
            throw std::runtime_error("Unexpected view-distance trailing data");
        regions_ = std::move(regions);
        maximum_distance_ = maximum;
        cached_region_.reset();
        error.clear();
        return Result::ok;
    } catch (const std::exception& failure) {
        error = failure.what();
        return Result::invalid_argument;
    }
}

// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004cde15
// Preserve region caching and original 450 fallback; host diagnostic dialog is not recovered.
float ViewDistanceMap::sample(Vec3 camera_position) {
    const auto has_point = [&](const Region& region) {
        return std::any_of(
            region.triangles.begin(), region.triangles.end(),
            [&](const auto& triangle) { return contains(camera_position, triangle); });
    };
    if (cached_region_ && has_point(regions_[*cached_region_]))
        return regions_[*cached_region_].distance;
    for (std::size_t i = 0; i < regions_.size(); ++i) {
        if (has_point(regions_[i])) {
            cached_region_ = i;
            return regions_[i].distance;
        }
    }
    cached_region_.reset();
    return 450.0f;
}

// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004fcc79
Transform original_sky_transform(Vec3 camera_position, float far_distance) {
    const float scale = far_distance - far_distance / 20.0f;
    Transform transform;
    transform.basis = {{{scale, 0, 0}, {0, scale - scale / 4.5f, 0}, {0, 0, scale}}};
    transform.origin = {camera_position.x, 0, camera_position.z};
    return transform;
}
} // namespace roadwars
