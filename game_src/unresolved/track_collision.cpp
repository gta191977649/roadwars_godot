#include "game_src/unresolved/track_collision.hpp"
#include "game_src/core/not_implemented.hpp"
#include "game_src/unresolved/binary_reader.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace roadwars {
namespace {
Vec3 read_vector(BinaryReader& reader) { return {reader.f32(), reader.f32(), reader.f32()}; }
bool triangle_contains(float x, float z, const std::array<Vec3, 3>& vertices) {
    unsigned result{};
    unsigned previous = 2;
    for (unsigned index = 0; index < 3; ++index) {
        const auto& current = vertices[index];
        const auto& last = vertices[previous];
        if (((current.z <= z && z < last.z) &&
             (x - current.x) * (last.z - current.z) < (z - current.z) * (last.x - current.x)) ||
            ((last.z <= z && z < current.z) &&
             (z - current.z) * (last.x - current.x) < (x - current.x) * (last.z - current.z)))
            result ^= 1;
        previous = index;
    }
    return result != 0;
}
float distance_xz(Vec3 a, Vec3 b) {
    const float x = a.x - b.x;
    const float z = a.z - b.z;
    return std::sqrt(x * x + z * z);
}
const MeshObject* find_start(const MeshObject& object, Vec3 parent, Vec3& position) {
    const Vec3 world{parent.x + object.position.x, parent.y + object.position.y,
                     parent.z + object.position.z};
    if (object.name == "Start1") {
        position = world;
        return &object;
    }
    for (const auto& child : object.children)
        if (const auto* result = find_start(child, world, position))
            return result;
    return nullptr;
}
} // namespace
namespace {
Vec3 add(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 subtract(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 scale(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
Vec3 normalized(Vec3 a) {
    const auto length = std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
    return length == 0 ? a : scale(a, 1.0f / length);
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004dd310
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0047c368
bool intersect(Vec3 a, Vec3 b, Vec3 c, Vec3 d, Vec3& point) {
    const float x = a.x - b.x, z = a.z - b.z, u = -(c.x - d.x), v = -(c.z - d.z);
    if ((u == 0 && v == 0) || (x == 0 && z == 0))
        return false;
    auto between = [](float p, float q, float r) {
        return (p <= q && r <= p) || (p <= r && q <= p);
    };
    if (u == 0 && x == 0) {
        if (a.x != c.x)
            return false;
        if (between(a.z, c.z, d.z) || between(b.z, c.z, d.z)) {
            point.x = a.x;
            point.z = (a.z + b.z + c.z + d.z) / 4.0f;
            return true;
        }
    }
    if (v == 0 && z == 0) {
        if (a.z != c.z)
            return false;
        if (between(a.x, c.x, d.x) || between(b.x, c.x, d.x)) {
            point.x = (a.x + b.x + c.x + d.x) / 4.0f;
            point.z = a.z;
            return true;
        }
    }
    if (u != 0 && x != 0 && z / x == v / u)
        return false;
    float t{}, s{};
    if (v == 0 || u == 0) {
        if (u == 0) {
            if (v != 0)
                t = (c.x - a.x) / x;
        } else
            t = (c.z - a.z) / z;
    } else
        t = ((c.z - a.z) + (c.x - a.x) * (-v / u)) / (x * (-v / u) + z);
    if (x == 0 || z == 0) {
        if (z == 0) {
            if (x != 0)
                s = (c.z - a.z) / v;
        } else
            s = (c.x - a.x) / u;
    } else
        s = ((c.z - a.z) + (c.x - a.x) * (-z / x)) / (u * (-z / x) + v);
    if (t > 0 || t < -1 || s > 0 || s < -1)
        return false;
    point.x = x * t + a.x;
    point.z = z * t + a.z;
    return true;
}
bool edge_hits(const std::array<Vec3, 4>& corners, std::span<const WallRecord> walls) {
    Vec3 point;
    for (unsigned edge = 0; edge < 4; ++edge)
        for (const auto& wall : walls)
            if (intersect(corners[edge], corners[(edge + 1) % 4], wall.lower, wall.upper, point))
                return true;
    return false;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00431499
float heading_delta(float previous, float current) {
    auto wrap = [](float a) {
        if (a < 0)
            a += 360;
        else if (a >= 360)
            a -= 360;
        return a;
    };
    previous = wrap(previous);
    current = wrap(current);
    float difference = previous - current;
    if (std::fabs(difference) > 180)
        difference = previous > current ? -(360 - previous + current) : (360 - current + previous);
    return -difference;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004315cd
bool same_side(Vec3 a, Vec3 b, Vec3 c, Vec3 d, float precision) {
    a.y = b.y = c.y = d.y = 0;
    const auto direction = normalized(subtract(a, b));
    const Vec3 normal{direction.z, 0, -direction.x};
    unsigned above{};
    for (const auto p : {subtract(a, c), subtract(a, d), subtract(b, c), subtract(b, d)})
        if (precision <= p.x * normal.x + p.z * normal.z)
            ++above;
    return above == 0 || above == 4;
}
} // namespace
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0047cb51
std::array<Vec3, 4> collision_footprint(const CollisionBody& body) {
    constexpr float radians = std::bit_cast<float>(std::uint32_t{0x3c8efef4});
    const auto first = (body.heading_degrees - body.corner_angle_degrees) * radians;
    const auto second = (body.heading_degrees + body.corner_angle_degrees) * radians;
    const Vec3 a{std::sin(first) * body.diagonal_radius, 0, std::cos(first) * body.diagonal_radius};
    const Vec3 b{std::sin(second) * body.diagonal_radius, 0,
                 std::cos(second) * body.diagonal_radius};
    const Vec3 center{body.position.x, 0, body.position.z};
    std::array<Vec3, 4> corners{add(center, a), add(center, b), subtract(center, a),
                                subtract(center, b)};
    if (body.front_extension != 0) {
        const auto extension =
            scale(normalized(subtract(corners[0], corners[3])), body.front_extension);
        corners[0] = add(corners[0], extension);
        corners[1] = add(corners[1], extension);
    }
    return corners;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00430f70
bool body_intersects_walls(const CollisionBody& body, std::span<const WallRecord> walls,
                           bool& enclosed_wall) {
    enclosed_wall = false;
    const auto corners = collision_footprint(body);
    if (edge_hits(corners, walls))
        return true;
    const std::array<Vec3, 3> first{corners[0], corners[1], corners[2]},
        second{corners[0], corners[2], corners[3]};
    auto contains = [&](Vec3 p) {
        return triangle_contains(p.x, p.z, first) || triangle_contains(p.x, p.z, second);
    };
    for (const auto& wall : walls)
        if (wall.parameter < body.radius + 1 && contains(wall.lower) && contains(wall.upper)) {
            enclosed_wall = true;
            return true;
        }
    return false;
}
namespace {
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00432002
bool collision_position(CollisionBody& previous, const CollisionBody& current,
                        std::span<const WallRecord> walls, bool enclosed,
                        CollisionBody& intersecting, CollisionBody& clear) {
    const auto saved = previous.position;
    unsigned corrections{};
    auto corners = collision_footprint(previous);
    for (int edge = 0; edge < 4; ++edge) {
        Vec3 point;
        for (const auto& wall : walls)
            if (intersect(corners[edge], corners[(edge + 1) % 4], wall.lower, wall.upper, point)) {
                auto direction = corrections >= 100 ? subtract(current.position, previous.position)
                                                    : scale(wall.normal, -1);
                previous.position = subtract(previous.position, scale(normalized(direction), 0.1f));
                corners = collision_footprint(previous);
                edge = -1;
                ++corrections;
                break;
            }
        if (corrections == 100)
            previous.position = saved;
        if (corrections > 200)
            return false;
    }
    const auto displacement = subtract(current.position, previous.position);
    const auto rotation = heading_delta(previous.heading_degrees, current.heading_degrees);
    const auto length =
        std::sqrt(displacement.x * displacement.x + displacement.y * displacement.y +
                  displacement.z * displacement.z);
    float tolerance = enclosed ? 0.001f : 0.01f;
    if (length > 1)
        tolerance /= length;
    float high = 1, low = 0, t = 1;
    bool found = false;
    while (true) {
        if (high - low <= tolerance) {
            intersecting = current;
            clear = current;
            intersecting.position = add(previous.position, scale(displacement, high));
            intersecting.heading_degrees = rotation * high + previous.heading_degrees;
            clear.position = add(previous.position, scale(displacement, low));
            clear.heading_degrees = rotation * low + previous.heading_degrees;
            return found;
        }
        auto probe = current;
        probe.position = add(previous.position, scale(displacement, t));
        probe.heading_degrees = rotation * t + previous.heading_degrees;
        const bool hit = edge_hits(collision_footprint(probe), walls);
        if (hit)
            found = true;
        if (!found) {
            high -= tolerance;
            t = high;
        } else if (!hit) {
            low = t;
            t = (high - t) * 0.5f + t;
        } else {
            high = t;
            t = (t - low) * 0.5f + low;
        }
    }
}
} // namespace
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004328b5
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004317ad
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00431a46
WallContact solve_wall_contact(const CollisionBody& previous, const CollisionBody& current,
                               std::span<const WallRecord> walls) {
    WallContact result;
    result.position = current.position;
    result.heading_degrees = current.heading_degrees;
    bool enclosed{};
    if (!body_intersects_walls(current, walls, enclosed))
        return result;
    auto prior = previous, intersecting = current, clear = current;
    unsigned involved = 4;
    std::array<unsigned, 4> edge_counts{};
    std::vector<unsigned> wall_counts(walls.size());
    std::vector<Vec3> contacts;
    auto restore = [&]() {
        result.restore_previous = true;
        result.position = previous.position;
        result.heading_degrees = previous.heading_degrees;
        return result;
    };
    for (unsigned iteration = 0; involved > 3 && iteration < 100; ++iteration) {
        const auto target = intersecting;
        if (!collision_position(prior, target, walls, enclosed, intersecting, clear))
            return restore();
        edge_counts.fill(0);
        std::fill(wall_counts.begin(), wall_counts.end(), 0);
        contacts.clear();
        const auto corners = collision_footprint(intersecting);
        for (unsigned edge = 0; edge < 4; ++edge)
            for (std::size_t wall = 0; wall < walls.size(); ++wall) {
                Vec3 point;
                if (intersect(corners[edge], corners[(edge + 1) % 4], walls[wall].lower,
                              walls[wall].upper, point)) {
                    contacts.push_back(point);
                    ++edge_counts[edge];
                    ++wall_counts[wall];
                }
            }
        involved = 0;
        for (auto count : edge_counts)
            involved += count != 0;
        for (auto count : wall_counts)
            involved += count != 0;
    }
    if (contacts.empty())
        return restore();
    for (auto point : contacts)
        result.contact_point = add(result.contact_point, point);
    result.contact_point = scale(result.contact_point, 1.0f / static_cast<float>(contacts.size()));
    result.contact_point.y = clear.position.y;
    const auto corners = collision_footprint(clear);
    bool found = false;
    float precision = 0;
    // Original increases precision without a bound until a separating normal is found.
    while (!found) {
        for (std::size_t wall = 0; wall < walls.size(); ++wall)
            if (wall_counts[wall])
                for (unsigned edge = 0; edge < 4; ++edge)
                    if (edge_counts[edge]) {
                        found = same_side(walls[wall].lower, walls[wall].upper, corners[edge],
                                          corners[(edge + 1) % 4], precision);
                        if (found)
                            result.normal = walls[wall].normal;
                    }
        if (!found)
            for (unsigned edge = 0; edge < 4; ++edge)
                if (edge_counts[edge])
                    for (std::size_t wall = 0; wall < walls.size(); ++wall)
                        if (wall_counts[wall]) {
                            found = same_side(corners[edge], corners[(edge + 1) % 4],
                                              walls[wall].lower, walls[wall].upper, precision);
                            if (found) {
                                result.normal = clear.edge_normals[edge];
                                if (result.normal.x == 0 && result.normal.y == 0 &&
                                    result.normal.z == 0)
                                    not_implemented(
                                        "00431a46:missing_vehicle_basis",
                                        "Collision normal requires original vehicle basis");
                            }
                        }
        precision += std::bit_cast<float>(std::uint32_t{0x38d1b717});
    }
    result.hit = true;
    result.position = clear.position;
    result.heading_degrees = clear.heading_degrees;
    const auto original_distance = distance_xz(current.position, previous.position);
    result.removed_fraction =
        original_distance == 0 ? 0
                               : std::min(1.0f, 1 - distance_xz(clear.position, previous.position) /
                                                        original_distance);
    return result;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004f2192
Result TrackCollision::Grid::decode(std::span<const std::byte> bytes,
                                    std::uint32_t expected_stride) {
    try {
        BinaryReader reader(bytes);
        Grid result;
        result.width = reader.u32();
        result.depth = reader.u32();
        result.origin_x = std::bit_cast<std::int32_t>(reader.u32());
        result.origin_z = std::bit_cast<std::int32_t>(reader.u32());
        result.right = std::bit_cast<std::int32_t>(reader.u32());
        result.bottom = std::bit_cast<std::int32_t>(reader.u32());
        result.cell_width = std::bit_cast<std::int32_t>(reader.u32());
        result.cell_depth = std::bit_cast<std::int32_t>(reader.u32());
        result.stride = reader.u32();
        if (!result.width || !result.depth || result.width > 4096 || result.depth > 4096 ||
            result.cell_width <= 0 || result.cell_depth <= 0 || result.stride != expected_stride ||
            static_cast<std::uint64_t>(result.width) * result.depth > reader.remaining() / 4)
            return Result::invalid_argument;
        result.cells.reserve(static_cast<std::size_t>(result.width) * result.depth);
        for (std::uint64_t cell = 0; cell < static_cast<std::uint64_t>(result.width) * result.depth;
             ++cell) {
            const auto count = reader.u32();
            if (count > reader.remaining() / result.stride)
                return Result::invalid_argument;
            const auto data = reader.take(static_cast<std::size_t>(count) * result.stride);
            result.cells.emplace_back(data.begin(), data.end());
        }
        if (reader.remaining())
            return Result::invalid_argument;
        *this = std::move(result);
        return Result::ok;
    } catch (const std::exception&) {
        return Result::invalid_argument;
    }
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004f23c0
std::span<const std::byte> TrackCollision::Grid::cell(Vec3 point) const {
    if (!std::isfinite(point.x) || !std::isfinite(point.z) || !width || !depth)
        return {};
    const double x =
        std::fabs(static_cast<double>(point.x - static_cast<float>(origin_x))) / cell_width;
    const double z =
        std::fabs(static_cast<double>(point.z - static_cast<float>(origin_z))) / cell_depth;
    if (x >= width || z >= depth)
        return {};
    return cells[static_cast<std::size_t>(z) * width + static_cast<std::size_t>(x)];
}
Result TrackCollision::load(ResourceStore& resources, std::string_view track,
                            const MeshObject& scene, std::string& error) {
    const std::string base = "Tracks/" + std::string(track) + "/World/";
    std::vector<std::byte> data;
    auto load_grid = [&](const char* name, Grid& grid, unsigned stride) {
        if (resources.read(base + name, data) != Result::ok) {
            error = "Missing " + base + name;
            return Result::not_found;
        }
        const auto result = grid.decode(data, stride);
        if (result != Result::ok)
            error = "Invalid " + base + name;
        return result;
    };
    auto result = load_grid("SurfaceGrid.bin", surfaces_, 48);
    if (result != Result::ok)
        return result;
    result = load_grid("CollisionGrid.bin", walls_, 40);
    if (result != Result::ok)
        return result;
    if (resources.read(base + "FrictionGrid.bin", data) != Result::ok) {
        error = "Missing friction grid";
        return Result::not_found;
    }
    try {
        BinaryReader reader(data);
        const auto count = reader.u32();
        if (!count || count > reader.remaining() / 552) {
            error = "Invalid friction material count";
            return Result::invalid_argument;
        }
        std::vector<Friction> materials;
        for (unsigned index = 0; index < count; ++index) {
            BinaryReader material(reader.take(552));
            const auto kind = material.u32();
            Friction record;
            record.kind = kind;
            record.longitudinal = material.f32();
            record.lateral = material.f32();
            record.rolling = material.f32();
            record.particle_dust = material.u32();
            record.skid_color_argb = material.u32();
            record.particle_color_argb = material.u32();
            material.take(260);
            const auto skid_sound = material.text(260);
            if (skid_sound != "none")
                record.skid_sound_path = "Sfx/" + skid_sound;
            materials.push_back(record);
        }
        result = friction_grid_.decode(reader.take(reader.remaining()), 40);
        if (result != Result::ok) {
            error = "Invalid friction spatial grid";
            return result;
        }
        friction_ = std::move(materials);
    } catch (const std::exception& exception) {
        error = exception.what();
        return Result::invalid_argument;
    }
    Vec3 position;
    const auto* start = find_start(scene, {}, position);
    if (!start || start->type != 4) {
        error = "Original Start1 type-4 dummy not found";
        return Result::not_found;
    }
    const auto ground = sample(position);
    if (!ground.hit) {
        error = "Original Start1 has no surface triangle";
        return Result::invalid_argument;
    }
    // 004bb389: start dummy is projected onto original surface grid, then Y -= 0.5.
    spawn_ = {{position.x, ground.point.y - 0.5f, position.z}, start->dummy_heading_degrees};
    error.clear();
    return Result::ok;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004d59cf
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004fb769
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004fb7fe
GroundContact TrackCollision::sample(Vec3 point) const {
    GroundContact result;
    result.point.x = point.x;
    result.point.z = point.z;
    BinaryReader surfaces(surfaces_.cell(point));
    while (surfaces.remaining()) {
        const std::array<Vec3, 3> triangle{read_vector(surfaces), read_vector(surfaces),
                                           read_vector(surfaces)};
        const auto normal = read_vector(surfaces);
        if (triangle_contains(point.x, point.z, triangle)) {
            if (normal.y == 0)
                return result;
            const double numerator =
                (-static_cast<double>(normal.x) * point.x -
                 static_cast<double>(normal.z) * point.z) +
                static_cast<double>(normal.z * triangle[0].z + normal.y * triangle[0].y +
                                    normal.x * triangle[0].x);
            result.point.y = static_cast<float>(numerator / normal.y);
            result.normal = normal;
            result.hit = true;
            break;
        }
    }
    if (!friction_.empty()) {
        std::size_t material{};
        BinaryReader friction(friction_grid_.cell(point));
        while (friction.remaining()) {
            const std::array<Vec3, 3> triangle{read_vector(friction), read_vector(friction),
                                               read_vector(friction)};
            const auto index = friction.u32();
            if (triangle_contains(point.x, point.z, triangle)) {
                if (index >= 1 && index <= friction_.size())
                    material = index - 1;
                break;
            }
        }
        result.longitudinal_grip = friction_[material].longitudinal;
        result.material_kind = friction_[material].kind;
        result.lateral_grip = friction_[material].lateral;
        result.rolling_resistance = friction_[material].rolling;
        result.particle_dust = friction_[material].particle_dust;
        result.particle_color_argb = friction_[material].particle_color_argb;
        result.skid_color_argb = friction_[material].skid_color_argb;
        result.skid_sound_path = friction_[material].skid_sound_path;
    }
    return result;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004bdecb
std::vector<WallRecord> TrackCollision::nearby_walls(Vec3 current, Vec3 previous, float radius,
                                                     bool check_height) const {
    std::vector<WallRecord> result;
    BinaryReader reader(walls_.cell(current));
    while (reader.remaining()) {
        WallRecord wall{read_vector(reader), read_vector(reader), read_vector(reader),
                        reader.f32()};
        const auto twice_radius = radius + radius;
        float limit = distance_xz(wall.lower, wall.upper) * 0.5f + twice_radius;
        if (limit < twice_radius)
            limit = radius;
        if (check_height && !(wall.lower.y < current.y && current.y < wall.upper.y))
            continue;
        const Vec3 center{(wall.lower.x + wall.upper.x) * 0.5f, 0,
                          (wall.lower.z + wall.upper.z) * 0.5f};
        if (distance_xz(current, center) < limit || distance_xz(previous, center) < limit)
            result.push_back(wall);
    }
    return result;
}
std::size_t TrackCollision::surface_record_count() const {
    std::size_t count{};
    for (const auto& cell : surfaces_.cells)
        count += cell.size() / 48;
    return count;
}
} // namespace roadwars
