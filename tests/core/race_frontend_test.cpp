#include "game_src/unresolved/race_frontend.hpp"
#include <cassert>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <limits>

using namespace roadwars;
class HudRenderer final : public Renderer {
  public:
    std::set<std::uint64_t> textures;
    std::vector<HudQuad> frame;
    std::uint64_t next{1};
    MeshHandle upload_mesh(std::span<const Vec3>, std::span<const std::uint32_t>) override {
        return {};
    }
    InstanceHandle create_instance(MeshHandle) override { return {}; }
    Result update_instance(InstanceHandle, const Transform&) override { return Result::ok; }
    void destroy_instance(InstanceHandle) override {}
    void destroy_mesh(MeshHandle) override {}
    void update_camera(const Transform&, float) override {}
    TextureHandle upload_texture(std::uint32_t width, std::uint32_t height,
                                 std::span<const std::byte> bytes) override {
        assert(bytes.size() == static_cast<std::size_t>(width) * height * 4);
        textures.insert(next);
        return {next++};
    }
    void destroy_texture(TextureHandle texture) override { assert(textures.erase(texture.value)); }
    Result draw_hud(std::span<const HudQuad> quads) override {
        for (const auto& quad : quads)
            assert(textures.contains(quad.texture.value));
        frame.assign(quads.begin(), quads.end());
        return Result::ok;
    }
};
class Files final : public ResourceReader {
  public:
    explicit Files(std::filesystem::path root) : root_(std::move(root)) {}
    Result read(std::string_view name, std::vector<std::byte>& output) override {
        std::ifstream input(root_ / std::filesystem::path(name), std::ios::binary | std::ios::ate);
        if (!input)
            return Result::not_found;
        const auto size = input.tellg();
        if (size < 0)
            return Result::invalid_argument;
        output.resize(static_cast<std::size_t>(size));
        input.seekg(0);
        input.read(reinterpret_cast<char*>(output.data()), size);
        return input ? Result::ok : Result::invalid_argument;
    }

