#include "game_src/unresolved/scene_turret_combat.hpp"
#include "game_src/unresolved/vehicle_equipment_effects.hpp"
#include <bit>
#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>

using namespace roadwars;
namespace {
void u32(std::vector<std::byte>& bytes, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i)
        bytes.push_back(static_cast<std::byte>((value >> (8 * i)) & 255));
}
void number(std::vector<std::byte>& bytes, float value) {
    u32(bytes, std::bit_cast<std::uint32_t>(value));
}
void vector(std::vector<std::byte>& bytes, Vec3 p) {
    number(bytes, p.x);
    number(bytes, p.y);
    number(bytes, p.z);
}
void name(std::vector<std::byte>& bytes, std::string_view value) {
    for (std::size_t i = 0; i < 32; ++i)
        bytes.push_back(i < value.size() ? static_cast<std::byte>(value[i]) : std::byte{});
}
std::vector<std::byte> mesh(bool muzzle) {
    std::vector<std::byte> bytes;
    name(bytes, "Root");
    u32(bytes, 1);
    u32(bytes, muzzle ? 1 : 0);
    vector(bytes, {100, 200, 300});
    u32(bytes, 0); // Attachment must discard this root offset.
    if (muzzle) {
        name(bytes, "295ULFire");
        u32(bytes, 1);
        u32(bytes, 0);
        vector(bytes, {0, 0, .5f});
        u32(bytes, 0);
    }
    return bytes;
}
class Reader final : public ResourceReader {
  public:
    bool missing_muzzle{};
    Result read(std::string_view name, std::vector<std::byte>& output) override {
        const auto key = ResourceStore::canonical(name);
        if (key == "CARS/SHARED/WEAPONS/FRONT/20MM CANNON/SPATIALOBJECT.MSH") {
            output = mesh(!missing_muzzle);
            return Result::ok;
        }
        if (key == "CARS/SHARED/WEAPONS/FRONT/20MM CANNON/AMMOSPATIALOBJECT.MSH") {
            output = mesh(false);
            return Result::ok;
        }
        std::string text;
        if (key == "TRACKS/TEST/WORLD/DATA.TXT")
            text = "[DATA]\nGUNS=1\n";
        else if (key == "CARS/SHARED/WEAPONS/FRONT/20MM CANNON/DATA.INI")
            text = "[PRICE]\nDEFAULTAMMO=100\n[DAMAGE]\nHITPOINTS=4\n[PHYSICS]\n"
                   "AMMO_COUNT=1\nAMMO_TOP_SPEED=200\nAMMO_ACC=1\n"
                   "AMMO_IMPACT_MULTIPLIER=3\nAMMO_YROTATIONAL_SPEED=360\n"
                   "AMMO_MASS_IN_POUNDS=200\nAMMO_TRACKING_TIMER_SECONDS=0\n";
        else
            return Result::not_found;
        output.resize(text.size());
        std::memcpy(output.data(), text.data(), text.size());
        return Result::ok;
    }
};
class DirectoryReader final : public ResourceReader {
  public:
    explicit DirectoryReader(std::filesystem::path root) : root_(std::move(root)) {}
    Result read(std::string_view name, std::vector<std::byte>& output) override {
        std::ifstream file(root_ / std::filesystem::path(name), std::ios::binary | std::ios::ate);
        if (!file)
            return Result::not_found;
        const auto size = file.tellg();
        if (size < 0)
            return Result::unavailable;
        output.resize(static_cast<std::size_t>(size));
        file.seekg(0);
        return file.read(reinterpret_cast<char*>(output.data()), size) ? Result::ok
                                                                       : Result::unavailable;
    }

