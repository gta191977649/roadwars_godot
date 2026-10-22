#include "game_src/unresolved/menu_frontend.hpp"
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>

using namespace roadwars;
namespace {
class TestRenderer final : public Renderer {
  public:
    std::uint64_t next{1};
    std::size_t textures{};
    std::vector<HudQuad> frame;
    MeshHandle upload_mesh(std::span<const Vec3>, std::span<const std::uint32_t>) override {
        return {};
    }
    InstanceHandle create_instance(MeshHandle) override { return {}; }
    Result update_instance(InstanceHandle, const Transform&) override { return Result::ok; }
    void destroy_instance(InstanceHandle) override {}
    void destroy_mesh(MeshHandle) override {}
    void update_camera(const Transform&, float) override {}
    TextureHandle upload_texture(std::uint32_t, std::uint32_t,
                                 std::span<const std::byte>) override {
        ++textures;
        return {next++};
    }
    void destroy_texture(TextureHandle) override {
        assert(textures);
        --textures;
    }
    Result draw_hud(std::span<const HudQuad> q) override {
        frame.assign(q.begin(), q.end());
        return Result::ok;
    }
};
std::vector<std::byte> bmp() {
    constexpr int width = 512, height = 2, stride = 1536;
    std::vector<std::byte> b(54 + stride * height);
    const auto put = [&](int at, unsigned value, int n) {
        for (int i = 0; i < n; ++i)
            b[at + i] = static_cast<std::byte>(value >> (i * 8));
    };
    b[0] = std::byte{'B'};
    b[1] = std::byte{'M'};
    put(2, b.size(), 4);
    put(10, 54, 4);
    put(14, 40, 4);
    put(18, width, 4);
    put(22, height, 4);
    put(26, 1, 2);
    put(28, 24, 2);
    for (int i = 0; i < 200; ++i)
        for (int c = 0; c < 3; ++c)
            b[54 + stride + i * 6 + c] = std::byte{255};
    return b;
}
class MemoryReader final : public ResourceReader {
  public:
    std::map<std::string, std::string> overrides;
    Result read(std::string_view path, std::vector<std::byte>& output) override {
        if (const auto found = overrides.find(std::string(path)); found != overrides.end()) {
            output.clear();
            for (unsigned char c : found->second)
                output.push_back(static_cast<std::byte>(c));
            return Result::ok;
        }
        if (path.ends_with(".pse"))
            return Result::not_found;
        if (path.ends_with(".bmp")) {
            output = bmp();
            return Result::ok;
        }
        std::string text;
        if (path == "Ini/RoadWars.ini")
            text = "[ROADWARS]\nLANGUAGE=0\n";
        else if (path == "Screens/SelectGameOption/Data.ini")
            text = config(
                6, "SELECT GAME", false,
                {"QUICK RACE", "SINGLE RACE", "SEASON RACE", "MULTIPLAYER", "OPTIONS", "EXIT"});
        else if (path == "Screens/Select Option/Data.ini")
            text = config(4, "SELECT OPTION", true, {"GRAPHICS", "SOUND", "GAME", "CONTROLLERS"});
        else if (path == "Screens/Intro/Data.ini")
            text = timed_config(3);
        else if (path == "Screens/Title/Data.ini")
            text = timed_config(7);
        else
            return Result::not_found;
        output.clear();
        for (unsigned char c : text)
            output.push_back(static_cast<std::byte>(c));
        return Result::ok;
    }
    static std::string config(int n, std::string title, bool back,
                              std::vector<std::string> values) {
        auto s = "[INFO]\nNUMSELECTIONS=" + std::to_string(n) +
                 "\nDOFADEIN=1\nTIMESOUT=0\nTIMEOUTSECS=0\nGOBACKENABLED=" + (back ? "1" : "0") +
                 "\nTITLE=" + title + "\nFADESECS=0.3\n";
        for (int i = 0; i < n; ++i)
            s += "[SELECTION" + std::to_string(i) + "]\nVALUE=" + values[i] +
                 "\nISAVAILABLE=1\nDOFADE=1\n";
        return s;
    }
    static std::string timed_config(double duration) {
        return "[INFO]\nNUMSELECTIONS=0\nDOFADEIN=1\nTIMESOUT=1\nTIMEOUTSECS=" +
               std::to_string(duration) + "\nGOBACKENABLED=0\nTITLE=NONE\nFADESECS=0.3\n";
    }
};
class DirectoryReader final : public ResourceReader {
  public:
    std::filesystem::path root;
    explicit DirectoryReader(std::filesystem::path path) : root(std::move(path)) {}
    Result read(std::string_view path, std::vector<std::byte>& out) override {
        std::ifstream file(root / std::filesystem::path(path), std::ios::binary | std::ios::ate);
        if (!file)
            return Result::not_found;
        auto size = file.tellg();
        if (size < 0)
            return Result::invalid_argument;
        out.resize(static_cast<std::size_t>(size));
        file.seekg(0);
        file.read(reinterpret_cast<char*>(out.data()), size);
        return file ? Result::ok : Result::invalid_argument;
    }
};
void advance_checked(MenuFrontend& menu, double elapsed, const FrontendInput& input = {}) {
    std::string error;
    const auto result = menu.advance(elapsed, input, error);
    if (result != Result::ok)
        std::cerr << error << '\n';
    assert(result == Result::ok);
}
void startup_timing_tests() {
    MemoryReader reader;
    ResourceStore resources(reader);
    TestRenderer one_renderer, many_renderer;
    MenuFrontend one(resources, one_renderer), many(resources, many_renderer);
    std::string error;
    assert(one.start(error, "intro") == Result::ok);
    assert(many.start(error, "intro") == Result::ok);
    advance_checked(one, 11.25);
    for (int i = 0; i < 1350; ++i)
        advance_checked(many, 1.0 / 120);
    assert(one.screen() == "main_menu" && many.screen() == one.screen());
    assert(std::abs(one.screen_time() - .65) < 1e-6);
    assert(std::abs(one.screen_time() - many.screen_time()) < 1e-8);
    // A page boundary with zero remaining time must not lose the pending fade.
    assert(one.start(error, "intro") == Result::ok);
    advance_checked(one, 3);
    assert(one.screen() == "intro");
    advance_checked(one, .3f);
    assert(one.screen() == "title" && one.screen_time() < 1e-8);
    advance_checked(one, 7 + static_cast<double>(.3f));
    assert(one.screen() == "main_menu");
    // Preview freezes automatic routing while continuing the requested page's clock.
    assert(one.preview("intro", 1, "", error) == Result::ok);
    advance_checked(one, 20);
    assert(one.screen() == "intro" && one.screen_time() == 21);
    // Held skip cannot immediately dismiss the next page.
    assert(one.start(error, "intro") == Result::ok);
    FrontendInput skip;
    skip.accept = skip.any_key = true;
    advance_checked(one, .3f, skip);
    assert(one.screen() == "title");
    advance_checked(one, .1, skip);
    assert(one.screen() == "title");
    advance_checked(one, 0);
    advance_checked(one, .3f, skip);
    assert(one.screen() == "main_menu");
}
void input_animation_tests() {
    MemoryReader reader;
    ResourceStore resources(reader);
    TestRenderer one_renderer, many_renderer;
    MenuFrontend one(resources, one_renderer), many(resources, many_renderer);
    std::string error;
    assert(one.start(error, "main_menu") == Result::ok);
    assert(many.start(error, "main_menu") == Result::ok);
    FrontendInput right;
    right.right = true;
    advance_checked(one, 0, right);
    advance_checked(many, 0, right);
    advance_checked(one, .0625, right);
    for (int i = 0; i < 8; ++i)
        advance_checked(many, .0625 / 8, right);
    assert(one.selected() == 1 && many.selected() == 1);
    assert(one.selections()[1].radius == 75 && many.selections()[1].radius == 75);
    assert(one.selections()[0].radius == many.selections()[0].radius);
    // A long hold updates animation but cannot repeatedly advance focus.
    advance_checked(one, 5, right);
    assert(one.selected() == 1 && one.selections()[1].radius == 100);
    advance_checked(one, 0);
    advance_checked(one, 0, right);
    assert(one.selected() == 2);
    // Grid boundaries do not wrap across rows.
    advance_checked(one, 0);
    advance_checked(one, 0, right);
    assert(one.selected() == 2);
    FrontendInput down;
    down.down = true;
    advance_checked(one, 0, down);
    assert(one.selected() == 5);
    FrontendInput left;
    left.left = true;
    advance_checked(one, 0, left);
    assert(one.selected() == 4);
    FrontendInput accept;
    accept.accept = true;
    advance_checked(one, .31, accept);
    assert(one.screen() == "select_option");
    advance_checked(one, .31, accept);
    assert(one.screen() == "select_option");
    // Disabled icons remain focusable, but activation reports an unavailable selection.
    auto disabled = MemoryReader::config(2, "SELECT GAME", false, {"OPTIONS", "EXIT"});
    const auto at = disabled.find("ISAVAILABLE=1");
    assert(at != std::string::npos);
    disabled.replace(at, std::string("ISAVAILABLE=1").size(), "ISAVAILABLE=0");
    reader.overrides["Screens/SelectGameOption/Data.ini"] = disabled;
    assert(one.start(error, "main_menu") == Result::ok);
    assert(one.selected() == 1); // Initial focus is the first available icon.
    advance_checked(one, 0, left);
    assert(one.selected() == 0);
    advance_checked(one, 0, accept);
    assert(one.screen() == "main_menu" && one.action() == "selection_unavailable:OPTIONS");
    assert(!one.quit_requested());
}
void malformed_config_tests() {
    const std::string path = "Screens/SelectGameOption/Data.ini";
    const auto valid = MemoryReader::config(2, "SELECT GAME", false, {"OPTIONS", "EXIT"});
    for (const auto& [key, replacement] : std::array<std::pair<std::string, std::string>, 8>{
             {{"NUMSELECTIONS=2", "NUMSELECTIONS=-1"},
              {"NUMSELECTIONS=2", "NUMSELECTIONS=2.5"},
              {"NUMSELECTIONS=2", "NUMSELECTIONS=65"},
              {"FADESECS=0.3", "FADESECS=-0.1"},
              {"FADESECS=0.3", "FADESECS=nan"},
              {"TIMEOUTSECS=0", "TIMEOUTSECS=3601"},
              {"VALUE=OPTIONS", "VALUE="},
              {"ISAVAILABLE=1", "UNRELATED=1"}}}) {
        MemoryReader reader;
        auto malformed = valid;
        const auto at = malformed.find(key);
        assert(at != std::string::npos);
        malformed.replace(at, key.size(), replacement);
        reader.overrides[path] = malformed;
        ResourceStore resources(reader);
        TestRenderer renderer;
        {
            MenuFrontend menu(resources, renderer);
            std::string error;
            assert(menu.start(error, "main_menu") == Result::invalid_argument);
            assert(!error.empty());
        }
        assert(renderer.textures == 0);
    }
    // A present malformed encoded config must not select a valid plaintext fallback.
    MemoryReader reader;
    auto encoded = valid;
    encoded.replace(encoded.find("NUMSELECTIONS=2"), 15, "NUMSELECTIONS=-1");
    for (char& c : encoded)
        c = static_cast<char>((static_cast<unsigned char>(c) + 1) & 255);
    reader.overrides["Screens/SelectGameOption/Data.pse"] = encoded;
    reader.overrides[path] = valid;
    ResourceStore resources(reader);
    TestRenderer renderer;
    MenuFrontend menu(resources, renderer);
    std::string error;
    assert(menu.start(error, "main_menu") == Result::invalid_argument);
}
void restart_lifetime_test() {
    MemoryReader reader;
    ResourceStore resources(reader);
    TestRenderer renderer;
    {
        MenuFrontend menu(resources, renderer);
        std::string error;
        assert(menu.start(error, "main_menu") == Result::ok);
        const auto initial_textures = renderer.textures;
        assert(menu.start(error, "main_menu") == Result::ok);
        assert(renderer.textures == initial_textures);
    }
    assert(renderer.textures == 0 && renderer.frame.empty());
}
} // namespace
int main(int argc, char** argv) {
    startup_timing_tests();
    input_animation_tests();
    malformed_config_tests();
    restart_lifetime_test();
    assert((MenuFrontend::icon_center(6, 5) == std::array<int, 2>{530, 348}));
    assert((MenuFrontend::icon_center(4, 2) == std::array<int, 2>{160, 340}));
    MemoryReader reader;
    ResourceStore resources(reader);
    TestRenderer renderer;
    {
        MenuFrontend menu(resources, renderer);
        std::string error;
        assert(menu.start(error, "main_menu") == Result::ok);
        assert(menu.selections().size() == 6 && menu.selected() == 0);
        FrontendInput right;
        right.right = true;
        assert(menu.advance(.0625, right, error) == Result::ok);
        assert(menu.selected() == 1 && menu.selections()[1].radius == 75);
        assert(menu.advance(.0625, right, error) == Result::ok);
        assert(menu.selected() == 1 && menu.selections()[1].radius == 100);
        menu.advance(0, {}, error);
        FrontendInput down;
        down.down = true;
        menu.advance(0, down, error);
        assert(menu.selected() == 4);
        menu.advance(0, {}, error);
        FrontendInput accept;
        accept.accept = true;
        assert(menu.advance(.31, accept, error) == Result::ok && menu.screen() == "select_option");
        menu.advance(0, {}, error);
        FrontendInput back;
        back.back = true;
        assert(menu.advance(.31, back, error) == Result::ok && menu.screen() == "main_menu");
        assert(menu.advance(std::numeric_limits<double>::quiet_NaN(), {}, error) ==
               Result::invalid_argument);
        assert(menu.preview("unknown", 0, "", error) == Result::not_found);
        assert(menu.preview("main_menu", 1, "", error) == Result::ok);
    }
    assert(renderer.textures == 0 && renderer.frame.empty());
    if (argc > 1) {
        DirectoryReader original(argv[1]);
        ResourceStore store(original);
        assert(store.open() == Result::ok);
        MenuFrontend menu(store, renderer);
        std::string error;
        if (menu.start(error) != Result::ok) {
            std::cerr << error << '\n';
            return 1;
        }
        assert(menu.advance(31, {}, error) == Result::ok);
        assert(menu.screen() == "main_menu");
        {
            TestRenderer split_renderer;
            MenuFrontend split(store, split_renderer);
            assert(split.start(error) == Result::ok);
            for (int frame = 0; frame < 1860; ++frame)
                advance_checked(split, 1.0 / 60);
            assert(split.screen() == menu.screen());
            assert(std::abs(split.screen_time() - menu.screen_time()) < 1e-6);
        }
        for (const auto& info : MenuFrontend::screens()) {
            if (info.custom)
                continue;
            if (menu.preview(info.id, 1, "", error) != Result::ok) {
                std::cerr << info.id << ": " << error << '\n';
                return 1;
            }
        }
        const auto press = [&](bool accept, bool right, bool down, bool back) {
            assert(menu.advance(0, {}, error) == Result::ok);
            FrontendInput input;
            input.accept = accept;
            input.right = right;
            input.down = down;
            input.back = back;
            assert(menu.advance(.31, input, error) == Result::ok);
        };
        assert(menu.start(error, "main_menu") == Result::ok);
        press(false, true, false, false);
        press(false, true, false, false); // SEASON RACE
        press(true, false, false, false);
        assert(menu.screen() == "main_menu" && menu.assertion_message() == "SEASON RACE");
        const auto frozen_time = menu.screen_time();
        press(false, true, false, false);
        assert(menu.selected() == 2 && menu.screen_time() == frozen_time);
        press(false, false, false, true);
        assert(menu.assertion_message().empty() && menu.screen() == "main_menu");
        // Multiplayer uses the same modal, including pointer dismissal.
        press(false, false, false, false);
        FrontendInput left;
        left.left = true;
        menu.advance(.31, left, error);
        menu.advance(0, {}, error);
        menu.advance(.31, left, error);
        menu.advance(0, {}, error);
        press(false, false, true, false);
        press(true, false, false, false);
        assert(menu.assertion_message() == "MULTIPLAYER");
        menu.advance(0, {}, error);
        FrontendInput close;
        close.pointer_accept = true;
        close.pointer_x = 320;
        close.pointer_y = 315;
        assert(menu.advance(0, close, error) == Result::ok && menu.assertion_message().empty());
        assert(menu.start(error, "main_menu") == Result::ok);
        assert(menu.menu_music_requested());
        press(false, true, false, false); // SINGLE RACE
        press(true, false, false, false);
        assert(menu.screen() == "select_character");
        press(true, false, false, false);
        assert(menu.screen() == "select_track");
        press(false, false, true, false); // Dark Forest
        press(true, false, false, false);
        assert(menu.screen() == "start_race_summary" && !menu.race_requested());
        assert(menu.race_request().mode == FrontendRaceMode::single);
        assert(menu.race_request().player_loadout &&
               menu.race_request().player_loadout->weapons[0].empty());
        assert(menu.race_request().laps == 2);
        press(false, false, false, true);
        assert(menu.screen() == "select_track");
        press(true, false, false, false);
        press(false, true, false, false);
        press(true, false, false, false);
        assert(menu.screen() == "loading" && menu.race_requested() && !menu.menu_music_requested());
        assert(menu.music_path() ==
               "Characters/" + std::string(menu.selected_character()) + "/ThemeMusic.wav");
        assert(menu.selected_character() == menu.race_request().character);
        press(false, false, false, true);
        assert(menu.screen() == "loading" && menu.race_requested());
        assert(menu.start(error, "main_menu") == Result::ok);
        press(true, false, false, false); // QUICK RACE
        press(true, false, false, false);
        press(false, false, true, false);
        press(true, false, false, false);
        assert(menu.screen() == "loading" && menu.race_requested());
        assert(menu.race_request().mode == FrontendRaceMode::quick &&
               !menu.race_request().player_loadout);
        assert(menu.show_pause({}, error) == Result::ok && menu.screen() == "pause");
        FrontendInput accept;
        accept.accept = true;
        assert(menu.advance(0, accept, error) == Result::ok &&
               menu.action() == "race_pause:CONTINUE");
        menu.advance(0, {}, error);
        FrontendInput down;
        down.down = true;
        menu.advance(0, down, error);
        menu.advance(0, {}, error);
        assert(menu.advance(0, accept, error) == Result::ok &&
               menu.action() == "race_pause:RESTART RACE");
        menu.advance(0, {}, error);
        menu.advance(0, down, error);
        menu.advance(0, {}, error);
        assert(menu.advance(0, accept, error) == Result::ok &&
               menu.action() == "unavailable_business_action:REPLAY");
        std::cout << "Original-asset frontend startup and generic screen coverage passed\n";
    }
    std::cout << "Frontend navigation, timing and lifetime tests passed\n";
}
