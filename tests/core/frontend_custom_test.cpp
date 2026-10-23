#include "game_src/unresolved/frontend_custom.hpp"
#include "game_src/unresolved/frontend_input_names.hpp"
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>

using namespace roadwars;
namespace {
class Files final : public ResourceReader {
  public:
    explicit Files(std::filesystem::path root) : root_(std::move(root)) {}
    Result read(std::string_view name, std::vector<std::byte>& output) override {
        std::ifstream stream(root_ / std::filesystem::path(name), std::ios::binary | std::ios::ate);
        if (!stream)
            return Result::not_found;
        const auto size = stream.tellg();
        if (size < 0)
            return Result::invalid_argument;
        output.resize(static_cast<std::size_t>(size));
        stream.seekg(0);
        stream.read(reinterpret_cast<char*>(output.data()), size);
        return stream ? Result::ok : Result::invalid_argument;
    }

  private:
    std::filesystem::path root_;
};
class CanvasRenderer final : public Renderer {
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
                                 std::span<const std::byte> data) override {
        assert(data.size() == static_cast<std::size_t>(width) * height * 4);
        textures.insert(next);
        return {next++};
    }
    void destroy_texture(TextureHandle handle) override {
        assert(textures.erase(handle.value) == 1);
    }
    Result draw_hud(std::span<const HudQuad> quads) override {
        for (const auto& quad : quads) {
            assert(textures.contains(quad.texture.value));
            for (const auto& p : quad.positions)
                for (float c : p)
                    assert(std::isfinite(c));
            for (const auto& c : quad.colors)
                for (float v : c)
                    assert(std::isfinite(v) && v >= 0 && v <= 1);
        }
        frame.assign(quads.begin(), quads.end());
        return Result::ok;
    }
};
class ConfigFixture final : public ResourceReader {
  public:
    Result read(std::string_view path, std::vector<std::byte>& output) override {
        if (path != "Ini/RoadWars.ini")
            return Result::not_found;
        constexpr std::string_view text =
            "[ROADWARS]\nSCREENX=640\nSCREENY=480\nVIEWDISTANCESCALAR=.4\nSPECIAL_EFFECTS_SCALAR=."
            "33\nSKIDMARK_SCALAR=.66\nCAR_DETAIL=2\nMUSIC=.8\nSFX=.9\nINGAMEMUSIC=."
            "6\nSOUNDCHANNELS=20\nUSERNAME=PLAYER\n";
        output.clear();
        for (char c : text)
            output.push_back(static_cast<std::byte>(c));
        return Result::ok;
    }
};
} // namespace
int main(int argc, char** argv) {
    assert(frontend_keyboard_name(0x1e) == "A");
    assert(frontend_keyboard_name(0x9c) == "NUMPAD ENTER");
    assert(frontend_keyboard_name(-1).empty());
    assert(frontend_joystick_name(0) == "JOYX POSITIVE");
    assert(frontend_joystick_name(21) == "JOY BUTTON 16");
    assert(frontend_joystick_name(99).empty());
    ConfigFixture fixture;
    ResourceStore fixture_store(fixture);
    std::string error;
    std::vector<FrontendOptionRow> rows;
    assert(frontend_custom_rows(fixture_store, "graphics", rows, error) == Result::ok);
    assert(rows.size() == 1 && !rows[0].available);
    assert(rows[0].id == "unavailable");
    assert(frontend_custom_rows(fixture_store, "sound", rows, error) == Result::ok);
    assert(rows.size() == 6 && rows[1].values[rows[1].selected] == "80%");
    assert(rows[2].values[rows[2].selected] == "90%");
    assert(rows[5].values[rows[5].selected] == "20");
    assert(frontend_custom_rows(fixture_store, "loading", rows, error) == Result::not_found);
    assert(!FrontendCustom::supports("missing_screen"));
    if (argc < 2)
        return 0;
    Files files(argv[1]);
    ResourceStore resources(files);
    assert(resources.open() == Result::ok);
    CanvasRenderer renderer;
    {
        FrontendCanvas canvas(resources, renderer);
        const auto loaded = canvas.load(error);
        if (loaded != Result::ok) {
            std::cerr << error << '\n';
            return 1;
        }
        FrontendCustomState state;
        state.elapsed = 4;
        state.loading_progress = .5f;
        state.message = "PREVIEW";
        state.racers = {{"Animal", "ANIMAL", 25, 5000, true},
                        {"Bud the Crud", "BUD THE CRUD", 20, 4000, true},
                        {"Niki Roadkill", "NIKI ROADKILL", 15, 3000, true},
                        {"Radcliff", "RADCLIFF", 10, 2000, true},
                        {"Electra", "ELECTRA", 5, 1000, true},
                        {"Sarge", "SARGE", 0, 0, false}};
        for (const auto screen :
             {"graphics", "sound", "game", "keyboard", "joystick", "create_season", "load_season",
              "select_session", "start_race_summary", "race_results", "standings", "season_results",
              "loading", "dialog", "pause", "host_or_join", "modify_vehicle", "victory",
              "defeat"}) {
            state.screen = screen;
            state.rows.clear();
            auto result = frontend_custom_rows(resources, screen, state.rows, error);
            assert(result == Result::ok || result == Result::not_found);
            for (const auto variant : {"", "host", "join", "TCP-IP", "SERIAL", "MODEM"}) {
                state.variant = variant;
                for (double time : {0., .1, .501, 1.4, 4.}) {
                    state.elapsed = time;
                    canvas.begin();
                    result = FrontendCustom::draw(canvas, state, error);
                    if (result != Result::ok) {
                        std::cerr << screen << ": " << error << '\n';
                        return 1;
                    }
                    assert(canvas.submit() == Result::ok);
                    assert(!renderer.frame.empty());
                }
            }
        }
        state.screen = "modify_vehicle";
        for (const auto category :
             {"tires", "drive_train", "weapons", "armor", "repair", "paint"}) {
            state.variant = category;
            assert(frontend_custom_rows(resources, state.screen, state.rows, error, category) ==
                   Result::ok);
            const auto item_count = state.rows.front().values.size();
            for (std::size_t item = 0; item < item_count; ++item) {
                assert(frontend_custom_rows(resources, state.screen, state.rows, error, category,
                                            item) == Result::ok);
                canvas.begin();
                const auto result = FrontendCustom::draw(canvas, state, error);
                if (result != Result::ok) {
                    std::cerr << category << ": " << error << '\n';
                    return 1;
                }
                assert(canvas.submit() == Result::ok);
            }
        }
        state.screen = "pause";
        for (const auto variant :
             {"Single Race", "Single Race No Replay", "Single Race Over",
              "Single Race Over No Replay", "Season Race", "Season Race No Replay",
              "Season Race Over", "Season Race Over No Replay", "Multiplayer", "Multiplayer Over",
              "Multiplayer Over Join"}) {
            assert(frontend_custom_rows(resources, state.screen, state.rows, error, variant) ==
                   Result::ok);
            canvas.begin();
            assert(FrontendCustom::draw(canvas, state, error) == Result::ok);
            assert(canvas.submit() == Result::ok);
        }
        // The first results portrait enters from x641, then reaches x140 after .501s.
        state.screen = "race_results";
        state.rows.clear();
        state.racers.resize(1);
        state.elapsed = 0;
        canvas.begin();
        assert(FrontendCustom::draw(canvas, state, error) == Result::ok);
        bool entering = false;
        for (const auto& quad : canvas.commands())
            entering |= quad.positions[0][0] == 641;
        assert(entering);
        state.elapsed = .501;
        canvas.begin();
        assert(FrontendCustom::draw(canvas, state, error) == Result::ok);
        bool arrived = false;
        for (const auto& quad : canvas.commands())
            arrived |= quad.positions[0][0] == 140;
        assert(arrived);
        state.screen = "graphics";
        state.rows.clear();
        canvas.begin();
        assert(FrontendCustom::draw(canvas, state, error) == Result::invalid_argument);
        state.screen = "loading";
        state.loading_progress = 2;
        assert(FrontendCustom::draw(canvas, state, error) == Result::invalid_argument);
    }
    assert(renderer.textures.empty() && renderer.frame.empty());
    std::cout << "CUSTOM_FRONTEND_ASSETS_OK\n";
}
