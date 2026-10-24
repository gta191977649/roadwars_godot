#include "game_src/unresolved/vehicle_damage_visuals.hpp"
#include <algorithm>
#include <bit>
#include <charconv>
#include <cmath>
#include <limits>
namespace roadwars {
Result select_vehicle_tire_lod(Vec3 object, Vec3 camera, int detail, std::size_t levels,
                               std::size_t& selected) {
    if (select_vehicle_body_lod(object, camera, detail, levels, selected) != Result::ok)
        return Result::invalid_argument;
    const std::array<float, 5> thresholds = detail == 2
        ? std::array<float, 5>{0, 15, 20, 30, 40}
        : std::array<float, 5>{0, 0, 15, 20, 30};
    const auto x = object.x - camera.x, z = object.z - camera.z;
    const auto distance_squared = z * z + x * x;
    selected = 0;
    for (std::size_t i = 1; i < levels; ++i)
        if (thresholds[i] * thresholds[i] <= distance_squared)
            selected = i;
    return Result::ok;
}
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
bool valid_atlas(VehicleDamageRect rect) {
    return rect.left >= 0 && rect.top >= 0 && rect.right <= 255 && rect.bottom <= 255 &&
           rect.left <= rect.right && rect.top <= rect.bottom;
}
bool parse_damage_rect(std::string_view text, VehicleDamageRect& rect) {
    std::array<std::int32_t, 4> values;
    for (std::size_t i = 0; i < values.size(); ++i) {
        while (!text.empty() && (text.front() == ' ' || text.front() == '\t'))
            text.remove_prefix(1);
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), values[i]);
        if (parsed.ec != std::errc{})
            return false;
        text.remove_prefix(static_cast<std::size_t>(parsed.ptr - text.data()));
        while (!text.empty() && (text.front() == ' ' || text.front() == '\t'))
            text.remove_prefix(1);
        if (i + 1 < values.size()) {
            if (text.empty() || text.front() != ',')
                return false;
            text.remove_prefix(1);
        }
    }
    rect = {values[0], values[1], values[2], values[3]};
    return text.empty() && valid_atlas(rect);
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
    std::array<VehicleDamageRect, 4> projectile_regions;
    constexpr std::array<std::string_view, 4> region_names{
        "FRONT", "REAR", "MIDDLELEFT", "MIDDLERIGHT"};
    for (std::size_t i = 0; i < region_names.size(); ++i)
        if (!parse_damage_rect(config.value("DAMAGE", region_names[i]), projectile_regions[i])) {
            error = "Original projectile damage atlas unavailable";
            return Result::unavailable;
        }
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
    if (result == Result::ok) {
        terminal_body_ = std::move(terminal);
        configure_projectile_regions(projectile_regions);
    }
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
    for (std::size_t i = 0; i < body_.size(); ++i) {
        original_positions_[i].clear();
        for (const auto& part : body_[i].parts)
            original_positions_[i].push_back(part.positions);
    }
    terminal_body_.reset();
    original_paint_ = paint;
    paint_ = std::make_shared<PaintState>(PaintState{std::move(paint), 0, transparency});
    damage_ = std::move(damage);
    texture_path_ = std::move(texture);
    marks_.clear();
    deformations_.clear();
    pixel_pairs_.clear();
    projectile_regions_configured_ = false;
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
VehicleDamageVisualState VehicleDamageVisuals::capture_state() const {
    if (!configured_)
        return {};
    return {texture_path_, paint_->bitmap, marks_, deformations_, pixel_pairs_};
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004228c5
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00422930
Result VehicleDamageVisuals::replay_texture_mark(const VehicleTextureMark& mark, bool damaged) {
    if (!configured_) return Result::unavailable;
    const auto& source = damaged ? damage_ : original_paint_;
    for (const auto& rect : mark.rectangles) {
        // Invalid native Blt rectangles fail unchanged, just as the live mark path.
        if (rect.left < 0 || rect.top < 0 || rect.right > 256 || rect.bottom > 256 ||
            rect.left >= rect.right || rect.top >= rect.bottom) continue;
        for (int y = rect.top; y < rect.bottom; ++y)
            for (int x = rect.left; x < rect.right; ++x) {
                const auto offset = (static_cast<std::size_t>(y) * 256 + x) * 4;
                std::copy_n(source.rgba.begin() + offset, 4, paint_->bitmap.rgba.begin() + offset);
            }
    }
    ++paint_->revision;
    return Result::ok;
}
Result VehicleDamageVisuals::reapply_deformations(std::span<const VehicleDeformation> records) {
    if (!configured_)
        return Result::unavailable;
    if (records.size() > 4)
        return Result::invalid_argument;
    for (const auto& record : records)
        if (!finite(record.local_point) || !finite(record.displacement))
            return Result::invalid_argument;
    // The input may borrow our own records.
    std::vector<VehicleDeformation> copy(records.begin(), records.end());
    deformations_ = std::move(copy);
    rebuild_deformations();
    return Result::ok;
}
Result VehicleDamageVisuals::restore_state(const VehicleDamageVisualState& state) {
    if (!configured_)
        return Result::unavailable;
    if (ResourceStore::canonical(state.texture_path) != ResourceStore::canonical(texture_path_) ||
        state.paint.width != 256 || state.paint.height != 256 ||
        state.paint.rgba.size() != 256 * 256 * 4 || state.marks.size() > 100 ||
        state.deformations.size() > 4 || state.pixel_pairs.size() > 100)
        return Result::invalid_argument;
    for (const auto& pair : state.pixel_pairs)
        if (pair.side > 3 || pair.x < 0 || pair.x > 254 || pair.y < 0 || pair.y > 255)
            return Result::invalid_argument;
    for (const auto& record : state.deformations)
        if (!finite(record.local_point) || !finite(record.displacement))
            return Result::invalid_argument;
    for (const auto& mark : state.marks)
        if (mark.side && *mark.side > 3)
            return Result::invalid_argument;
    paint_->bitmap = state.paint;
    ++paint_->revision;
    marks_ = state.marks;
    deformations_ = state.deformations;
    pixel_pairs_ = state.pixel_pairs;
    rebuild_deformations();
    return Result::ok;
}
// Repaint subset of 00420935: existing rectangle marks and dents are replayed.
Result VehicleDamageVisuals::repaint_damage_from(const VehicleDamageVisualState& state) {
    if (!configured_)
        return Result::unavailable;
    if (state.marks.size() > 100 || state.deformations.size() > 4 || state.pixel_pairs.size() > 100)
        return Result::invalid_argument;
    for (const auto& pair : state.pixel_pairs)
        if (pair.side > 3 || pair.x < 0 || pair.x > 254 || pair.y < 0 || pair.y > 255)
            return Result::invalid_argument;
    for (const auto& record : state.deformations)
        if (!finite(record.local_point) || !finite(record.displacement))
            return Result::invalid_argument;
    for (const auto& mark : state.marks)
        if (mark.side && *mark.side > 3)
            return Result::invalid_argument;
    // 00420eb4..00420f3c replays exact stored rectangles via 004224c0's
    // non-null rectangle argument, without regenerating marks or consuming RNG.
    paint_->bitmap = original_paint_;
    pixel_pairs_.clear();
    // 00420dd3..00420eaa replays pixel pairs before the rectangle marks,
    // recapturing prior colors from the NEW livery rather than the old snapshot.
    for (const auto& pair : state.pixel_pairs)
        apply_pixel_pair(pair.side, pair.type, pair.x, pair.y);
    marks_ = state.marks;
    for (const auto& mark : marks_)
        for (const auto& rect : mark.rectangles) {
            if (rect.left < 0 || rect.top < 0 || rect.right > 256 || rect.bottom > 256 ||
                rect.left >= rect.right || rect.top >= rect.bottom)
                continue;
            for (auto y = rect.top; y < rect.bottom; ++y)
                for (auto x = rect.left; x < rect.right; ++x) {
                    const auto offset = (static_cast<std::size_t>(y) * 256 + x) * 4;
                    std::copy_n(damage_.rgba.begin() + offset, 4,
                                paint_->bitmap.rgba.begin() + offset);
                }
        }
    ++paint_->revision;
    deformations_ = state.deformations;
    rebuild_deformations();
    return Result::ok;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00421e95
Result VehicleDamageVisuals::mark_pixel_pair(std::uint32_t side, std::int32_t type,
                                           VehicleDamageRect atlas, Vec2 offset) {
    if (!configured_)
        return Result::unavailable;
    if (side > 3 || !std::isfinite(offset.x) || !std::isfinite(offset.y) ||
        offset.x < -.5f || offset.x > .5f || offset.y < -.5f || offset.y > .5f ||
        atlas.left < 0 || atlas.top < 0 || atlas.right > 255 || atlas.bottom > 255 ||
        atlas.left > atlas.right || atlas.top > atlas.bottom)
        return Result::invalid_argument;
    const auto u = offset.x + .5f, v = offset.y + .5f;
    auto x = static_cast<std::int32_t>(atlas.left + static_cast<double>(atlas.right - atlas.left) * u);
    auto y = static_cast<std::int32_t>(255 - (atlas.top + static_cast<double>(atlas.bottom - atlas.top) * v));
    if (x > 253)
        x -= 2;
    if (y > 254)
        y = 1;
    apply_pixel_pair(side, type, x, y);
    return Result::ok;
}
Result VehicleDamageVisuals::configure_projectile_regions(std::array<VehicleDamageRect, 4> regions) {
    if (!configured_)
        return Result::unavailable;
    if (!std::all_of(regions.begin(), regions.end(), valid_atlas))
        return Result::invalid_argument;
    projectile_regions_ = regions;
    projectile_regions_configured_ = true;
    return Result::ok;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00420ff4
Result VehicleDamageVisuals::projectile_damage(std::size_t contact_side, std::int32_t type,
                                             ParticleRandom& random, bool finished, bool dying) {
    if (!configured_)
        return Result::unavailable;
    if (contact_side >= 4)
        return Result::invalid_argument;
    if (finished || dying)
        return Result::ok;
    if (!projectile_regions_configured_)
        return Result::unavailable;
    // 00420ff4 samples before the downstream 100-pair cap is checked.
    const auto u = static_cast<float>(random.next() % 100) / 100.f - .5f;
    const auto v = static_cast<float>(random.next() % 100) / 100.f - .5f;
    constexpr std::array<std::uint32_t, 4> native_sides{0, 3, 1, 2};
    const auto side = native_sides[contact_side];
    return mark_pixel_pair(side, type, projectile_regions_[side], {u, v});
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00421f1d
void VehicleDamageVisuals::apply_pixel_pair(std::uint32_t side, std::int32_t type,
                                          std::int32_t x, std::int32_t y) {
    if (pixel_pairs_.size() >= 100)
        return;
    VehiclePixelPairMark pair{side, type, x, y, {}};
    if (type == 0) {
        const auto start = (static_cast<std::size_t>(y) * 256 + x) * 4;
        for (std::size_t i = 0; i < 2; ++i)
            std::copy_n(paint_->bitmap.rgba.begin() + start + i * 4, 4, pair.previous[i].begin());
        // Original native 1555 surfaces encode RGB128 as five-bit16 (decoded131).
        const std::array<std::byte, 4> gray{std::byte{131}, std::byte{131}, std::byte{131},
                                           std::byte{255}};
        const std::array<std::byte, 4> black{std::byte{0}, std::byte{0}, std::byte{0},
            paint_->transparency == TextureTransparency::color_key ? std::byte{0} : std::byte{255}};
        std::copy(gray.begin(), gray.end(), paint_->bitmap.rgba.begin() + start);
        std::copy(black.begin(), black.end(), paint_->bitmap.rgba.begin() + start + 4);
        ++paint_->revision;
    }
    pixel_pairs_.push_back(pair);
}
void VehicleDamageVisuals::rebuild_deformations() {
    for (std::size_t i = 0; i < body_.size(); ++i)
        for (std::size_t p = 0; p < body_[i].parts.size(); ++p)
            body_[i].parts[p].positions = original_positions_[i][p];
    const auto records = std::move(deformations_);
    deformations_.clear();
    for (const auto& record : records)
        deform(record.local_point, record.displacement);
    ++revision_;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00420683
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004226d4
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00422c65
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00422195
Result VehicleDamageVisuals::repair(VehicleRepairRegion region, float fraction, float width,
                                   float length) {
    if (!configured_)
        return Result::unavailable;
    if (!std::isfinite(fraction) || fraction < 0 || fraction > 1 ||
        !std::isfinite(width) || width <= 0 || !std::isfinite(length) || length <= 0)
        return Result::invalid_argument;
    float min_x{}, max_x{}, min_z{}, max_z{};
    std::optional<std::uint32_t> side;
    bool texture_repair = true;
    switch (region) {
    case VehicleRepairRegion::all:
        min_x = -width; max_x = width; min_z = -length; max_z = length;
        break;
    case VehicleRepairRegion::internal:
        texture_repair = false;
        break;
    case VehicleRepairRegion::front:
        min_x = -width; max_x = width; min_z = length * .33f; max_z = length;
        side = 0;
        break;
    case VehicleRepairRegion::right:
        max_x = width; min_z = -length * .33f; max_z = length * .33f;
        side = 3;
        break;
    case VehicleRepairRegion::rear:
        min_x = -width; max_x = width; min_z = -length; max_z = -length * .33f;
        side = 1;
        break;
    case VehicleRepairRegion::left:
        min_x = -width; min_z = -length * .33f; max_z = length * .33f;
        side = 2;
        break;
    default:
        return Result::invalid_argument;
    }
    if (texture_repair) {
        const auto pair_matches = [&](const auto& pair) { return !side || pair.side == *side; };
        auto pair_count = static_cast<std::size_t>(
            std::count_if(pixel_pairs_.begin(), pixel_pairs_.end(), pair_matches));
        if (pair_count > 1)
            pair_count = static_cast<std::size_t>(static_cast<double>(pair_count) * fraction + .5);
        for (std::size_t i = 0; i < pixel_pairs_.size() && pair_count;) {
            const auto& pair = pixel_pairs_[i];
            if (!pair_matches(pair)) {
                ++i;
                continue;
            }
            if (pair.type == 0) {
                const auto start = (static_cast<std::size_t>(pair.y) * 256 + pair.x) * 4;
                for (std::size_t p = 0; p < 2; ++p)
                    std::copy(pair.previous[p].begin(), pair.previous[p].end(),
                              paint_->bitmap.rgba.begin() + start + p * 4);
                ++paint_->revision;
            }
            pixel_pairs_.erase(pixel_pairs_.begin() + static_cast<std::ptrdiff_t>(i));
            --pair_count;
        }
        const auto matches = [&](const auto& mark) { return !side || mark.side == side; };
        auto count = static_cast<std::size_t>(std::count_if(marks_.begin(), marks_.end(), matches));
        // The original removes a singleton even for a fractional repair; only
        // larger sets use rounded proportional counts (00422738..0042274f).
        if (count > 1)
            count = static_cast<std::size_t>(static_cast<double>(count) * fraction + .5);
        for (std::size_t i = 0; i < marks_.size() && count;) {
            if (!matches(marks_[i])) {
                ++i;
                continue;
            }
            for (const auto& rect : marks_[i].rectangles) {
                if (rect.left < 0 || rect.top < 0 || rect.right > 256 || rect.bottom > 256 ||
                    rect.left >= rect.right || rect.top >= rect.bottom)
                    continue;
                for (auto y = rect.top; y < rect.bottom; ++y)
                    for (auto x = rect.left; x < rect.right; ++x) {
                        const auto offset = (static_cast<std::size_t>(y) * 256 + x) * 4;
                        std::copy_n(original_paint_.rgba.begin() + offset, 4,
                                    paint_->bitmap.rgba.begin() + offset);
                    }
            }
            marks_.erase(marks_.begin() + static_cast<std::ptrdiff_t>(i));
            --count;
            ++paint_->revision;
        }
    }
    for (std::size_t i = 0; i < deformations_.size();) {
        auto& record = deformations_[i];
        const auto p = record.local_point;
        if (p.x < min_x || p.x > max_x || p.z < min_z || p.z > max_z) {
            ++i;
            continue;
        }
        record.displacement = {record.displacement.x * (1 - fraction),
                               record.displacement.y * (1 - fraction),
                               record.displacement.z * (1 - fraction)};
        if (fraction >= 1)
            deformations_.erase(deformations_.begin() + static_cast<std::ptrdiff_t>(i));
        else
            ++i;
        // Restore original vertices and replay every remaining record after each
        // matching repair, retaining record order and the original strict radius.
        rebuild_deformations();
    }
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
    if (replay_texture_) replay_texture_(mark);
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
    if (replay_deformation_) replay_deformation_(deformations_.back());
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
