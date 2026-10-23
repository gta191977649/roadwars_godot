#include "game_src/unresolved/vehicle_damage_visuals.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
namespace roadwars {
Result select_vehicle_body_lod(Vec3 object, Vec3 camera, int detail, std::size_t levels,
                               std::size_t& selected) {
    if (detail < 0 || detail > 2 || levels < 1 || levels > 5 ||
        !std::isfinite(object.x) || !std::isfinite(object.z) ||
        !std::isfinite(camera.x) || !std::isfinite(camera.z))
        return Result::invalid_argument;
    const auto x = object.x - camera.x, z = object.z - camera.z;
    const auto distance_squared = z * z + x * x;
    selected = 0;
    for (std::size_t i = 1; i < levels; ++i) {
        const auto threshold = std::max(0.f, static_cast<float>(static_cast<int>(i) - (2 - detail)) * 40.f);
        if (threshold * threshold <= distance_squared)
            selected = i;
    }
    return Result::ok;
}
namespace {
bool finite(Vec3 p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }
float length(Vec3 p) { return std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z); }
Vec3 unit(Vec3 p) {
    const auto n = length(p);
    return n == 0 ? Vec3{} : Vec3{p.x / n, p.y / n, p.z / n};
}
Vec3 rotate_y(Vec3 p, float radians) {
    const auto s = std::sin(radians), c = std::cos(radians);
    return {p.x * c + p.z * s, p.y, p.z * c - p.x * s};
}
const MeshObject* body(const MeshObject& node) {
    if (node.name == "1024Body" || node.name == "Body")
        return &node;
    for (const auto& child : node.children)
        if (const auto* found = body(child))
            return found;
    return nullptr;
}
std::int32_t add_wrap(std::int32_t a, std::int32_t b) {
    return std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(a) +
                                       std::bit_cast<std::uint32_t>(b));
}
std::int32_t pixel(float coordinate) {
    const double value = static_cast<double>(coordinate) * 255;
    //005167d5 uses FISTP QWORD; its integer-indefinite low EAX is zero.
    // Valid64-bit results are then truncated to low32 by the caller.
    if (!std::isfinite(value) || value < -9223372036854775808.0 || value >= 9223372036854775808.0)
        return 0;
    return std::bit_cast<std::int32_t>(
        static_cast<std::uint32_t>(static_cast<std::int64_t>(value)));
}
bool near(Vec3 p, Vec3 center) {
    constexpr float radius = .32499998807907104f;
    return center.x - radius < p.x && p.x < center.x + radius && center.z - radius < p.z &&
           p.z < center.z + radius;
}
} // namespace
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00420935
// Body LOD loading subset0040f06f, native renderer allocations remain external.
Result VehicleDamageVisuals::load(ResourceStore& resources, std::string_view vehicle, int paint_job,
                                  std::string& error) {
    if (vehicle.empty() || vehicle.find_first_of("/\\:\r\n") != std::string_view::npos ||
        paint_job < 0 || paint_job > 6)
        return Result::invalid_argument;
    const auto base = "Cars/" + std::string(vehicle);
    Configuration config;
    if (config.load(resources, base + "/Data/Data.ini") != Result::ok) {
        error = "Vehicle damage configuration unavailable";
        return Result::unavailable;
    }
    const auto texture = config.value("DAMAGE", "BODYTEXTURE");
    const auto dot = texture.find_last_of('.');
    if (texture.empty() || dot == std::string::npos) {
        error = "Vehicle BODYTEXTURE unavailable";
        return Result::unavailable;
    }
    std::array<MeshObject, 4> lods;
    std::vector<std::byte> bytes;
    for (std::size_t i = 0; i < 4; ++i) {
        const auto path = base + "/Body/SpatialObject" + (i ? std::to_string(i + 1) : "") + ".msh";
        MeshObject scene;
        if (resources.read(path, bytes) != Result::ok ||
            decode_msh(bytes, scene, error) != Result::ok)
            return Result::unavailable;
        const auto* selected = i ? &scene : body(scene);
        if (!selected) {
            error = "Original body LOD unavailable";
            return Result::unavailable;
        }
        lods[i] = *selected;
    }
    const auto directory =
        paint_job ? base + "/Paint/Paint Job " + std::to_string(paint_job) + "/" : base + "/Body/";
    Bitmap paint, damage;
    TextureTransparency transparency{}, damage_transparency{};
    const auto damage_name = texture.substr(0, dot) + "Damage.bmp";
    if (resources.read(directory + texture, bytes) != Result::ok ||
        decode_bitmap(bytes, paint) != Result::ok ||
        prepare_original_texture(directory + texture, paint, transparency) != Result::ok ||
        resources.read(directory + damage_name, bytes) != Result::ok ||
        decode_bitmap(bytes, damage) != Result::ok ||
        prepare_original_texture(directory + damage_name, damage, damage_transparency) !=
            Result::ok) {
        error = "Original paint/damage bitmap unavailable";
        return Result::unavailable;
    }
    std::optional<MeshObject> terminal;
    const auto terminal_result = resources.read(base + "/Body/SpatialObject5.msh", bytes);
    if (terminal_result == Result::ok) {
        MeshObject mesh;
        if (decode_msh(bytes, mesh, error) != Result::ok)
            return Result::unavailable;
        terminal = std::move(mesh);
    } else if (terminal_result != Result::not_found)
        return terminal_result;
    const auto result = configure(std::move(lods), std::move(paint), std::move(damage),
                                  directory + texture, transparency);
    if (result == Result::ok)
        terminal_body_ = std::move(terminal);
    if (result != Result::ok)
        error = "Unsupported original body damage resources";
    else
        error.clear();
    return result;
}
Result VehicleDamageVisuals::configure(std::array<MeshObject, 4> lods, Bitmap paint, Bitmap damage,
                                       std::string texture, TextureTransparency transparency) {
    if (paint.width != 256 || paint.height != 256 || damage.width != 256 || damage.height != 256 ||
        paint.rgba.size() != 256 * 256 * 4 || damage.rgba.size() != 256 * 256 * 4)
        return Result::unavailable;
    for (const auto& mesh : lods)
        for (const auto& part : mesh.parts) {
            if (part.positions.size() != part.uv.size() ||
                (!part.native_uv.empty() && part.native_uv.size() != part.positions.size()))
                return Result::invalid_argument;
            for (auto p : part.positions)
                if (!finite(p))
                    return Result::invalid_argument;
        }
    body_ = std::move(lods);
    terminal_body_.reset();
    paint_ = std::make_shared<PaintState>(PaintState{std::move(paint), 0, transparency});
    damage_ = std::move(damage);
    texture_path_ = std::move(texture);
    marks_.clear();
    deformations_.clear();
    revision_ = 0;
    configured_ = true;
    return Result::ok;
}
Result VehicleDamageVisuals::share_paint_with(VehicleDamageVisuals& other) {
    if (!configured_ || !other.configured_)
        return Result::unavailable;
    if (ResourceStore::canonical(texture_path_) != ResourceStore::canonical(other.texture_path_))
        return Result::invalid_argument;
    paint_ = other.paint_;
    return Result::ok;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004224c0
void VehicleDamageVisuals::mark(std::optional<std::uint32_t> side, Vec2 uv,
                                ParticleRandom& random) {
    if (marks_.size() >= 100)
        return;
    VehicleTextureMark mark;
    mark.side = side;
    const auto u = pixel(uv.x), v = pixel(uv.y);
    for (auto& rect : mark.rectangles) {
        const auto x = add_wrap(u, static_cast<std::int32_t>(random.next() % 11) - 5);
        const auto y = add_wrap(v, static_cast<std::int32_t>(random.next() % 11) - 5);
        rect = {std::max(add_wrap(x, -4), 0), std::max(add_wrap(y, -4), 0),
                std::min(add_wrap(x, 4), 255), std::min(add_wrap(y, 4), 255)};
        // Native same-rectangle DirectDraw Blt(flags0) replaces pixels, without
        // source color-key/alpha blending. Invalid rectangles fail unchanged.
        if (rect.left < 0 || rect.top < 0 || rect.right > 256 || rect.bottom > 256 ||
            rect.left >= rect.right || rect.top >= rect.bottom)
            continue;
        for (auto yrow = rect.top; yrow < rect.bottom; ++yrow)
            for (auto xcol = rect.left; xcol < rect.right; ++xcol) {
                const auto offset = (static_cast<std::size_t>(yrow) * 256 + xcol) * 4;
                std::copy_n(damage_.rgba.begin() + offset, 4, paint_->bitmap.rgba.begin() + offset);
            }
    }
    marks_.push_back(mark);
    ++paint_->revision;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0042299b
void VehicleDamageVisuals::deform(Vec3 point, Vec3 displacement) {
    if (deformations_.size() >= 4)
        return;
    for (auto& mesh : body_)
        for (auto& part : mesh.parts)
            for (auto& vertex : part.positions)
                if (near(vertex, point)) {
                    vertex.x += displacement.x;
                    vertex.y += displacement.y;
                    vertex.z += displacement.z;
                }
    // Original copies changed vertices to buffers; it does not recompute normals.
    deformations_.push_back({point, displacement});
    ++revision_;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004215fd
// Visual branch only: caller must retain preceding004211a0 health dispatch.
Result VehicleDamageVisuals::collision(const VehicleDamageVisualContact& contact,
                                       ParticleRandom& random) {
    if (!configured_)
        return Result::unavailable;
    if (!finite(contact.force) || !finite(contact.world_point) || !finite(contact.vehicle_origin) ||
        !finite(contact.front_direction) || !std::isfinite(contact.heading_degrees) ||
        !std::isfinite(contact.mass_kg) || contact.mass_kg <= 0 || !std::isfinite(contact.width) ||
        contact.width <= 0 || !std::isfinite(contact.length) || contact.length <= 0)
        return Result::invalid_argument;
    if (contact.invulnerable || contact.disabled)
        return Result::ok;
    float severity = (1 / contact.mass_kg) * .015978554263710976f * length(contact.force);
    if (severity < .029999999329447746f)
        return Result::ok;
    Vec3 local = {contact.world_point.x - contact.vehicle_origin.x,
                  contact.world_point.y - contact.vehicle_origin.y,
                  contact.world_point.z - contact.vehicle_origin.z};
    if (contact.receiver_front_guard) {
        const auto forward = unit(contact.front_direction), direction = unit(local);
        const auto angle = std::acos(std::clamp(forward.x * direction.x + forward.y * direction.y +
                                                    forward.z * direction.z,
                                                -1.f, 1.f)) *
                           57.288352966308594f;
        if (angle < 60)
            return Result::ok;
    }
    const auto distance = length(local);
    constexpr float radians = .017455555498600006f;
    local = rotate_y(unit(local), -contact.heading_degrees * radians);
    local = {local.x * distance, local.y * distance, local.z * distance};
    const auto force = rotate_y(unit(contact.force), -contact.heading_degrees * radians);
    severity = std::min(severity, .25f);
    std::optional<std::uint32_t> side;
    for (const auto& part : body_[0].parts)
        for (std::size_t i = 0; i < part.positions.size(); ++i) {
            const auto p = part.positions[i];
            if (!near(p, local))
                continue;
            if (-contact.width < p.x && p.x < contact.width && contact.length * .33f < p.z &&
                p.z < contact.length)
                side = 0;
            else if (-contact.width < p.x && p.x < contact.width && -contact.length < p.z &&
                     p.z < -contact.length * .33f)
                side = 1;
            else if (0 < p.x && p.x < contact.width && -contact.length * .33f <= p.z &&
                     p.z <= contact.length * .33f)
                side = 3;
            else if (-contact.width < p.x && p.x <= 0 && -contact.length * .33f <= p.z &&
                     p.z <= contact.length * .33f)
                side = 2;
            mark(side, part.native_uv.empty() ? part.uv[i] : part.native_uv[i], random);
        }
    deform(local, {force.x * severity, severity / 5, force.z * severity});
    return Result::ok;
}
} // namespace roadwars
