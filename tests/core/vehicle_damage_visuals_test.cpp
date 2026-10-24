#include "game_src/unresolved/vehicle_damage_visuals.hpp"
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
using namespace roadwars;
namespace {
Bitmap bitmap(std::byte value) {
    return {256, 256, std::vector<std::byte>(256 * 256 * 4, value), 24, true, {}};
}
std::array<MeshObject, 4> meshes(std::size_t count = 1) {
    std::array<MeshObject, 4> result;
    for (auto& mesh : result) {
        MeshPart part;
        part.positions.assign(count, {.1f, 0, 1});
        part.uv.assign(count, {.5f, .5f});
        part.normals.assign(count, {0, 1, 0});
        mesh.parts.push_back(std::move(part));
    }
    return result;
}
VehicleDamageVisualContact contact() {
    return {{100000, 0, 0}, {0, 0, 1}, {}, {0, 0, 1}, 0, 1000, 2, 4};
}
class Files final : public ResourceReader {
  public:
    explicit Files(std::filesystem::path path) : root_(std::move(path)) {}
    Result read(std::string_view name, std::vector<std::byte>& out) override {
        std::ifstream stream(root_ / std::filesystem::path(name), std::ios::binary | std::ios::ate);
        if (!stream)
            return Result::not_found;
        const auto n = stream.tellg();
        if (n < 0)
            return Result::invalid_argument;
        out.resize(static_cast<std::size_t>(n));
        stream.seekg(0);
        stream.read(reinterpret_cast<char*>(out.data()), n);
        return stream ? Result::ok : Result::unavailable;
    }

