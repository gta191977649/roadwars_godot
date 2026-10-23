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
            }
    }
}
