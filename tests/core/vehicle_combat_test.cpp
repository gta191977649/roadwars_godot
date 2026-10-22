#include "game_src/unresolved/contact_orientation.hpp"
#include "game_src/unresolved/vehicle_combat.hpp"
#include "game_src/unresolved/weapon_trails.hpp"
#include <bit>
#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
using namespace roadwars;
namespace {
void u32(std::vector<std::byte>& b, std::uint32_t n) {
    for (unsigned i = 0; i < 4; ++i)
        b.push_back(static_cast<std::byte>((n >> (i * 8)) & 255));
}
void number(std::vector<std::byte>& b, float n) { u32(b, std::bit_cast<std::uint32_t>(n)); }
void name(std::vector<std::byte>& b, std::string_view n) {
    for (unsigned i = 0; i < 32; ++i)
        b.push_back(i < n.size() ? static_cast<std::byte>(n[i]) : std::byte{});
}
void node(std::vector<std::byte>& b, std::string_view n, Vec3 p, unsigned children) {
    name(b, n);
    u32(b, 1);
    u32(b, children);
    number(b, p.x);
    number(b, p.y);
    number(b, p.z);
    u32(b, 0);
}
class Reader : public ResourceReader {
  public:
    bool missing_muzzle{};
    Result read(std::string_view path, std::vector<std::byte>& out) override {
        const auto key = ResourceStore::canonical(path);
        out.clear();
        if (key == "CARS/TEST/BODY/SPATIALOBJECT.MSH") {
            node(out, "Body", {}, 2);
            node(out, "FrontWeaponMount", {0, 1, 0}, 0);
            node(out, "RearWeaponMount", {0, 1, -1}, 0);
        } else if (key.ends_with("/20MM CANNON/SPATIALOBJECT.MSH")) {
            node(out, "Cannon", {100, 200, 300}, missing_muzzle ? 0 : 1);
            if (!missing_muzzle)
                node(out, "295ULFire", {0, 0, .5f}, 0);
        } else if (key.ends_with("/20MM CANNON/AMMOSPATIALOBJECT.MSH"))
            node(out, "Bullet", {}, 0);
        else if (key.ends_with("/DATA.INI")) {
            const std::string text = "[PRICE]\nDEFAULTAMMO=2\n[DAMAGE]\nHITPOINTS=4\n"
                                     "[PHYSICS]\nAMMO_COUNT=1\nAMMO_TOP_SPEED=200\nAMMO_ACC=1\n"
                                     "AMMO_IMPACT_MULTIPLIER=3\nAMMO_YROTATIONAL_SPEED=360\n"
                                     "AMMO_MASS_IN_POUNDS=200\nAMMO_TRACKING_TIMER_SECONDS=0\n";
            out.resize(text.size());
            std::memcpy(out.data(), text.data(), text.size());
        } else
            return Result::not_found;
        return Result::ok;
    }
};
class DirectoryReader : public ResourceReader {
  public:
    explicit DirectoryReader(std::filesystem::path p) : root(std::move(p)) {}
    Result read(std::string_view path, std::vector<std::byte>& out) override {
        std::ifstream stream(root / std::filesystem::path(path), std::ios::binary | std::ios::ate);
        if (!stream)
            return Result::not_found;
        const auto count = stream.tellg();
        if (count < 0)
            return Result::unavailable;
        out.resize(static_cast<std::size_t>(count));
        stream.seekg(0);
        return stream.read(reinterpret_cast<char*>(out.data()), count) ? Result::ok
                                                                       : Result::unavailable;
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
    VehicleCombat combat;
    std::string error;
    assert(!combat.request(WeaponSlot::front, 10));
    require(combat.load(resources, "Test", {"20mm Cannon", "", ""}, error), error);
    assert(combat.assets()[0].weapon.position.x == 0);
    assert(combat.assets()[0].muzzle_positions[0].z == .5f);
    assert(combat.slot(WeaponSlot::front).ammunition == 2);
    std::vector<std::pair<std::uint64_t, bool>> observed;
    combat.observe_callbacks(
        [&](std::uint64_t token, bool insert) { observed.emplace_back(token, insert); });
    assert(combat.request(WeaponSlot::front, 0));
    assert(!combat.request(WeaponSlot::front, 0));
    assert(combat.slot(WeaponSlot::front).ammunition == 1);
    VehicleCombatTarget target;
    target.identity = 2;
    target.participant = 1;
    target.position = {0, 1, 10};
    target.height = 2;
    target.footprint = {{{-2, 1, 12}, {2, 1, 12}, {2, 1, 8}, {-2, 1, 8}}};
    VehicleCombatFrame frame;
    frame.owner_identity = 1;
    frame.forward = {0, 0, 1};
    frame.targets = {&target, 1};
    TrackCollision track;
    assert(combat.callback_tokens() == std::vector<std::uint64_t>{1});
    require(combat.dispatch_callback(1, 1.0f / 60, frame, track, nullptr, {}, error), error);
    assert(combat.callback_tokens() == std::vector<std::uint64_t>{33});
    assert((observed ==
            std::vector<std::pair<std::uint64_t, bool>>{{1, true}, {33, true}, {1, false}}));
    assert(combat.dispatch_callback(1, 0, frame, track, nullptr, {}, error) ==
           Result::invalid_argument);
    unsigned contacts{};
    VehicleCombatServices services;
    services.impact = [](const VehicleCombatImpact& event) {
        assert(event.hit.kind == ProjectileHitKind::vehicle);
        return Result::ok;
    };
    services.damage = [&](const VehicleCombatDamage& event) {
        assert(event.participant == 1 && event.hitpoints == 4);
        assert(!event.internal_damage && !event.has_impulse);
        ++contacts;
        return Result::ok;
    };
    for (unsigned tick = 0; tick < 120; ++tick)
        require(combat.advance(1.0f / 60, frame, track, nullptr, services, error), error);
    assert(contacts == 1 && combat.slot(WeaponSlot::front).ready);
    assert(combat.request(WeaponSlot::front, 0));
    for (unsigned tick = 0; tick < 120; ++tick)
        require(combat.advance(1.0f / 60, frame, track, nullptr, services, error), error);
    assert(contacts == 2 && !combat.request(WeaponSlot::front, 0));
    reader.missing_muzzle = true;
    assert(combat.load(resources, "Test", {"20mm Cannon", "", ""}, error) == Result::unavailable);
    assert(combat.slot(WeaponSlot::front).ammunition == 0); // Transactional load.
    assert(combat.load(resources, "Test", {"", "20mm Cannon", ""}, error) ==
           Result::invalid_argument);
    require(combat.load(resources, "Test", {"", "", "Smoke Screen"}, error), error);
    assert(combat.request(WeaponSlot::rear, 0));
    require(combat.advance(1.0f / 60, frame, track, nullptr, {}, error), error);
    assert(combat.advance(1.0f / 60, frame, track, nullptr, {}, error) == Result::unavailable);
    require(combat.load(resources, "Test", {"", "", "Oil Slick"}, error), error);
    frame.route_node = 0;
    unsigned oil_draws{};
    services.random = [&]() { return oil_draws++; };
    assert(combat.request(WeaponSlot::rear, 0));
    require(combat.advance(.01f, frame, track, nullptr, services, error), error);
    for (unsigned tick = 0; tick < 70; ++tick) {
        frame.vehicle_pose.origin.x = tick % 2 ? 10.f : -10.f;
        require(combat.advance(.01f, frame, track, nullptr, services, error), error);
    }
    assert(combat.oil_slick()->patches[0].triangles.size() == 50);
    assert(combat.oil_visual_triangles().size() == 49 && oil_draws == 49);
    for (unsigned i = 0; i < 49; ++i)
        assert(combat.oil_visual_triangles()[i].atlas_frame == i % 4);
    if (argc == 2) {
        DirectoryReader directory(argv[1]);
        ResourceStore installed(directory);
        require(installed.open(), "Original resource index unavailable");
        unsigned cars{};
        std::string first_car;
        for (const auto& entry : std::filesystem::directory_iterator(directory.root / "Cars")) {
            if (!entry.is_directory() || !std::filesystem::exists(entry.path() / "Body"))
                continue;
            ++cars;
            if (first_car.empty())
                first_car = entry.path().filename().string();
            for (const auto& descriptor : weapon_descriptors()) {
                std::array<std::string, 3> names;
                names[static_cast<std::size_t>(descriptor.slot)] = descriptor.name;
                require(combat.load(installed, entry.path().filename().string(), names, error),
                        error);
                const auto view = combat.slot(descriptor.slot);
                assert(view.descriptor == &descriptor);
                assert(view.ammunition == view.parameters.default_ammunition);
                if (descriptor.slot == WeaponSlot::side) {
                    const auto& asset = combat.assets()[1];
                    assert(asset.has_mount_housing);
                    assert(asset.mount_housing_meshes[0].name == "ULRORMount1");
                    assert(asset.mount_housing_meshes[1].name == "ROR1");
                    for (const auto& part : asset.mount_housing_meshes)
                        assert(part.position.x == 0 && part.position.y == 0 &&
                               part.position.z == 0);
                }
            }
        }
        assert(cars != 0);
        // Actual assets with isolated typed services. This owner-only harness does not
        // claim the race-global callback/RNG order or a rendered executable comparison.
        for (const auto& descriptor : weapon_descriptors()) {
            std::array<std::string, 3> names;
            names[static_cast<std::size_t>(descriptor.slot)] = descriptor.name;
            require(combat.load(installed, first_car, names, error), error);
            const auto initial = combat.slot(descriptor.slot).ammunition;
            if (descriptor.name == "Battering Spikes") {
                assert(initial == 0 && !combat.request(descriptor.slot, 0, 20, 0));
                Vec3 amplified;
                assert(combat.contact_weapon()->modify_contact_force({0, 0, 10}, amplified) ==
                           Result::ok &&
                       amplified.z == 1010);
                require(combat.load(installed, first_car, {}, error), error);
                assert(combat.callback_tokens().empty());
                continue;
            }
            VehicleCombatFrame isolated;
            isolated.owner_identity = 1;
            isolated.forward = {0, 0, 1};
            isolated.route_node = 0;
            isolated.heading = 20;
            isolated.pitch = 15;
            isolated.roll = 20;
            isolated.body_pitch = 11;
            isolated.body_roll = 17;
            isolated.vehicle_pose = original_vehicle_transform({0, 10, 0}, 20, 15, 20);
            ParticleRandom random(1);
            std::array<WeaponTrailPool, 30> pools;
            std::array<bool, 30> configured{};
            VehicleCombatServices fixture;
            unsigned target_selections{};
            fixture.select_guided_target = [&] {
                ++target_selections;
                return Result::ok;
            };
            fixture.random = [&] { return random.next(); };
            fixture.damage = [](const VehicleCombatDamage&) { return Result::ok; };
            fixture.impact = [](const VehicleCombatImpact&) { return Result::ok; };
            fixture.jam = [](std::size_t, float) { return Result::ok; };
            fixture.effect_count = [&](VehicleCombatEffectKey key, std::size_t& count) {
                count = pools.at(key.projectile).active_count();
                return Result::ok;
            };
            fixture.effect_step = [&](const VehicleCombatEffectStep& step) {
                auto& pool = pools.at(step.key.projectile);
                if (!configured.at(step.key.projectile)) {
                    const auto result = pool.configure(descriptor.name, 1);
                    if (result != Result::ok)
                        return result;
                    configured.at(step.key.projectile) = true;
                }
                return pool.advance(step.delta, step.emitting, step.position, step.velocity, random,
                                    step.heading);
            };
            if (!combat.request(descriptor.slot, 0, isolated.heading, isolated.pitch,
                                isolated.body_pitch))
                throw std::runtime_error(std::string(descriptor.name) +
                                         " initial request ammo=" + std::to_string(initial));
            assert(!combat.request(descriptor.slot, 0, isolated.heading, isolated.pitch,
                                   isolated.body_pitch));
            assert(combat.slot(descriptor.slot).ammunition == initial - 1);
            combat.refresh_visuals(isolated);
            if (descriptor.slot == WeaponSlot::side) {
                unsigned housing{};
                for (const auto& visual : combat.visuals())
                    if (visual.kind == VehicleCombatVisualKind::mount_housing && visual.visible)
                        ++housing;
                assert(housing == 2);
            }
            if (descriptor.name == "Rocket Launcher" || weapon_is_guided(descriptor.name)) {
                std::size_t attached{};
                for (const auto& visual : combat.visuals())
                    if (visual.kind == VehicleCombatVisualKind::projectile && visual.visible)
                        ++attached;
                assert(attached == static_cast<std::size_t>(
                                       combat.slot(descriptor.slot).parameters.ammunition_count));
            }
            bool checked_guided_sound{};
            for (unsigned tick = 0; tick < 1800; ++tick) {
                require(combat.advance(1.0f / 60, isolated, track, nullptr, fixture, error),
                        std::string(descriptor.name) + ": " + error);
                if (weapon_is_guided(descriptor.name) && !checked_guided_sound &&
                    combat.slot(descriptor.slot).active) {
                    const auto audio = combat.drain_audio_events();
                    assert(audio.size() == 1 && audio[0].sample == CombatSample::heat);
                    const auto first = combat.guided_missiles().front().position;
                    assert(audio[0].position.x == first.x && audio[0].position.y == first.y &&
                           audio[0].position.z == first.z);
                    checked_guided_sound = true;
                }
            }
            assert(!weapon_is_guided(descriptor.name) || checked_guided_sound);
            assert(target_selections == (weapon_is_guided(descriptor.name) ? 1u : 0u));
            const bool persistent =
                descriptor.name == "Battering Spikes" || descriptor.name == "Land Mines" ||
                descriptor.name == "Reactive Mines" || descriptor.name == "Arachnoids";
            assert(persistent || combat.callback_tokens().empty());
            assert(persistent || combat.slot(descriptor.slot).ready);
            // Destroy all coordinator-owned active instances through a successful reload.
            // Persistent mines have no invented lifetime timeout.
            require(combat.load(installed, first_car, {}, error), error);
            assert(combat.callback_tokens().empty() && combat.drain_audio_events().empty());
            for (const auto& visual : combat.visuals())
                assert(!visual.visible);
        }
    }
}