  private:
    std::filesystem::path root_;
};
} // namespace
int main(int argc, char** argv) {
    for (int detail = 0; detail <= 2; ++detail) {
        std::size_t selected{};
        const std::array<float, 5> tire_thresholds = detail == 2
            ? std::array<float, 5>{0, 15, 20, 30, 40}
            : std::array<float, 5>{0, 0, 15, 20, 30};
        for (std::size_t level = detail == 2 ? 1 : 2; level < 5; ++level) {
            const auto distance = tire_thresholds[level];
            assert(select_vehicle_tire_lod({distance - .01f, 200, 0}, {}, detail, 5, selected) ==
                   Result::ok && selected == level - 1);
            assert(select_vehicle_tire_lod({distance, 200, 0}, {}, detail, 5, selected) ==
                   Result::ok && selected == level);
        }
        const auto choose = [&](float distance, std::size_t expected) {
            assert(select_vehicle_body_lod({distance, 100000, 0}, {}, detail, 5, selected) ==
                   Result::ok);
            assert(selected == expected);
        };
        choose(0, static_cast<std::size_t>(2 - detail));
        for (std::size_t level = static_cast<std::size_t>(3 - detail); level < 5; ++level) {
            const auto distance = static_cast<float>(static_cast<int>(level) - (2 - detail)) * 40;
            choose(distance - .01f, level - 1);
            choose(distance, level);
            choose(-distance, level);
        }
        assert(select_vehicle_body_lod({100, 0, 0}, {100, 500, -40}, detail, 5, selected) ==
               Result::ok);
        assert(selected == static_cast<std::size_t>(3 - detail));
    }
    std::size_t selected{};
    assert(select_vehicle_body_lod({}, {}, 3, 5, selected) == Result::invalid_argument);
    assert(select_vehicle_body_lod({}, {}, 2, 0, selected) == Result::invalid_argument);
    VehicleDamageVisuals visuals;
    std::size_t draws{};
    ParticleRandom random([&] {
        ++draws;
        return 5u;
    });
    assert(visuals.collision(contact(), random) == Result::unavailable);
    auto body = meshes();
    assert(visuals.configure(body, bitmap(std::byte{1}), bitmap(std::byte{200}), "paint",
                             TextureTransparency::color_key) == Result::ok);
    auto event = contact();
    event.force = {};
    assert(visuals.collision(event, random) == Result::ok && draws == 0 && visuals.revision() == 0);
    event = contact();
    event.receiver_front_guard = true;
    assert(visuals.collision(event, random) == Result::ok && draws == 0);
    event.receiver_front_guard = false;
    assert(visuals.collision(event, random) == Result::ok && draws == 8);
    assert(visuals.marks().size() == 1 && visuals.marks()[0].side == 3);
    const auto rect = visuals.marks()[0].rectangles[0];
    assert(rect.left == 123 && rect.top == 123 && rect.right == 131 && rect.bottom == 131);
    assert(visuals.paint().rgba[(123 * 256 + 123) * 4] == std::byte{200});
    assert(visuals.paint().rgba[(131 * 256 + 131) * 4] == std::byte{1});
    VehicleDamageVisuals sibling, unrelated;
    assert(sibling.configure(body, bitmap(std::byte{1}), bitmap(std::byte{200}), "PAINT") ==
           Result::ok);
    assert(unrelated.configure(body, bitmap(std::byte{1}), bitmap(std::byte{200}), "otherpaint") ==
           Result::ok);
    assert(sibling.share_paint_with(visuals) == Result::ok);
    assert(sibling.texture_transparency() == TextureTransparency::color_key);
    assert(&sibling.paint() == &visuals.paint());
    assert(unrelated.share_paint_with(visuals) == Result::invalid_argument);
    assert(sibling.paint().rgba[(123 * 256 + 123) * 4] == std::byte{200});
    assert(unrelated.paint().rgba[(123 * 256 + 123) * 4] == std::byte{1});
    const auto shared_revision = visuals.revision();
    assert(sibling.collision(event, random) == Result::ok);
    assert(visuals.revision() == shared_revision + 1 && sibling.marks().size() == 1);
    assert(visuals.deformations().size() == 1 && sibling.deformations().size() == 1);
    for (const auto& mesh : visuals.body_lods()) {
        const auto p = mesh.parts[0].positions[0];
        assert(p.x == .35f && p.y == .05f && p.z == 1);
        assert(mesh.parts[0].normals[0].y == 1);
    }
    // Four deformation records at most; mark state has an independent cap.
    for (int i = 0; i < 6; ++i)
        assert(visuals.collision(event, random) == Result::ok);
    assert(visuals.deformations().size() == 4);
    body = meshes(110);
    body[0].parts[0].native_uv.assign(110, {std::numeric_limits<float>::quiet_NaN(), .5f});
    assert(visuals.configure(body, bitmap(std::byte{1}), bitmap(std::byte{200}), "paint") ==
           Result::ok);
    draws = 0;
    assert(visuals.collision(event, random) == Result::ok && draws == 800 &&
           visuals.marks().size() == 100);
    // Native FISTP QWORD integer-indefinite has low EAX0, then pixel rectangles
    // clip to the left edge. The mark still consumes all eight draws.
    const auto bad = visuals.marks()[0].rectangles[0];
    assert(bad.left == 0 && bad.right == 4);
    assert(visuals.paint().rgba[0] == std::byte{1});
    assert(visuals.collision(event, random) == Result::ok && draws == 800);
    event.mass_kg = 0;
    assert(visuals.collision(event, random) == Result::invalid_argument);
    // Original regional repair restores clean rectangles and rebuilds meshes
    // from baseline plus the surviving deformation records, in record order.
    VehicleDamageVisuals repair_visuals;
    assert(repair_visuals.repair(VehicleRepairRegion::all, 1, 2, 4) == Result::unavailable);
    auto repair_body = meshes(4);
    const std::vector<Vec3> repair_points{{0, 0, 3}, {1, 0, 0}, {0, 0, -3}, {-1, 0, 0}};
    for (auto& mesh : repair_body) {
        mesh.parts[0].positions = repair_points;
        mesh.parts[0].uv = {{.2f, .2f}, {.4f, .4f}, {.6f, .6f}, {.8f, .8f}};
    }
    assert(repair_visuals.configure(repair_body, bitmap(std::byte{1}), bitmap(std::byte{200}),
                                    "repair-paint") == Result::ok);
    for (const auto point : repair_points) {
        auto hit = contact();
        hit.world_point = point;
        assert(repair_visuals.collision(hit, random) == Result::ok);
    }
    assert(repair_visuals.deformations().size() == 4 && repair_visuals.marks().size() == 4);
    const auto damaged = repair_visuals.capture_state();
    const auto before_invalid = repair_visuals.revision();
    assert(repair_visuals.repair(VehicleRepairRegion::all, 1.01f, 2, 4) ==
           Result::invalid_argument);
    assert(repair_visuals.revision() == before_invalid);
    assert(repair_visuals.repair(VehicleRepairRegion::front, .5f, 2, 4) == Result::ok);
    assert(repair_visuals.marks().size() == 3);
    assert(repair_visuals.deformations()[0].displacement.x == .125f);
    for (const auto& mesh : repair_visuals.body_lods()) {
        assert(mesh.parts[0].positions[0].x == .125f);
        assert(mesh.parts[0].positions[1].x == 1.25f);
        assert(mesh.parts[0].normals[0].y == 1);
    }
    const auto repaired_rect = damaged.marks[0].rectangles[0];
    const auto repaired_pixel = (repaired_rect.top * 256 + repaired_rect.left) * 4;
    assert(repair_visuals.paint().rgba[repaired_pixel] == std::byte{1});
    assert(repair_visuals.repair(VehicleRepairRegion::right, 1, 2, 4) == Result::ok);
    assert(repair_visuals.deformations().size() == 3 && repair_visuals.marks().size() == 2);
    assert(repair_visuals.body_lods()[0].parts[0].positions[1].x == 1);
    assert(repair_visuals.restore_state(damaged) == Result::ok);
    assert(repair_visuals.deformations().size() == 4 && repair_visuals.marks().size() == 4);
    assert(repair_visuals.body_lods()[0].parts[0].positions[0].x == .25f);
    assert(repair_visuals.paint().rgba[repaired_pixel] == std::byte{200});
    assert(repair_visuals.repair(VehicleRepairRegion::rear, 1, 2, 4) == Result::ok);
    assert(repair_visuals.body_lods()[0].parts[0].positions[2].x == 0);
    assert(repair_visuals.deformations().size() == 3 && repair_visuals.marks().size() == 3);
    assert(repair_visuals.repair(VehicleRepairRegion::left, 1, 2, 4) == Result::ok);
    assert(repair_visuals.body_lods()[0].parts[0].positions[3].x == -1);
    assert(repair_visuals.deformations().size() == 2 && repair_visuals.marks().size() == 2);
    auto invalid_state = damaged;
    invalid_state.texture_path = "another-paint";
    assert(repair_visuals.restore_state(invalid_state) == Result::invalid_argument);
    assert(repair_visuals.repair(VehicleRepairRegion::all, 1, 2, 4) == Result::ok);
    assert(repair_visuals.deformations().empty() && repair_visuals.marks().empty());
    for (const auto& mesh : repair_visuals.body_lods())
        for (std::size_t i = 0; i < repair_points.size(); ++i) {
            assert(mesh.parts[0].positions[i].x == repair_points[i].x);
            assert(mesh.parts[0].positions[i].z == repair_points[i].z);
            assert(mesh.parts[0].positions[i].y == 0);
        }
    // With more than one matching mark the removal quota rounds count*fraction.
    auto many_marks = damaged;
    many_marks.marks.assign(5, damaged.marks[0]);
    assert(repair_visuals.restore_state(many_marks) == Result::ok);
    assert(repair_visuals.repair(VehicleRepairRegion::front, .5f, 2, 4) == Result::ok);
    assert(repair_visuals.marks().size() == 2);
    // A geometry-only transfer does not import paint state.
    VehicleDamageVisuals repainted;
    assert(repainted.configure(repair_body, bitmap(std::byte{7}), bitmap(std::byte{200}),
                               "new-paint") == Result::ok);
    assert(repainted.reapply_deformations(damaged.deformations) == Result::ok);
    assert(repainted.marks().empty() && repainted.paint().rgba[repaired_pixel] == std::byte{7});
    assert(repainted.body_lods()[0].parts[0].positions[0].x == .25f);
    // Original repaint retains exact old mark rectangles and uses the NEW
    // livery's damage bitmap. Neither the old paint nor new RNG is imported.
    assert(repainted.configure(repair_body, bitmap(std::byte{7}), bitmap(std::byte{99}),
                               "new-paint") == Result::ok);
    const auto draws_before_repaint = draws;
    assert(repainted.repaint_damage_from(damaged) == Result::ok);
    assert(draws == draws_before_repaint);
    assert(repainted.marks().size() == damaged.marks.size());
    assert(repainted.marks()[0].rectangles[0].left == repaired_rect.left);
    assert(repainted.marks()[0].side == damaged.marks[0].side);
    assert(repainted.paint().rgba[repaired_pixel] == std::byte{99});
    assert(repainted.paint().rgba[0] == std::byte{7});
    assert(repainted.deformations().size() == damaged.deformations.size());
    assert(repainted.body_lods()[0].parts[0].positions[0].x == .25f);
    auto invalid_repaint = damaged;
    invalid_repaint.marks.resize(101);
    const auto before_bad_repaint = repainted.revision();
    assert(repainted.repaint_damage_from(invalid_repaint) == Result::invalid_argument);
    assert(repainted.revision() == before_bad_repaint);
    assert(repainted.paint().rgba[repaired_pixel] == std::byte{99});
    assert(repainted.repair(VehicleRepairRegion::front, 1, 2, 4) == Result::ok);
    assert(repainted.paint().rgba[repaired_pixel] == std::byte{7});
    const auto center_point = VehicleDeformation{{}, {.1f, .2f, .3f}};
    assert(repainted.reapply_deformations(std::span(&center_point, 1)) == Result::ok);
    assert(repainted.repair(VehicleRepairRegion::internal, .5f, 2, 4) == Result::ok);
    assert(repainted.deformations()[0].displacement.x == .05f);
    auto overlap_body = meshes();
    for (auto& mesh : overlap_body)
        mesh.parts[0].positions[0] = {.1f, 0, 0};
    assert(repainted.configure(overlap_body, bitmap(std::byte{7}), bitmap(std::byte{200}),
                               "overlap") == Result::ok);
    const std::array<VehicleDeformation, 2> overlap_dents{
        VehicleDeformation{{}, {.25f, 0, 0}},
        VehicleDeformation{{.6f, 0, 0}, {.1f, 0, 0}}};
    assert(repainted.reapply_deformations(overlap_dents) == Result::ok);
    assert(std::abs(repainted.body_lods()[0].parts[0].positions[0].x - .45f) < .00001f);
    // Removing the first dent changes which vertices the remaining dent reaches.
    // Subtracting .25 from the damaged position would incorrectly produce .20.
    assert(repainted.repair(VehicleRepairRegion::internal, 1, 2, 4) == Result::ok);
    assert(repainted.deformations().size() == 1);
    assert(repainted.body_lods()[0].parts[0].positions[0].x == .1f);
    VehicleDamageVisuals pixel_visuals;
    assert(pixel_visuals.configure(meshes(), bitmap(std::byte{1}), bitmap(std::byte{200}),
                                    "pixel-paint", TextureTransparency::color_key) == Result::ok);
    assert(pixel_visuals.mark_pixel_pair(0, 0, {0, 0, 255, 255}, {.5f, -.5f}) == Result::ok);
    assert(pixel_visuals.pixel_pairs().size() == 1);
    const auto pair = pixel_visuals.pixel_pairs()[0];
    assert(pair.x == 253 && pair.y == 1 && pair.previous[0][0] == std::byte{1});
    const auto pair_pixel = (pair.y * 256 + pair.x) * 4;
    assert(pixel_visuals.paint().rgba[pair_pixel] == std::byte{131});
    assert(pixel_visuals.paint().rgba[pair_pixel + 4] == std::byte{0});
    assert(pixel_visuals.paint().rgba[pair_pixel + 7] == std::byte{0});
    assert(pixel_visuals.repair(VehicleRepairRegion::rear, 1, 2, 4) == Result::ok);
    assert(pixel_visuals.pixel_pairs().size() == 1);
    const auto pair_snapshot = pixel_visuals.capture_state();
    assert(pixel_visuals.repair(VehicleRepairRegion::front, .25f, 2, 4) == Result::ok);
    assert(pixel_visuals.pixel_pairs().empty());
    assert(pixel_visuals.paint().rgba[pair_pixel] == std::byte{1});
    assert(pixel_visuals.restore_state(pair_snapshot) == Result::ok);
    assert(pixel_visuals.pixel_pairs().size() == 1);
    assert(pixel_visuals.paint().rgba[pair_pixel] == std::byte{131});
    assert(repainted.configure(meshes(), bitmap(std::byte{7}), bitmap(std::byte{99}),
                               "pixel-repaint", TextureTransparency::color_key) == Result::ok);
    assert(repainted.repaint_damage_from(pair_snapshot) == Result::ok);
    assert(repainted.pixel_pairs()[0].previous[0][0] == std::byte{7});
    assert(repainted.paint().rgba[pair_pixel] == std::byte{131});
    assert(repainted.repair(VehicleRepairRegion::all, 1, 2, 4) == Result::ok);
    assert(repainted.paint().rgba[pair_pixel] == std::byte{7});
    // Nonzero types still occupy the original record pool but change no pixels.
    assert(repainted.mark_pixel_pair(3, 1, {10, 10, 20, 20}, {}) == Result::ok);
    const auto no_change = (240 * 256 + 15) * 4;
    assert(repainted.paint().rgba[no_change] == std::byte{7});
    assert(repainted.repair(VehicleRepairRegion::right, 1, 2, 4) == Result::ok);
    assert(repainted.pixel_pairs().empty());
    // Three pair records use rounded proportional removal: round(3*.5)=2.
    for (int i = 0; i < 3; ++i)
        assert(repainted.mark_pixel_pair(2, 0, {10 + i * 3, 10, 12 + i * 3, 12}, {}) == Result::ok);
    assert(repainted.repair(VehicleRepairRegion::left, .5f, 2, 4) == Result::ok);
    assert(repainted.pixel_pairs().size() == 1);
    for (int i = 0; i < 110; ++i)
        assert(repainted.mark_pixel_pair(1, 1, {10, 10, 20, 20}, {}) == Result::ok);
    assert(repainted.pixel_pairs().size() == 100);
    const auto before_invalid_pair = repainted.revision();
    assert(repainted.mark_pixel_pair(4, 0, {10, 10, 20, 20}, {}) == Result::invalid_argument);
    assert(repainted.revision() == before_invalid_pair);
    VehicleDamageVisuals projectile_visuals;
    assert(projectile_visuals.configure(meshes(), bitmap(std::byte{1}), bitmap(std::byte{200}),
                                         "projectile-paint") == Result::ok);
    std::size_t projectile_draws{};
    ParticleRandom projectile_random([&] { ++projectile_draws; return 50u; });
    assert(projectile_visuals.projectile_damage(0, 0, projectile_random) == Result::unavailable);
    assert(projectile_draws == 0);
    std::array<VehicleDamageRect, 4> atlas_regions;
    for (std::size_t i = 0; i < atlas_regions.size(); ++i) {
        const auto x = 10 + static_cast<std::int32_t>(i) * 30;
        atlas_regions[i] = {x, 20, x + 10, 30};
    }
    assert(projectile_visuals.configure_projectile_regions(atlas_regions) == Result::ok);
    assert(projectile_visuals.projectile_damage(0, 0, projectile_random, true, false) == Result::ok);
    assert(projectile_visuals.projectile_damage(0, 0, projectile_random, false, true) == Result::ok);
    assert(projectile_draws == 0 && projectile_visuals.pixel_pairs().empty());
    constexpr std::array<std::uint32_t, 4> expected_sides{0, 3, 1, 2};
    for (std::size_t contact_side = 0; contact_side < 4; ++contact_side) {
        assert(projectile_visuals.projectile_damage(contact_side, 0, projectile_random) == Result::ok);
        const auto& generated = projectile_visuals.pixel_pairs()[contact_side];
        const auto side = expected_sides[contact_side];
        assert(generated.side == side);
        assert(generated.x == atlas_regions[side].left + 5 && generated.y == 230);
    }
    assert(projectile_draws == 8);
    for (int i = 0; i < 100; ++i)
        assert(projectile_visuals.mark_pixel_pair(0, 1, {10, 10, 20, 20}, {}) == Result::ok);
    assert(projectile_visuals.pixel_pairs().size() == 100);
    assert(projectile_visuals.projectile_damage(0, 0, projectile_random) == Result::ok);
    assert(projectile_draws == 10 && projectile_visuals.pixel_pairs().size() == 100);
    if (argc == 2) {
        Files files(argv[1]);
        ResourceStore resources(files);
        assert(resources.open() == Result::ok);
        std::string error;
        for (const auto* vehicle :
             {"Animal", "Avenging Angel", "Bud The Crud", "Daisy", "Electra", "Heartbreaker",
              "Kitiara", "Niki Roadkill", "Radcliff", "Romulus", "Sarge", "Wainright"})
            for (int paint = 1; paint <= 6; ++paint) {
                const auto result = visuals.load(resources, vehicle, paint, error);
                if (result != Result::ok)
                    std::cerr << vehicle << " paint " << paint << ": " << error << '\n';
                assert(result == Result::ok);
                assert(!visuals.body_lods()[0].parts.empty() && visuals.paint().width == 256);
                assert(visuals.texture_transparency() == TextureTransparency::color_key);
                auto actual = contact();
                actual.world_point = visuals.body_lods()[0].parts[0].positions[0];
                actual.width = 10;
                actual.length = 10;
                const auto before = draws;
                assert(visuals.collision(actual, random) == Result::ok);
                assert(!visuals.marks().empty() && draws - before == visuals.marks().size() * 8);
                const auto before_projectile = draws;
                assert(visuals.projectile_damage(0, 0, random) == Result::ok);
                assert(visuals.pixel_pairs().size() == 1 && draws == before_projectile + 2);
            }
    }
}
