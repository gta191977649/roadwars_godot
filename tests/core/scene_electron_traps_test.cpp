#include "game_src/unresolved/scene_electron_traps.hpp"
#include <bit>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <stdexcept>
using namespace roadwars;
namespace {
void u32(std::vector<std::byte>& b, std::uint32_t n) {
    for (unsigned i = 0; i < 4; ++i)
        b.push_back(static_cast<std::byte>((n >> (i * 8)) & 255));
}
void u16(std::vector<std::byte>& b, unsigned n) {
    b.push_back(static_cast<std::byte>(n & 255));
    b.push_back(static_cast<std::byte>(n >> 8));
}
void number(std::vector<std::byte>& b, float n) { u32(b, std::bit_cast<std::uint32_t>(n)); }
class Reader : public ResourceReader {
  public:
    Result read(std::string_view path, std::vector<std::byte>& b) override {
        b.clear();
        if (path.ends_with("LightningAnim.ska")) {
            u32(b, 1);
            u32(b, 10);
            for (unsigned i = 0; i < 16; ++i)
                number(b, i % 5 == 0 ? 1.f : 0.f);
        } else if (path.ends_with("Lightning.ska")) {
            u32(b, 3);
            for (unsigned v = 0; v < 3; ++v)
                for (unsigned f = 0; f < 8; ++f)
                    number(b, f == 0 ? static_cast<float>(v) : 0);
            u32(b, 1);
            u16(b, 0);
            u16(b, 1);
            u16(b, 2);
            for (unsigned i = 0; i < 64; ++i)
                b.push_back(std::byte{});
            u32(b, 3);
            u16(b, 0);
            u16(b, 1);
            u16(b, 2);
            u32(b, 0);
        } else
            return Result::not_found;
        return Result::ok;
    }
};
class DirectoryReader : public ResourceReader {
  public:
    explicit DirectoryReader(std::filesystem::path value) : root(std::move(value)) {}
    Result read(std::string_view path, std::vector<std::byte>& b) override {
        std::ifstream f(root / std::filesystem::path(path), std::ios::binary | std::ios::ate);
        if (!f)
            return Result::not_found;
        const auto size = f.tellg();
        if (size < 0)
            return Result::unavailable;
        b.resize(static_cast<std::size_t>(size));
        f.seekg(0);
        return f.read(reinterpret_cast<char*>(b.data()), size) ? Result::ok : Result::unavailable;
    }
    std::filesystem::path root;
};
void require(Result result, const std::string& error) {
    if (result != Result::ok)
        throw std::runtime_error(error);
}
} // namespace
int main(int argc, char** argv) {
    Reader reader;
    ResourceStore resources(reader);
    MeshObject world;
    for (unsigned i = 1; i <= 2; ++i)
        for (unsigned endpoint = 1; endpoint <= 2; ++endpoint) {
            MeshObject node;
            node.name = "Electron" + std::to_string(i) + "_" + std::to_string(endpoint);
            node.type = 4;
            node.position = {endpoint == 1 ? 0.f : 10.f, 0, static_cast<float>((i - 1) * 100)};
            node.dummy_heading_degrees = 90;
            world.children.push_back(node);
        }
    SceneElectronTraps traps;
    std::string error;
    world.type = 4;
    world.dummy_heading_degrees = 90; // Static serialized dummy basis is not runtime Euler.
    require(traps.load(resources, world, error), error);
    assert(traps.poses().size() == 2 && traps.callback_tokens().empty());
    assert(std::abs(traps.poses()[0].range - 13) < .0001f);
    assert(traps.poses()[0].timer == 0 && traps.poses()[1].timer == 1.5f);
    require(traps.advance_world(0, error), error);
    assert(traps.callback_tokens().empty()); // Strict timer<0.
    require(traps.advance_world(.1f, error), error);
    assert(traps.callback_tokens() == std::vector<std::uint64_t>{1});
    VehicleCombatTarget target;
    target.participant = 0;
    target.identity = 1;
    target.footprint = {{{4, 999, 1}, {6, 999, 1}, {6, 999, -1}, {4, 999, -1}}};
    target.velocity = {1, 2, 3};
    VehicleCombatServices services;
    unsigned hits{};
    services.effect_count = [](VehicleCombatEffectKey, std::size_t& count) {
        count = 0;
        return Result::ok;
    };
    services.effect_step = [](const VehicleCombatEffectStep& effect) {
        if (effect.emitting)
            assert(effect.velocity.y == 2);
        return Result::ok;
    };
    services.damage = [&](const VehicleCombatDamage& damage) {
        assert(damage.hitpoints == 20 && !damage.internal_damage && !damage.has_impulse);
        ++hits;
        return Result::ok;
    };
    require(traps.dispatch_callback(1, .1f, {&target, 1}, services, error), error);
    assert(hits == 4 && traps.poses()[0].beam_visible);
    assert(traps.geometry(0) && traps.geometry(0)->frame_count() == 1);
    assert(traps.dispatch_callback(0, .1f, {}, services, error) == Result::invalid_argument);
    assert(traps.dispatch_callback(1, .1f, {}, {}, error) == Result::unavailable);
    for (unsigned step = 0; step < 17; ++step) {
        require(traps.advance_world(.1f, error), error);
        for (const auto token : traps.callback_tokens())
            require(traps.dispatch_callback(token, .1f, {}, services, error), error);
    }
    assert(!traps.poses()[0].active && !traps.poses()[0].beam_visible);
    auto missing = world;
    missing.children.erase(missing.children.begin() + 1);
    assert(traps.load(resources, missing, error) == Result::unavailable);
    assert(traps.poses().size() == 2);
    if (argc == 2) {
        DirectoryReader directory(argv[1]);
        ResourceStore installed(directory);
        require(installed.open(), error);
        unsigned total{};
        for (const auto& entry : std::filesystem::directory_iterator(directory.root / "Tracks")) {
            if (!entry.is_directory() || entry.path().filename() == "Shared")
                continue;
            const auto path =
                "Tracks/" + entry.path().filename().string() + "/World/SpatialObject.msh";
            std::vector<std::byte> bytes;
            require(installed.read(path, bytes), path);
            MeshObject scene;
            require(decode_msh(bytes, scene, error), error);
            require(traps.load(installed, scene, error), error);
            total += static_cast<unsigned>(traps.poses().size());
        }
        assert(total == 4);
    }
}