  private:
    std::filesystem::path root_;
};
int main(int argc, char** argv) {
    // 00489fbb sums all configured slots, including the active unfinished lap.
    FrontendRacer timing;
    timing.lap_times = {31.25f, 30.5f, 2.75f};
    timing.completed_laps = 2;
    RaceLapSummary summary;
    assert(race_lap_summary(timing, 3, summary) == Result::ok);
    assert(summary.best_lap == 30.5f && summary.total == 64.5f);
    timing.lap_times[2] += 1;
    assert(race_lap_summary(timing, 3, summary) == Result::ok && summary.total == 65.5f);
    timing.eliminated = true;
    assert(race_lap_summary(timing, 3, summary) == Result::ok && summary.total_dnf &&
           !summary.best_dnf);
    timing.completed_laps = 0;
    assert(race_lap_summary(timing, 3, summary) == Result::ok && summary.best_dnf);
    timing.finished = true;
    assert(race_lap_summary(timing, 3, summary) == Result::ok && !summary.total_dnf &&
           !summary.best_dnf);
    assert(race_lap_summary(timing, 0, summary) == Result::invalid_argument);
    timing.lap_times[0] = std::numeric_limits<float>::quiet_NaN();
    assert(race_lap_summary(timing, 3, summary) == Result::invalid_argument);
    assert(format_race_time(0) == "NO RECORD");
    assert(format_race_time(0, "EMPTY") == "EMPTY");
    assert(format_race_time(64.5f) == "01'04''50");
    assert(format_race_time(.125f) == "00'00''12");
    assert(format_race_time(600.75f) == "10'00''75");
    // Hand-derived fixed outputs from 0048adf5..0048aebc, including __ftol truncation.
    FrontendMapSettings map{{100, 0, 200}, 0.5f, 67, 52, 90};
    assert((map.project({100, 0, 200}) == std::array<int, 2>{67, 404}));
    assert((map.project({120, 0, 240}) == std::array<int, 2>{47, 394}));
    map.rotation_degrees = -90;
    assert((map.project({120, 0, 240}) == std::array<int, 2>{87, 414}));
    map.rotation_degrees = 180;
    assert((map.project({120, 0, 240}) == std::array<int, 2>{77, 384}));
    map.rotation_degrees = 0;
    assert((map.project({120, 0, 240}) == std::array<int, 2>{57, 424}));
    map.start_x = 0;
    assert(map.project({103, 0, 200})[0] == -1);

    // Two rows with a blank row delimiter and double blank columns at row ends.
    Bitmap atlas;
    atlas.width = 8;
    atlas.height = 6;
    atlas.rgba.resize(8 * 6 * 4);
    for (int y : {0, 1, 3, 4})
        for (int x : {0, 1, 3, 4})
            atlas.rgba[(y * 8 + x) * 4] = std::byte{255};
    const std::array<unsigned char, 4> alphabet{'A', 'B', '0', '1'};
    HudBitmapFont font;
    assert(font.decode(atlas, alphabet) == Result::ok);
    assert(font.height() == 2 && font.space_width() == 2);
    assert(font.glyph('B').x == 3 && font.glyph('0').y == 3);
    atlas.rgba.pop_back();
    assert(font.decode(atlas, alphabet) == Result::invalid_argument);
    AmmunitionNotices notices;
    assert(notices.push("MACHINE GUN", 99));
    assert(notices.entries()[0].text == "MACHINE GUN 99" && notices.entries()[0].remaining == 2);
    assert(notices.advance(.5f) == Result::ok);
    assert(notices.push("machine gun", 98));
    assert(notices.entries()[0].text == "MACHINE GUN 98" && notices.entries()[0].remaining == 2);
    assert(notices.push("GATLING GUN", 9));
    assert(notices.push("LAND MINES", 2));
    assert(notices.push("NITROUS BOOST", 0));
    assert(!notices.push("OTHER", 1));
    assert(notices.advance(-1) == Result::invalid_argument);
    assert(notices.advance(std::numeric_limits<float>::infinity()) == Result::invalid_argument);
    assert(notices.entries()[0].remaining == 2);
    assert(notices.advance(3) == Result::ok);
    for (const auto& notice : notices.entries()) assert(notice.remaining == 0);
    assert(!notices.push("", 1) && !notices.push("MACHINE GUN", -1));
    notices.clear();
    assert(notices.push("MACHINE GUN", 1));
    assert(notices.advance(.5f) == Result::ok);
    assert(notices.push("NITROUS BOOST", 1));
    assert(notices.advance(1.5f) == Result::ok);
    assert(notices.push("NITROUS BOOST", 0));
    // The original loop takes an earlier empty lane before a later matching one.
    assert(notices.entries()[0].text == "NITROUS BOOST 0" &&
           notices.entries()[1].text == "NITROUS BOOST 1");
    notices.clear();
    for (const auto& notice : notices.entries())
        assert(notice.remaining == 0 && notice.text.empty());
    if (argc < 2)
        return 0;

    Files files(argv[1]);
    ResourceStore resources(files);
    assert(resources.open() == Result::ok);
    HudRenderer renderer;
    RaceFrontend hud(renderer);
    std::string error;
    const std::array<std::string, 1> characters{"Animal"};
    const auto result = hud.load(resources, "Dark Forest", {}, characters, error);
    if (result != Result::ok) {
        std::cerr << error << '\n';
        return 1;
    }
    assert(hud.map_settings().rotation_degrees == 90);
    assert(std::abs(hud.map_settings().scale - 0.0459997f) < 1e-8f);
    const auto texture_count = renderer.textures.size();
    assert(texture_count == 25); // HUD/cockpit textures and the opaque transition pixel.
    std::array<FrontendRacer, 1> racers{{{{0, 0, 0}, 0}}};
    FrontendFrame frame;
    frame.racers = racers;
    frame.speed = 30;
    frame.rpm = 4000;
    frame.gear_index = 2;
    assert(hud.update(frame) == Result::ok);
    assert(renderer.frame.size() >= 9);
    assert(renderer.frame.front().positions[0][0] == 512);
    const auto& dot = renderer.frame[renderer.frame.size() - 3];
    assert(dot.positions[0][0] == 63 && dot.positions[0][1] == 400);
    racers[0].position = {100, 0, 0};
    assert(hud.update(frame) == Result::ok);
    const auto& moved = renderer.frame[renderer.frame.size() - 3];
    assert(moved.positions[0][1] == 395);
    frame.start_countdown = 9;
    assert(hud.update(frame) == Result::ok && renderer.frame.empty());
    frame.start_countdown = 3.5f;
    assert(hud.update(frame) == Result::ok && renderer.frame.size() == 1);
    const auto digit = renderer.frame.front();
    assert(digit.positions[2][0] - digit.positions[0][0] > 20);
    assert(std::abs(digit.colors[0][3] - 127.f / 255) < 1e-6f);
    assert(digit.positions[0][0] < 320 && digit.positions[2][0] > 320);
    frame.start_countdown = .5f;
    assert(hud.update(frame) == Result::ok && renderer.frame.size() > 1);
    frame.start_countdown.reset();
    frame.player_index = 1;
    assert(hud.update(frame) == Result::invalid_argument);
    frame.player_index = 0;
    assert(hud.update(frame) == Result::ok);
    const auto unlocked_frame = renderer.frame;
    frame.weapon_lock_warning = true;
    assert(hud.update(frame) == Result::ok);
    constexpr std::string_view warning = "WEAPONS LOCKED";
    constexpr auto warning_count = warning.size() - 1;
    assert(renderer.frame.size() == unlocked_frame.size() + warning_count);
    const auto& warning_left = renderer.frame.front();
    const auto& warning_right = renderer.frame[warning_count - 1];
    // Native004823f2 anchor300, y5, horizontal-only integer centering.
    assert(std::abs((warning_left.positions[0][0] + warning_right.positions[2][0]) / 2 - 300) <=
           .5f);
    for (std::size_t i = 0; i < warning_count; ++i) {
        assert(renderer.frame[i].positions[0][1] == 5);
        assert((renderer.frame[i].colors[0] == std::array<float, 4>{1, 0, 0, 102.f / 255}));
        assert((renderer.frame[i].colors[2] == std::array<float, 4>{1, 1, 0, 102.f / 255}));
    }
    hud.set_visible(false);
    assert(hud.update(frame) == Result::ok && renderer.frame.size() == warning_count);
    racers[0].finished = true;
    assert(hud.update(frame) == Result::ok && renderer.frame.size() == warning_count);
    racers[0].finished = false;
    racers[0].eliminated = true;
    assert(hud.update(frame) == Result::ok && renderer.frame.empty());
    racers[0].eliminated = false;
    hud.set_visible(true);
    frame.weapon_lock_warning = false;
    assert(hud.update(frame) == Result::ok && renderer.frame.size() == unlocked_frame.size());
    const std::array<std::string, 3> weapons{"Machine Gun", "Guided Missile", "Land Mines"};
    assert(hud.configure_equipment(resources, "Animal", weapons, error) == Result::ok);
    FrontendEquipmentFrame equipment;
    equipment.selected_item = 1;
    equipment.internal_hp = 25;
    equipment.maximum_internal_hp = 100;
    equipment.armor_hp = {100, 50, 0, -20};
    equipment.maximum_armor_hp = {100, 100, 100, 100};
    equipment.present = {true, true, true};
    equipment.ammunition = {120, 12, 5};
    frame.equipment = equipment;
    assert(hud.update(frame) == Result::ok);
    // Original Animal damage rect14,28..53,101, shiftedX31 and panelY352.
    const auto& internal = renderer.frame[renderer.frame.size() - 5];
    assert(internal.positions[0][0] == 45 && internal.positions[0][1] == 380);
    assert(internal.positions[2][0] == 84 && internal.positions[2][1] == 453);
    assert(internal.colors[0][0] == 1 && internal.colors[0][1] == 127.f / 255 &&
           internal.colors[0][2] == 0);
    const auto& rear = renderer.frame[renderer.frame.size() - 2];
    assert(rear.colors[0][0] == 1 && rear.colors[0][1] == 0);
    frame.equipment->selected_item = 3;
    frame.equipment->weapons_unlocked = false;
    assert(hud.update(frame) == Result::ok);
    const auto& icon = renderer.frame[renderer.frame.size() - 4];
    assert(icon.positions[0][0] == 0 && icon.positions[0][1] == 352 &&
           icon.colors[0][0] == 100.f / 255);
    assert(renderer.frame.back().colors[0][3] == 178.f / 255);
    frame.equipment->weapons_unlocked = true;
    assert(hud.update(frame) == Result::ok && renderer.frame.back().colors[0][0] == 1);
    frame.equipment->selected_item = 7;
    assert(hud.update(frame) == Result::invalid_argument);
    frame.equipment->selected_item = 3;
    racers[0].eliminated = true;
    assert(hud.update(frame) == Result::ok);
    assert(renderer.frame.front().positions[0][0] == 0 &&
           renderer.frame.front().positions[0][1] == 352);
    assert(renderer.frame.back().colors[0][0] == 229.f / 255 &&
           renderer.frame.back().colors[0][1] == 0 &&
           renderer.frame.back().colors[2][1] == 229.f / 255);
    racers[0].eliminated = false;
    racers[0].finished = true;
    assert(hud.update(frame) == Result::ok && renderer.frame.size() == 5);
    racers[0].finished = false;
    frame.equipment.reset();
    const std::array<AmmunitionNotice, 4> messages{{{"MACHINE GUN 99", 2},
                                                  {"GATLING GUN 9", 2},
                                                  {"LAND MINES 2", 2},
                                                  {"NITROUS BOOST 0", 2}}};
    frame.ammunition_notices = messages;
    frame.developer_build = false;
    assert(hud.update(frame) == Result::ok);
    const auto without_dev = renderer.frame;
    const auto& last = renderer.frame.back();
    const float message_height = last.positions[2][1] - last.positions[0][1];
    assert(last.positions[0][1] == 440 - 3 * (message_height + 5));
    assert(last.colors[0][3] == 178.f / 255 && last.colors[2][3] == 102.f / 255);
    frame.developer_build = true;
    assert(hud.update(frame) == Result::ok && renderer.frame.size() > without_dev.size());
    std::size_t dev_glyphs = 0;
    for (const auto& q : renderer.frame) {
        const bool present = std::any_of(without_dev.begin(), without_dev.end(), [&](const auto& p) {
            return q.positions == p.positions && q.uv == p.uv && q.texture.value == p.texture.value;
        });
        if (!present) {
            ++dev_glyphs;
            assert(q.positions[0][1] >= 440 + message_height + 5);
        }
    }
    assert(dev_glyphs > 0);
    for (const auto& message : messages) assert(message.remaining == 2);
    const std::array<AmmunitionNotice, 1> fading{{{"MACHINE GUN 0", .25f}}};
    frame.ammunition_notices = fading;
    assert(hud.update(frame) == Result::ok);
    for (const auto& color : renderer.frame.back().colors)
        assert(color[3] == 127.f / 255);
    frame.ammunition_notices = {};
    frame.developer_build = false;
    hud.set_visible(false);
    assert(hud.update(frame) == Result::ok && renderer.frame.empty());
    hud.start_fade_in();
    assert(hud.update(frame) == Result::ok && renderer.frame.size() == 1);
    assert(renderer.frame.back().positions[2] == (std::array<float, 2>{640, 480}));
    assert(renderer.frame.back().colors[0][3] == 1);
    hud.set_visible(true);
    frame.start_countdown = 10;
    assert(hud.update(frame) == Result::ok && renderer.frame.size() == 1);
    assert(renderer.frame.back().colors[0][3] == 1);
    frame.start_countdown.reset();
    hud.set_visible(false);
    hud.advance_fade(.25f);
    assert(hud.update(frame) == Result::ok && renderer.frame.back().colors[0][3] == .5f);
    hud.advance_fade(.25f);
    assert(hud.update(frame) == Result::ok && renderer.frame.empty());
    hud.stop();
    assert(renderer.textures.empty());
    assert(hud.load(resources, "Missing Track", {}, characters, error) != Result::ok);
    assert(renderer.textures.empty());
    assert(hud.load(resources, "Dark Forest", {}, characters, error) == Result::ok);
    assert(renderer.textures.size() == texture_count);
    hud.stop();
    assert(renderer.textures.empty());
    std::cout << "ROADWARS_FRONTEND_CORE_OK: original assets, font, map motion and lifecycle\n";
}
