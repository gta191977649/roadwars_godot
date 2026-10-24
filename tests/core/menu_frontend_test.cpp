#include "game_src/unresolved/menu_frontend.hpp"
#include "game_src/unresolved/title_attract.hpp"
#include <algorithm>
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
    std::size_t materials{}, meshes{}, instances{};
    std::vector<HudQuad> frame;
    MeshHandle upload_mesh(std::span<const Vec3>, std::span<const std::uint32_t>) override {
        return {};
    }
    MeshHandle upload_surface(std::span<const Vec3>, std::span<const std::array<float, 2>>,
                              std::span<const std::uint32_t>, MaterialHandle, std::span<const Vec3>,
                              std::span<const std::array<float, 4>>) override {
        ++meshes;
        return {next++};
    }
    MaterialHandle create_material(TextureHandle, const MaterialSettings&) override {
        ++materials;
        return {next++};
    }
    void destroy_material(MaterialHandle) override {
        assert(materials);
        --materials;
    }
    InstanceHandle create_instance(MeshHandle mesh) override {
        assert(mesh.value);
        ++instances;
        return {next++};
    }
    Result update_instance(InstanceHandle, const Transform&) override { return Result::ok; }
    void destroy_instance(InstanceHandle) override {
        assert(instances);
        --instances;
    }
    void destroy_mesh(MeshHandle) override {
        assert(meshes);
        --meshes;
    }
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
        else if (path == "Screens/SelectCharacter/Data.ini")
            text = config(6, "CHARACTER", true,
                          {"Animal", "Alpha", "Big Cat", "Hammer", "Scout", "Shark"});
        else if (path == "Screens/SelectTrack/Data.ini")
            text = config(2, "TRACK", true, {"Dark Forest", "Badlands"});
        else if (path.ends_with("/World/Data.txt"))
            text = "[DATA]\nNUMLAPS=3\n";
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
void sound_options_tests() {
    struct SoundHost final : SoundOptionsHost {
        SoundOptions saved;
        bool fail{};
        int saves{};
        Result save_sound_options(const SoundOptions& value, std::string& error) override {
            if (fail) {
                error = "SAVE FAILED";
                return Result::unavailable;
            }
            saved = value;
            ++saves;
            return Result::ok;
        }
    } host;
    MemoryReader reader;
    reader.overrides["Ini/RoadWars.ini"] =
        "[ROADWARS]\nLANGUAGE=0\nMUSIC=1\nSFX=.75\nINGAMEMUSIC=.5\n"
        "PLAYMUSICFROMCD=0\nUSE3DSOUNDHARDWARE=1\nSOUNDCHANNELS=20\n";
    ResourceStore resources(reader);
    TestRenderer renderer;
    MenuFrontend menu(resources, renderer);
    menu.set_sound_options_host(&host);
    std::string error;
    assert(menu.start(error, "sound") == Result::ok);
    const auto press = [&](FrontendInput input) {
        advance_checked(menu, 0, {});
        advance_checked(menu, .06, input);
    };
    FrontendInput down, right, left, back;
    down.down = right.right = left.left = back.back = true;
    assert(menu.option_rows().size() == 6 && !menu.option_rows()[4].available &&
           !menu.option_rows()[5].available);
    press(down);
    press(right); // Native wrap 100 -> 0; music previews immediately.
    assert(menu.music_volume() == 0 && menu.sfx_gain() == .75f);
    advance_checked(menu, 1, right);
    assert(menu.music_volume() == 0); // Held key does not repeat.
    press(left);
    assert(menu.music_volume() == 1);
    press(down);
    press(right);
    assert(std::abs(menu.sfx_gain() - .75f) < 1e-6 && host.saves == 0);
    host.fail = true;
    press(back);
    assert(menu.screen() == "sound" && host.saves == 0 && menu.sfx_gain() == .75f);
    host.fail = false;
    press(back);
    assert(host.saves == 1 && std::abs(host.saved.effects - .8f) < 1e-6);
    assert(host.saved.hardware_3d && host.saved.channels == 20);
    assert(std::abs(menu.sfx_gain() - .8f) < 1e-6);
    SoundOptions loaded;
    assert(load_sound_options(resources, loaded) == Result::ok &&
           std::abs(loaded.effects - host.saved.effects) < 1e-6);
    MenuFrontend reopened(resources, renderer);
    assert(reopened.start(error, "sound") == Result::ok);
    assert(reopened.option_rows()[2].selected == 16);
    FrontendInput pointer;
    pointer.pointer_accept = true;
    pointer.pointer_x = 615;
    pointer.pointer_y = 240; // Row 3 starts at the vertical center for six equal rows.
    advance_checked(reopened, .06, pointer);
    assert(reopened.selected() == 3 && reopened.option_rows()[3].selected == 11);
    loaded.music = std::numeric_limits<float>::quiet_NaN();
    assert(override_sound_options(resources, loaded) == Result::invalid_argument);
    assert(load_sound_options(resources, loaded) == Result::ok &&
           std::abs(loaded.effects - host.saved.effects) < 1e-6);
    loaded.channels = 9;
    assert(validate_sound_options(loaded) == Result::invalid_argument);
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
    assert(one.screen() == "loading" && many.screen() == one.screen());
    assert(one.race_requested() && one.race_request().mode == FrontendRaceMode::attract);
    assert(one.race_request().character == many.race_request().character &&
           one.race_request().track == many.race_request().track);
    assert(std::abs(one.screen_time() - .65) < 1e-6);
    assert(std::abs(one.screen_time() - many.screen_time()) < 1e-8);
    // Even a stalled host must present black before the attract loading intro.
    assert(one.commands().back().colors[0][3] == 1);
    assert(!one.loading_intro_complete());
    one.release_loading_black();
    advance_checked(one, .3f);
    assert(one.loading_intro_complete());
    // A page boundary with zero remaining time must not lose the pending fade.
    assert(one.start(error, "intro") == Result::ok);
    advance_checked(one, 3);
    assert(one.screen() == "intro");
    advance_checked(one, .3f);
    assert(one.screen() == "title" && one.screen_time() < 1e-8);
    advance_checked(one, 7 + static_cast<double>(.3f));
    assert(one.screen() == "loading" && one.race_requested());
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
    // Attract simulation owns the45-second deadline and the requested exit fade.
    TitleAttract demo;
    demo.start();
    assert(!demo.advance(5));
    assert(demo.advance(1.0 / 60));
    assert(!demo.exiting() && demo.exit_opacity() == 0);
    demo.dismiss();
    assert(!demo.advance(.25) && demo.exit_opacity() == .5f && !demo.black());
    demo.advance(.25);
    assert(demo.black() && demo.exit_opacity() == 1);
    demo.start();
    for (int i = 0; i < 2700; ++i)
        demo.advance(1.0 / 60);
    assert(demo.exiting() && !demo.black());
    demo.advance(.5);
    assert(demo.black());
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
class GraphicsHost final : public GraphicsSettingsHost {
  public:
    GraphicsSettings live{}, saved{};
    int saves{}, restarts{};
    GraphicsCapabilities capabilities() const override {
        return {
            {{"TEST DISPLAY", 1920, 1080}},
            {RenderingBackend::automatic, RenderingBackend::vulkan, RenderingBackend::opengl},
            {AntiAliasing::off, AntiAliasing::fxaa},
            true,
            true,
            true};
    }
    GraphicsSettings current_settings() const override { return live; }
    Result apply_live(const GraphicsSettings& value) override {
        live = value;
        return Result::ok;
    }
    Result persist(const GraphicsSettings& value) override {
        saved = value;
        ++saves;
        return Result::ok;
    }
    RenderingBackend active_backend() const override { return RenderingBackend::vulkan; }
    bool restart_required() const override { return saved.backend == RenderingBackend::opengl; }
    Result restart() override {
        ++restarts;
        return Result::ok;
    }
};
void graphics_menu_tests() {
    MemoryReader reader;
    ResourceStore resources(reader);
    TestRenderer renderer;
    GraphicsHost host;
    MenuFrontend menu(resources, renderer, &host);
    std::string error;
    assert(menu.start(error, "graphics") == Result::ok);
    assert(menu.option_rows()[0].label == "DISPLAY" && menu.option_rows()[1].label == "RESOLUTION");
    host.live.width = 1100;
    host.live.height = 700;
    advance_checked(menu, 0);
    assert(menu.option_rows()[1].values[0] == "1100 X 700");
    const auto press = [&](std::string_view id, bool right = false) {
        const auto rows = menu.option_rows();
        const auto it =
            std::find_if(rows.begin(), rows.end(), [&](const auto& row) { return row.id == id; });
        assert(it != rows.end());
        const int target = static_cast<int>(it - rows.begin());
        while (menu.selected() != target) {
            FrontendInput move;
            move.down = menu.selected() < target;
            move.up = !move.down;
            advance_checked(menu, 0, move);
            advance_checked(menu, 0);
        }
        FrontendInput input;
        input.right = right;
        input.accept = !right;
        advance_checked(menu, 0, input);
        advance_checked(menu, 0);
    };
    press("window_mode", true);
    press("vsync", true);
    assert(host.live.vsync && host.saves == 0); // Draft edits do not change the host.
    press("apply");
    assert(!host.live.vsync && host.saves == 0);
    assert(menu.option_rows().front().id == "confirm");
    advance_checked(menu, 16);
    assert(host.live.vsync && host.saves == 0);
    press("window_mode", true);
    press("vsync", true);
    press("apply");
    press("confirm");
    assert(!host.saved.vsync && host.saves == 1);
    press("page", true);
    press("chrome_mapping", true);
    assert(host.saved.chrome_mapping);
    press("apply");
    assert(!host.saved.chrome_mapping && host.saves == 2);
    press("page", true);
    press("backend", true);
    press("backend", true);
    press("apply");
    assert(host.saved.backend == RenderingBackend::opengl);
    press("restart");
    assert(host.restarts == 1 && menu.quit_requested());
    press("defaults");
    press("back");
    assert(menu.option_rows().front().id == "monitor"); // Modal retains the options underneath.
    FrontendInput escape;
    escape.back = true;
    advance_checked(menu, 0, escape); // Cancel the question and keep editing.
    advance_checked(menu, 0);
    press("back");
    FrontendInput no;
    no.accept = true; // Default NO discards draft changes without saving.
    const auto saves = host.saves;
    advance_checked(menu, 0, no);
    assert(host.saves == saves);
    assert(menu.screen() == "main_menu");
    assert(menu.start(error, "graphics") == Result::ok);
    press("page", true);
    press("chrome_mapping", true);
    const auto candidate = !host.saved.chrome_mapping;
    press("back");
    FrontendInput yes;
    yes.left = true;
    advance_checked(menu, 0, yes);
    advance_checked(menu, 0);
    yes = {};
    yes.accept = true;
    advance_checked(menu, 0, yes);
    assert(menu.screen() == "main_menu" && host.saved.chrome_mapping == candidate);
    MenuFrontend unavailable(resources, renderer);
    assert(unavailable.start(error, "graphics") == Result::ok);
    assert(unavailable.option_rows().front().id == "unavailable");
    assert(!unavailable.option_rows().front().available);
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
    sound_options_tests();
    startup_timing_tests();
    input_animation_tests();
    malformed_config_tests();
    restart_lifetime_test();
    graphics_menu_tests();
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
        assert(menu.screen() == "loading" && menu.race_requested() &&
               menu.race_request().mode == FrontendRaceMode::attract);
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
        press(true, false, false, false); // Garage loading.
        for (int frame = 0; menu.screen() == "loading" && frame < 30; ++frame)
            advance_checked(menu, .1);
        assert(menu.screen() == "modify_vehicle" && menu.garage_session());
        assert(menu.garage_session()->state().cash == 25000);
        press(false, false, true, false);
        press(false, false, true, false);
        press(true, false, false, false); // Weapons.
        assert(menu.option_rows().size() == 3);
        press(false, false, true, false);
        press(false, false, true, false);
        press(true, false, false, false);
        for (int confirmation = 0; menu.garage_confirmation_pending() && confirmation < 3;
             ++confirmation) {
            advance_checked(menu, 0);
            FrontendInput yes;
            yes.left = true;
            advance_checked(menu, 0, yes);
            press(true, false, false, false);
        }
        assert(menu.garage_session()->state().cash == 15000 &&
               ResourceStore::canonical(menu.garage_session()->state().loadout.weapons[0]) ==
                   "MACHINE GUN");
        assert(menu.race_request().player_loadout &&
               ResourceStore::canonical(menu.race_request().player_loadout->weapons[0]) ==
                   "MACHINE GUN");
        press(true, false, false, false); // Offer sell-back.
        assert(menu.garage_confirmation_pending());
        press(false, false, false, true); // Native confirmation ignores Escape.
        assert(menu.garage_confirmation_pending());
        press(true, false, false, false); // Default No declines.
        assert(!menu.garage_confirmation_pending() && menu.garage_session()->state().cash == 15000);
        // Rejection uses native 004b090a: Escape/outside clicks do not dismiss it.
        press(false, false, false, false);
        FrontendInput choose_quality;
        choose_quality.up = true;
        advance_checked(menu, 0, choose_quality);
        for (int i = 0; i < 7; ++i) {
            advance_checked(menu, 0);
            FrontendInput next;
            next.right = true;
            advance_checked(menu, 0, next);
        }
        press(false, false, true, false);
        press(true, false, false, false);
        assert(menu.garage_notice_pending() && !menu.garage_confirmation_pending());
        assert(menu.garage_session()->state().cash == 15000);
        const auto notice_time = menu.screen_time();
        const std::vector<HudQuad> notice_commands(menu.commands().begin(), menu.commands().end());
        press(false, false, false, true);
        assert(menu.garage_notice_pending());
        advance_checked(menu, 0);
        FrontendInput outside;
        outside.pointer_accept = true;
        outside.pointer_x = outside.pointer_y = 0;
        advance_checked(menu, 1, outside);
        assert(menu.garage_notice_pending() && menu.screen_time() == notice_time);
        assert(menu.commands().size() == notice_commands.size());
        for (std::size_t i = 0; i < notice_commands.size(); ++i) {
            assert(menu.commands()[i].positions == notice_commands[i].positions);
            assert(menu.commands()[i].colors == notice_commands[i].colors);
        }
        press(true, false, false, false);
        assert(!menu.garage_notice_pending() && menu.garage_session()->state().cash == 15000);
        advance_checked(menu, 0);
        advance_checked(menu, 0, choose_quality);
        for (int i = 0; i < 7; ++i) {
            advance_checked(menu, 0);
            FrontendInput previous;
            previous.left = true;
            advance_checked(menu, 0, previous);
        }
        press(false, false, true, false);
        // Native store input remains live while doors close. Inputs below consume no
        // presentation time, so every assertion precedes the threshold swap.
        const auto rapid = [&](FrontendInput input) {
            advance_checked(menu, 0);
            advance_checked(menu, 0, input);
        };
        FrontendInput up, down_store, right_store, left_store, accept_store, back_store;
        up.up = true;
        down_store.down = true;
        right_store.right = true;
        left_store.left = true;
        accept_store.accept = true;
        back_store.back = true;
        rapid(up);
        rapid(up); // Item row.
        const auto old_item = menu.garage_selection();
        rapid(right_store);
        rapid(right_store); // Repeated right does not queue two item increments.
        rapid(left_store);  // Latest direction wins, relative to the still-current Front.
        assert(menu.garage_selection().item == old_item.item);
        rapid(down_store);
        rapid(right_store); // Quality changes the old Front while doors keep closing.
        assert(menu.garage_selection().item == old_item.item &&
               menu.garage_selection().product != old_item.product);
        rapid(left_store);
        assert(menu.garage_selection().product == old_item.product);
        rapid(down_store);
        rapid(accept_store); // Action on current Front still offers Machine Gun sell-back.
        assert(menu.garage_confirmation_pending());
        const auto modal_time = menu.screen_time();
        advance_checked(menu, 1);
        assert(menu.screen_time() == modal_time && menu.garage_selection().item == old_item.item);
        rapid(accept_store); // Default No declines; resume the same closing doors.
        advance_checked(menu, 1);
        assert(menu.garage_selection().item == 2 && menu.garage_session()->state().cash == 15000);
        rapid(up);
        rapid(up);
        rapid(right_store); // Start Rear -> Front, then abandon it before the threshold.
        rapid(back_store);
        assert(menu.screen() == "modify_vehicle");
        rapid(down_store);
        rapid(down_store);
        rapid(accept_store); // Reenter Weapons with the installed Front selected.
        assert(menu.garage_selection().item == 0 &&
               menu.garage_selection().product == old_item.product);
        advance_checked(menu, 1);
        assert(menu.garage_selection().item == 0); // Abandoned transition cannot arrive later.
        press(false, false, false, true);          // Garage.
        press(false, false, false, true);          // Summary.
        assert(menu.screen() == "start_race_summary");
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
               menu.action() == "race_pause:REPLAY");
        assert(menu.show_pause({}, error, false, false) == Result::ok);
        for (const auto& row : menu.option_rows())
            assert(row.label != "REPLAY");
        assert(menu.show_pause({}, error, true, false) == Result::ok);
        FrontendInput cancel_terminal;
        cancel_terminal.back = true;
        assert(menu.advance(0, cancel_terminal, error) == Result::ok &&
               menu.action() == "race_pause:CANCEL");
        std::cout << "Original-asset frontend startup and generic screen coverage passed\n";
    }
    std::cout << "Frontend navigation, timing and lifetime tests passed\n";
}
