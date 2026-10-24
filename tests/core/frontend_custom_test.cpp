#include "game_src/unresolved/frontend_custom.hpp"
#include "game_src/unresolved/frontend_dialog.hpp"
#include "game_src/unresolved/frontend_input_names.hpp"
#include "game_src/unresolved/frontend_localization.hpp"
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
        // Dialog geometry and flag=3 text anchoring from 004afd70/004b090a.
        {
            FrontendDialogLayout confirmation, notice;
            assert(frontend_dialog_layout(canvas, false, confirmation) == Result::ok);
            assert(frontend_dialog_layout(canvas, true, notice) == Result::ok);
            assert(confirmation.panel.x == 16 && confirmation.panel.y == 178);
            assert(confirmation.panel.width == 608 && confirmation.panel.height == 124);
            assert(confirmation.buttons[0].x == 207 && confirmation.buttons[1].x == 325);
            assert(confirmation.buttons[0].y == 233 && confirmation.buttons[0].height == 48);
            assert(confirmation.button_centers[0][0] == 261 &&
                   confirmation.button_centers[1][0] == 379);
            assert(notice.buttons[0].x == 266 && notice.button_centers[0][0] == 320);
            assert(confirmation.hit(207, 233) == 0 && confirmation.hit(314.99f, 280.99f) == 0);
            assert(confirmation.hit(315, 233) == -1 && confirmation.hit(325, 281) == -1);
            assert(notice.hit(266, 233) == 0 && notice.hit(374, 233) == -1);
            std::vector<std::byte> bytes;
            Bitmap bitmap;
            HudBitmapFont original_font;
            assert(resources.read("Fonts/16Font12Imp.bmp", bytes) == Result::ok);
            assert(decode_bitmap(bytes, bitmap) == Result::ok);
            assert(original_font.decode(bitmap, frontend_text_alphabet()) == Result::ok);
            const auto expected_width = [&](std::string_view text, float scale) {
                int width{};
                for (unsigned char c : text)
                    width += static_cast<int>((c == ' ' ? original_font.space_width()
                                                        : original_font.glyph(c).width + 1) *
                                              scale);
                return width - static_cast<int>(scale);
            };
            canvas.begin();
            canvas.text(FrontendFont::small, 0, 0,
                        std::string("A\x01"
                                    "B"),
                        false, .8f);
            assert(canvas.commands().size() == 2);
            assert(canvas.commands()[1].positions[0][0] ==
                   static_cast<int>((original_font.glyph('A').width + 1) * .8f) +
                       static_cast<int>(.8f));
            for (float scale : {.8f, 1.f, 1.3f}) {
                assert(canvas.text_width(FrontendFont::small, "A A", scale) ==
                       expected_width("A A", scale));
                canvas.begin();
                canvas.text(FrontendFont::small, 320, 217, "A A", true, scale, 0xffffffff,
                            0xffffffff, FrontendVerticalAlign::center);
                assert(canvas.commands().size() == 2);
                const int left = 320 - expected_width("A A", scale) / 2;
                const int top = 217 - static_cast<int>(original_font.height() * scale / 2);
                assert(canvas.commands()[0].positions[0][0] == left);
                assert(canvas.commands()[0].positions[0][1] == top);
                assert(canvas.commands()[1].positions[0][0] ==
                       left + static_cast<int>((original_font.glyph('A').width + 1) * scale) +
                           static_cast<int>(original_font.space_width() * scale));
            }
            FrontendCustomState dialog;
            dialog.screen = "dialog";
            dialog.preview_data = false;
            dialog.message = "ARE YOU SURE?";
            dialog.selected = 1;
            canvas.begin();
            assert(FrontendCustom::draw(canvas, dialog, error) == Result::ok);
            const auto commands = canvas.commands();
            assert(commands.front().texture.value == confirmation.panel_sprite.texture.value);
            assert(std::abs(commands.front().colors[0][3] - .6f) < 1e-6f);
            assert(commands[1].positions[0][0] == 320 - expected_width(dialog.message, 1) / 2);
            assert(commands[1].positions[0][1] == 217 - original_font.height() / 2);
            assert(commands.back().colors[0] == (std::array<float, 4>{0, 1, 0, 1}));
            // The single-button rejection is opaque and has the original green/yellow tail.
            dialog.variant = "notice";
            dialog.message = "YOU DO NOT HAVE ENOUGH MONEY!";
            canvas.begin();
            assert(FrontendCustom::draw(canvas, dialog, error) == Result::ok);
            assert(canvas.commands().front().colors[0][3] == 1);
            assert(canvas.commands().back().colors[0] == (std::array<float, 4>{0, 1, 0, 1}));
            assert(canvas.commands().back().colors[2] ==
                   (std::array<float, 4>{0, 1, 20 / 255.f, 1}));
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
                const auto qualities = state.rows.size() > 1 && state.rows[1].label == "QUALITY"
                                           ? state.rows[1].values.size()
                                           : 1;
                for (std::size_t quality = 0; quality < qualities; ++quality) {
                    if (state.rows.size() > 1 && state.rows[1].label == "QUALITY")
                        state.rows[1].selected = quality;
                    canvas.begin();
                    const auto result = FrontendCustom::draw(canvas, state, error);
                    if (result != Result::ok) {
                        std::cerr << category << ": " << error << '\n';
                        return 1;
                    }
                    assert(canvas.submit() == Result::ok);
                }
            }
        }
        // 004455f5 uses Nebula12 for every store row; 00460d57 emits past
        // the supplied bottom edge. These fixed coordinates come from Data.ini.
        state.variant = "tires";
        state.preview_data = false;
        state.selected = 0;
        state.rows = {{"ITEM", {"STREET", "SLICKS"}},
                      {"QUALITY", {"POOR", "GOOD", "BEST"}},
                      {"ACTION", {"BUY $500"}}};
        state.fields["DESCRIPTION"] = std::string(1200, 'W');
        state.fields["ADVICE"] = "";
        state.fields["AMMO"] = "0";
        state.fields["PRODUCT_AVAILABLE"] = "1";
        FrontendSprite nebula, mechanic, meter, damage_atlas, body_icon;
        assert(canvas.asset("Fonts/16Nebula12.bmp", nebula) == Result::ok);
        assert(canvas.asset("Cars/Shared/Tires/16Mechanic.bmp", mechanic) == Result::ok);
        assert(canvas.asset("ModifyVehicle/16Meter.bmp", meter) == Result::ok);
        assert(canvas.asset("Cars/Animal/Damage/16DamageShellInternal.bmp", damage_atlas) ==
               Result::ok);
        assert(canvas.asset("Cars/Animal/Garage/16GarIcon1.bmp", body_icon) == Result::ok);
        canvas.begin();
        assert(FrontendCustom::draw(canvas, state, error) == Result::ok);
        bool item_arrow{}, selected_gradient{}, description_overflow{}, mechanic_at_native_y{},
            body_at_native_position{};
        for (const auto& quad : canvas.commands()) {
            if (quad.texture.value == nebula.texture.value) {
                item_arrow |= quad.positions[0][0] == 20 && quad.positions[0][1] == 50;
                selected_gradient |= quad.positions[0][1] == 50 &&
                                     std::abs(quad.colors[0][3] - 150.f / 255) < .0001f &&
                                     quad.colors[2][0] == 1 && quad.colors[2][1] == 1 &&
                                     quad.colors[2][2] == 0;
                description_overflow |= quad.positions[0][0] == 30 && quad.positions[0][1] > 260;
            }
            mechanic_at_native_y |= quad.texture.value == mechanic.texture.value &&
                                    quad.positions[0][0] == 20 && quad.positions[0][1] == 190;
            body_at_native_position |= quad.texture.value == body_icon.texture.value &&
                                       quad.positions[0][0] == 473 && quad.positions[0][1] == 375 &&
                                       quad.positions[2][1] == 441;
        }
        assert(item_arrow && selected_gradient && description_overflow && mechanic_at_native_y &&
               body_at_native_position);
        // Translated labels must not become resource directory names.
        state.fields["PRODUCT_RESOURCE"] = "Cars/Shared/Tires/STREET/POOR";
        state.rows[0].values[0] = "PNEUS ROUTIERS";
        state.rows[1].values[0] = "MEDIOCRE";
        canvas.begin();
        assert(FrontendCustom::draw(canvas, state, error) == Result::ok);
        state.preview_data = true;
        state.fields.clear();
        state.variant.clear();
        state.rows.clear();
        state.fields["TIRES"] = "Street";
        state.fields["TIRES_TYPE"] = "Street";
        state.fields["TIRES_QUALITY"] = "Poor";
        state.fields["ARMOR"] = "None";
        state.fields["ARMOR_QUALITY"] = "Good";
        state.fields["DRIVE_RATIO"] = "0.5";
        state.fields["NITROUS_AMMO"] = "2";
        state.fields["NITROUS_RATIO"] = "0.5";
        state.fields["INTERNAL_HP"] = "90";
        state.fields["MAX_INTERNAL_HP"] = "100";
        for (const auto side : {"FRONT", "RIGHT", "REAR", "LEFT"}) {
            state.fields[std::string(side) + "_HP"] = "25";
            state.fields["MAX_" + std::string(side) + "_HP"] = "100";
        }
        canvas.begin();
        assert(FrontendCustom::draw(canvas, state, error) == Result::ok);
        std::set<int> filled_bar_widths;
        bool damaged_health_color{};
        for (const auto& quad : canvas.commands()) {
            const auto& color = quad.colors[0];
            const auto width = static_cast<int>(quad.positions[1][0] - quad.positions[0][0]);
            const auto height = static_cast<int>(quad.positions[2][1] - quad.positions[0][1]);
            if (height == 5 && std::abs(color[0] - 70.f / 255) < .0001f &&
                std::abs(color[1] - 240.f / 255) < .0001f)
                filled_bar_widths.insert(width);
            damaged_health_color |= color[0] == 1 && std::abs(color[1] - 127.f / 255) < .0001f &&
                                    color[2] == 0 && std::abs(color[3] - .8f) < .0001f;
        }
        assert(filled_bar_widths == std::set<int>({21, 24, 37, 49}));
        assert(damaged_health_color);
        std::size_t regions{};
        bool sampled_meter{};
        for (const auto& quad : canvas.commands()) {
            if (quad.texture.value == damage_atlas.texture.value) {
                ++regions;
                if (quad.positions[0][0] == 209 && quad.positions[0][1] == 413) {
                    assert(quad.positions[2][0] == 242 && quad.positions[2][1] == 474);
                    assert(quad.uv[0][0] == 14.f / 128 && quad.uv[0][1] == 1 - 28.f / 128);
                }
            }
            sampled_meter |= quad.texture.value == meter.texture.value &&
                             quad.uv[0][1] == 1 - 1.f / 32 && quad.uv[2][1] == 1 - 6.f / 32;
        }
        assert(regions == 5 && sampled_meter);
        // Font1's original top-only +5 offset must not translate its lower edge.
        state.rows = {{"TIRES", {}}};
        canvas.begin();
        assert(FrontendCustom::draw(canvas, state, error) == Result::ok);
        bool slanted_category{};
        for (const auto& quad : canvas.commands())
            slanted_category |= quad.positions[0][0] == 55 && quad.positions[3][0] == 50 &&
                                quad.positions[0][1] == 50;
        assert(slanted_category);
        Configuration characters;
        assert(characters.load(resources, "Screens/SelectCharacter/Data.ini") == Result::ok);
        float character_count{};
        assert(characters.number("INFO", "NUMSELECTIONS", character_count) == Result::ok &&
               character_count == 12);
        for (int character = 0; character < 12; ++character) {
            state.fields["CHARACTER"] =
                characters.value("SELECTION" + std::to_string(character), "VALUE");
            canvas.begin();
            assert(FrontendCustom::draw(canvas, state, error) == Result::ok);
            assert(canvas.submit() == Result::ok);
        }
        state.rows.clear();
        state.fields.clear();
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