  private:
    std::filesystem::path root_;
};
} // namespace
int main(int argc, char** argv) {
    Reader reader;
    ResourceStore resources(reader);
    MeshObject world;
    world.name = "World";
    MeshObject node;
    node.name = "Turret_090_Full_C";
    node.position = {0, 1, 0};
    node.parts.resize(1);
    node.parts[0].positions = {{-1, -1, -1}, {1, 1, 1}};
    world.children.push_back(node);
    SceneTurretCombat combat;
    std::string error;
    assert(combat.load(resources, "Test", world, error) == Result::ok);
    assert(combat.assets().size() == 1 && combat.assets()[0].weapon.position.x == 0);
    assert(combat.assets()[0].muzzle_local_positions[0].z == .5f);
    assert(combat.assets()[0].weapon.children[0].visible == false);
    assert(combat.callback_tokens().empty());
    TurretTarget aim;
    aim.id = 0;
    aim.position = {40, 1, 0};
    WeaponProjectileTarget body;
    body.identity = 1;
    body.participant = 0;
    body.position = aim.position;
    body.height = 2;
    body.footprint = {{{38, 1, 2}, {42, 1, 2}, {42, 1, -2}, {38, 1, -2}}};
    TrackCollision track;
    SceneCombatServices services;
    services.impact = [](const SceneCombatImpact&) { return Result::ok; };
    VehicleEquipmentEffects health;
    health.internal_hp = health.maximum_internal_hp = 100;
    health.armor_hp = health.base_armor_hp = health.maximum_armor_hp = {100, 100, 100, 100};
    services.damage = [&](const SceneCombatDamageEvent& event) {
        assert(event.participant == 0 && event.hitpoints == 4 && !event.has_impulse);
        return damage_vehicle_contact(health, event.contact_side, event.hitpoints);
    };
    assert(combat.advance(.01f, {&aim, 1}, {&body, 1}, track, {}, error) == Result::unavailable);
    for (unsigned tick = 0; tick < 240; ++tick)
        assert(combat.advance(1.0f / 60.0f, {&aim, 1}, {&body, 1}, track, services, error, false) ==
               Result::ok);
    assert(combat.launched_projectiles() == 0 && !combat.turrets().poses()[0].target);
    for (unsigned tick = 0; tick < 420; ++tick) {
        const auto result =
            combat.advance(1.0f / 60.0f, {&aim, 1}, {&body, 1}, track, services, error);
        assert(result == Result::ok);
    }
    if (combat.launched_projectiles() == 0 || combat.vehicle_impacts() == 0)
        throw std::runtime_error(
            "No scene contact: launched=" + std::to_string(combat.launched_projectiles()) +
            " impacts=" + std::to_string(combat.vehicle_impacts()) +
            " yaw=" + std::to_string(combat.turrets().poses()[0].heading) +
            " pitch=" + std::to_string(combat.turrets().poses()[0].pitch) +
            " target=" + std::to_string(combat.turrets().poses()[0].target.has_value()) +
            " guns=" + std::to_string(combat.turrets().guns_enabled()));
    assert(health.armor_hp[0] + health.armor_hp[1] + health.armor_hp[2] + health.armor_hp[3] < 400);
    assert(combat.projectiles().size() == 6); // Five projectiles plus the actual muzzle subtree.
    combat.reset();
    assert(combat.launched_projectiles() == 0 && combat.vehicle_impacts() == 0);
    for (const auto& visual : combat.projectiles())
        assert(!visual.visible);
    reader.missing_muzzle = true;
    assert(combat.load(resources, "Test", world, error) == Result::unavailable);
    assert(combat.assets().size() == 1); // Failed load preserves the live scene.

    if (argc == 2) {
        const std::filesystem::path root(argv[1]);
        DirectoryReader installed(root);
        ResourceStore original(installed);
        assert(original.open() == Result::ok);
        for (const auto& entry : std::filesystem::directory_iterator(root / "Tracks")) {
            if (!entry.is_directory() || entry.path().filename() == "Shared")
                continue;
            const auto name = entry.path().filename().string();
            std::vector<std::byte> bytes;
            MeshObject scene;
            assert(original.read("Tracks/" + name + "/World/SpatialObject.msh", bytes) ==
                   Result::ok);
            assert(decode_msh(bytes, scene, error) == Result::ok);
            assert(combat.load(original, name, scene, error) == Result::ok);
            for (const auto& asset : combat.assets()) {
                assert(asset.weapon.position.x == 0 && asset.weapon.position.y == 0 &&
                       asset.weapon.position.z == 0);
                if (asset.rocket)
                    assert(asset.rocket_shape.length > 0 && asset.rocket_shape.width > 0 &&
                           asset.rocket_shape.height > 0);
                else
                    assert(!asset.muzzle_local_positions.empty());
            }
        }
    }
}
