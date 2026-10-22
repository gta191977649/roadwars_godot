#include "game_src/unresolved/race_route.hpp"
#include "game_src/unresolved/binary_reader.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace roadwars {
namespace {
Vec3 add(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 subtract(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 scale(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
float magnitude(Vec3 a) { return std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z); }
Vec3 normalize(Vec3 a) {
    const auto value = magnitude(a);
    return value == 0 ? a : scale(a, 1.0f / value);
}
bool finite(Vec3 p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }
Vec3 vector(BinaryReader& input) { return {input.f32(), input.f32(), input.f32()}; }
RaceRoutePoint read_point(BinaryReader& input) {
    RaceRoutePoint result;
    result.id = static_cast<int>(input.u32());
    result.left = vector(input);
    result.right = vector(input);
    result.center = vector(input);
    result.half_width = input.f32();
    result.lateral = vector(input);
    result.optimal_offset = input.f32();
    result.alternate_offset = input.f32();
    result.right_bound = input.f32();
    result.left_bound = input.f32();
    result.speed_hint = input.f32();
    for (auto& flag : result.flags)
        flag = static_cast<int>(input.u32());
    result.cumulative_distance = input.f32();
    result.branch_kind = static_cast<int>(input.u32());
    input.u32(); // Serialized 32-bit runtime pointer; never convert to a host address.
    if (result.half_width <= 0 || result.cumulative_distance < 0)
        throw std::runtime_error("Invalid route width or cumulative distance");
    // 004182ee normalizes orientation lazily. A value copy has the same point result
    // without making diagnostic reads mutate the original route data.
    if (result.right_bound < result.left_bound) {
        result.lateral.x = -result.lateral.x;
        result.lateral.z = -result.lateral.z;
        result.optimal_offset = -result.optimal_offset;
        result.alternate_offset = -result.alternate_offset;
        result.right_bound = -result.right_bound;
        result.left_bound = -result.left_bound;
    }
    return result;
}
// Original 004dd310 XZ intersection. The optional parameter is its negative
// fraction on the first segment; preserve its endpoint/collinear cases.
bool intersect(Vec3 a, Vec3 b, Vec3 c, Vec3 d, Vec3& output, float* fraction = nullptr) {
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
            output.x = a.x;
            output.z = (a.z + b.z + c.z + d.z) / 4.0f;
            return true;
        }
    }
    if (v == 0 && z == 0) {
        if (a.z != c.z)
            return false;
        if (between(a.x, c.x, d.x) || between(b.x, c.x, d.x)) {
            output.x = (a.x + b.x + c.x + d.x) / 4.0f;
            output.z = a.z;
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
    output.x = x * t + a.x;
    output.z = z * t + a.z;
    if (fraction)
        *fraction = t;
    return true;
}
bool triangle_contains(Vec3 position, Vec3 a, Vec3 b, Vec3 c) {
    const std::array<Vec3, 3> vertices{a, b, c};
    unsigned result{}, previous = 2;
    for (unsigned index = 0; index < 3; ++index) {
        const auto& current = vertices[index];
        const auto& last = vertices[previous];
        if (((current.z <= position.z && position.z < last.z) &&
             (position.x - current.x) * (last.z - current.z) <
                 (position.z - current.z) * (last.x - current.x)) ||
            ((last.z <= position.z && position.z < current.z) &&
             (position.z - current.z) * (last.x - current.x) <
                 (position.x - current.x) * (last.z - current.z)))
            result ^= 1;
        previous = index;
    }
    return result != 0;
}
} // namespace

Result RaceRoute::load(ResourceStore& resources, std::string_view track, std::string& error,
                       bool reverse) {
    std::vector<std::byte> bytes;
    const auto path =
        "Tracks/" + std::string(track) + "/World/" + (reverse ? "LapRev.bin" : "Lap.bin");
    const auto result = resources.read(path, bytes);
    if (result != Result::ok) {
        error = "Cannot read original race route: " + path;
        return result;
    }
    return decode(bytes, error);
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004c5cc7
Result RaceRoute::decode(std::span<const std::byte> bytes, std::string& error) {
    nodes_.clear();
    length_ = 0;
    checkpoints_ = {};
    try {
        BinaryReader input(bytes);
        const auto version = input.u32(), count = input.u32();
        if (version < 1 || version > 4 || count < 6 || count > input.remaining() / 104)
            throw std::runtime_error("Invalid original route version or count");
        std::vector<RaceRouteNode> nodes;
        nodes.reserve(count);
        for (std::uint32_t index = 0; index < count; ++index) {
            if (version != 4)
                input.take(32);
            RaceRouteNode node;
            node.main = read_point(input);
            if (node.main.id != static_cast<int>(index + 1))
                throw std::runtime_error("Nonsequential original route ID");
            const auto branch = input.u32();
            if (branch) {
                if (version != 4)
                    input.take(32);
                node.branch = read_point(input);
            }
            nodes.push_back(std::move(node));
        }
        if (input.remaining())
            throw std::runtime_error("Trailing original route bytes");
        const auto length =
            nodes.back().main.cumulative_distance +
            magnitude(subtract(nodes.back().main.center, nodes.front().main.center));
        if (!std::isfinite(length) || length <= 0)
            throw std::runtime_error("Invalid original route length");
        nodes_ = std::move(nodes);
        length_ = length;
        for (int index = 0; index < 5; ++index)
            checkpoints_[index] = ((static_cast<int>(count) - 5) * index) / 5;
        error.clear();
        return Result::ok;
    } catch (const std::exception& exception) {
        error = exception.what();
        return Result::invalid_argument;
    }
}
int RaceRoute::wrap(int index) const {
    const auto count = static_cast<int>(nodes_.size());
    if (count == 0)
        throw std::runtime_error("Original route is not loaded");
    index %= count;
    return index < 0 ? index + count : index;
}
const RaceRoutePoint& RaceRoute::point(int index, bool branch) const {
    const auto& node = nodes_[wrap(index)];
    return branch && node.branch ? *node.branch : node.main;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0041817d
bool RaceRoute::alternate_path(int base, bool on_branch, int recent, int behavior) const {
    int kind{};
    for (int index = 0; index <= 5; ++index) {
        const auto& branch = nodes_[wrap(base + index)].branch;
        if (branch && (branch->branch_kind & 2) != 0) {
            kind = branch->branch_kind;
            break;
        }
    }
    if (on_branch)
        return true;
    if (kind == 0)
        return recent != -1;
    const bool aggressive = behavior == 2 || behavior == 3;
    if (kind == 3)
        return aggressive;
    if (aggressive && kind == 2)
        return false;
    return true;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004182ee
Vec3 RaceRoute::sample(int base, int relative, bool on_branch, int recent, int behavior,
                       float& lateral_offset, bool& branch_latch) const {
    const auto& node = nodes_[wrap(base + relative)];
    const int preferred_branch = behavior == 2 || behavior == 3 ? 3 : 2;
    const RaceRoutePoint* selected = &node.main;
    if (node.branch) {
        if (on_branch || branch_latch)
            selected = &*node.branch;
        else if (node.branch->branch_kind == preferred_branch) {
            selected = &*node.branch;
            branch_latch = true;
        }
    }
    const auto offset = alternate_path(base, on_branch, recent, behavior)
                            ? selected->alternate_offset
                            : selected->optimal_offset;
    float clamped = offset + lateral_offset;
    if (selected->left_bound <= clamped) {
        if (selected->right_bound < clamped) {
            clamped = selected->right_bound;
            lateral_offset = clamped - offset;
        }
    } else {
        clamped = selected->left_bound;
        lateral_offset = clamped - offset;
    }
    return add(selected->center, scale(selected->lateral, clamped));
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004185fe
Vec3 RaceRoute::respawn(int index, float fraction, bool branch) const {
    const auto& first = point(index, branch);
    const auto& second = point(index + 1, branch);
    const auto a = add(first.center, scale(first.lateral, first.optimal_offset));
    const auto b = add(second.center, scale(second.lateral, second.optimal_offset));
    // The original keeps the first point's Y; ground projection is a separate call.
    return {a.x + (b.x - a.x) * fraction, a.y, a.z + (b.z - a.z) * fraction};
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004be8f9
int RaceRoute::initial_node(Vec3 position) const {
    const int count = static_cast<int>(nodes_.size());
    for (int index = std::max(count - 30, 1); index < count; ++index) {
        const auto& a = point(index);
        const auto& b = point(index + 1);
        if (triangle_contains(position, b.left, b.right, a.right) ||
            triangle_contains(position, a.left, a.right, b.left) ||
            triangle_contains(position, b.right, b.left, a.right) ||
            triangle_contains(position, a.left, a.right, b.right))
            return index;
    }
    return -1;
}
Result RaceRoute::initialize_progress(RaceRouteProgress& progress, Vec3 position,
                                      std::string& error) const {
    progress = {};
    progress.branch_recent = -1;
    progress.current_node = initial_node(position);
    if (progress.current_node < 0) {
        error = "Cannot find original initial lap line index";
        return Result::invalid_argument;
    }
    error.clear();
    return Result::ok;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004bd136
Result RaceRoute::update(RaceRouteProgress& progress, Vec3 previous, Vec3 current, int total_laps,
                         std::string& error) const {
    if (nodes_.empty() || !finite(previous) || !finite(current) || total_laps < 1 ||
        total_laps > 10 || progress.current_node < 0 ||
        progress.current_node >= static_cast<int>(nodes_.size())) {
        error = "Invalid original route progression input";
        return Result::invalid_argument;
    }
    const auto end = add(current, normalize(subtract(current, previous)));
    const bool previous_gates = progress.checkpoints[1] && progress.checkpoints[2] &&
                                progress.checkpoints[3] && progress.checkpoints[4];
    int next = wrap(progress.current_node + 1);
    Vec3 intersection;
    const auto& next_node = nodes_[next];
    const bool main_forward =
        intersect(next_node.main.left, next_node.main.right, previous, end, intersection);
    const bool branch_forward =
        next_node.branch &&
        intersect(next_node.branch->left, next_node.branch->right, previous, end, intersection);
    if (main_forward || branch_forward) {
        progress.current_node = next_node.main.id - 1;
        if (progress.on_branch && branch_forward)
            progress.branch_recent = 1;
        else if (!branch_forward && progress.branch_recent != -1) {
            if (++progress.branch_recent > 3)
                progress.branch_recent = -1;
        }
        progress.on_branch = branch_forward;
        for (int gate = 0; gate < 5; ++gate) {
            if (progress.current_node != checkpoints_[gate])
                continue;
            if (progress.current_node == 0 && previous_gates) {
                ++progress.completed_laps;
                progress.checkpoints = {};
                progress.checkpoints[0] = true;
            } else if (gate == 0 || progress.checkpoints[gate - 1])
                progress.checkpoints[gate] = true;
        }
    } else {
        next = wrap(progress.current_node - 1);
        const auto& node = nodes_[next];
        const bool main_back =
            intersect(previous, end, node.main.left, node.main.right, intersection);
        const bool branch_back = node.branch && intersect(previous, end, node.branch->left,
                                                          node.branch->right, intersection);
        if (main_back || branch_back) {
            for (int gate = 0; gate < 5; ++gate)
                if (progress.current_node < checkpoints_[gate])
                    progress.checkpoints[gate] = false;
            progress.current_node = node.main.id - 1;
            if (progress.current_node == static_cast<int>(nodes_.size()) - 1 &&
                std::find(progress.checkpoints.begin(), progress.checkpoints.end(), false) !=
                    progress.checkpoints.end())
                progress.checkpoints = {};
            progress.on_branch = branch_back;
        }
    }
    update_percentage(progress, current);
    progress.finished = progress.completed_laps >= total_laps;
    error.clear();
    return Result::ok;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004bf081
void RaceRoute::update_percentage(RaceRouteProgress& progress, Vec3 current) const {
    const auto& a = point(progress.current_node, progress.on_branch);
    const auto& b = point(progress.current_node + 1, progress.on_branch);
    const auto& c = point(progress.current_node - 1, progress.on_branch);
    const bool in_next = triangle_contains(current, a.left, a.right, b.left) ||
                         triangle_contains(current, b.left, b.right, a.right) ||
                         triangle_contains(current, a.left, a.right, b.right) ||
                         triangle_contains(current, b.left, b.right, a.left);
    const auto& origin = in_next ? a : c;
    const auto& target = in_next ? b : a;
    // 004bf081 projects with the next crossing's direction for the forward
    // quad, and with the previous crossing's direction for the backward quad.
    const auto& crossing = in_next ? b : c;
    auto lateral = subtract(crossing.right, crossing.left);
    lateral.y = 0;
    lateral = scale(normalize(lateral), 100);
    Vec3 intersection;
    if (!intersect(origin.center, target.center, subtract(current, lateral), add(current, lateral),
                   intersection))
        return;
    const auto cumulative =
        point(in_next ? progress.current_node : progress.current_node - 1).cumulative_distance;
    progress.previous_route_percent = progress.route_percent;
    progress.route_percent =
        ((cumulative + magnitude(subtract(intersection, origin.center))) / length_) * 100.0f;
    const auto difference = progress.route_percent - progress.previous_route_percent;
    if (difference < -10)
        progress.previous_route_percent = progress.route_percent - .01f;
    else if (difference > 10)
        progress.previous_route_percent = progress.route_percent + .01f;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00413b93
float RaceRoute::ranking_progress(const RaceRouteProgress& progress) const {
    if (!progress.checkpoints[0] || (progress.current_node == 0 && !progress.checkpoints[4]))
        return 0;
    return progress.route_percent;
}
} // namespace roadwars
