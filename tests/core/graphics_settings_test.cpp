#include "game_src/core/graphics_settings.hpp"
#include "game_src/unresolved/resource_store.hpp"

#include <cmath>
#include <limits>
#include <map>
#include <source_location>
#include <stdexcept>

using namespace roadwars;
namespace {
void require(bool value, std::source_location line = std::source_location::current()) {
    if (!value)
        throw std::runtime_error("Graphics settings contract failed at line " +
                                 std::to_string(line.line()));
}
struct Host final : GraphicsSettingsHost {
    GraphicsSettings live, saved;
    Result apply_result{Result::ok}, save_result{Result::ok};
    int saves{};
    GraphicsCapabilities capabilities() const override {
        return {{{"Monitor", 1920, 1080}},
                {RenderingBackend::automatic, RenderingBackend::vulkan},
                {AntiAliasing::off, AntiAliasing::fxaa}, true, true, false};
    }
    GraphicsSettings current_settings() const override { return live; }
    Result apply_live(const GraphicsSettings& s) override {
        if (apply_result == Result::ok)
            live = s;
        return apply_result;
    }
    Result persist(const GraphicsSettings& s) override {
        if (save_result == Result::ok) {
            saved = s;
            ++saves;
        }
        return save_result;
    }
};
void transaction_contract() {
    Host host;
    GraphicsSettingsTransaction resized(host);
    resized.observe_window_size(1100, 700);
    require(resized.draft().width == 1100 && resized.confirmed().height == 700);
    resized.draft().width = 1280;
    resized.observe_window_size(1200, 800);
    require(resized.draft().width == 1280 && resized.confirmed().width == 1200);
    resized.discard_draft();
    require(resized.draft().width == 1200);
    GraphicsSettingsTransaction tx(host);
    tx.draft().width = 1280;
    tx.draft().chrome_mapping = false;
    require(tx.apply(100) == Result::ok && tx.pending_confirmation());
    require(host.live.width == 1280 && host.saves == 0);
    require(tx.seconds_remaining(110) == 5);
    // Candidate is immutable during confirmation, even if the UI edits its draft.
    tx.draft().width = 1920;
    require(tx.confirm() == Result::ok);
    require(host.saved.width == 1280 && tx.confirmed().width == 1280);
    tx.draft().width = 1600;
    require(tx.apply(120) == Result::ok);
    require(tx.update(134.99) == Result::ok && tx.pending_confirmation());
    require(tx.update(135) == Result::ok && !tx.pending_confirmation());
    require(host.live.width == 1280 && !host.live.chrome_mapping && host.saves == 1);
    tx.draft().backend = RenderingBackend::vulkan;
    require(tx.apply(140) == Result::ok && !tx.pending_confirmation());
    require(host.saved.backend == RenderingBackend::vulkan && host.saves == 2);
    tx.draft().window_mode = WindowMode::fullscreen;
    require(tx.apply(145) == Result::ok);
    require(tx.apply(146) == Result::unavailable);
    host.save_result = Result::unavailable;
    require(tx.confirm() == Result::unavailable && tx.pending_confirmation());
    host.apply_result = Result::unavailable;
    require(tx.revert() == Result::unavailable && tx.pending_confirmation());
    host.apply_result = Result::ok;
    require(tx.revert() == Result::ok && host.live.window_mode == WindowMode::windowed);
    tx.draft().aa = AntiAliasing::fxaa;
    require(tx.apply(150) == Result::unavailable);
    require(host.live.aa == AntiAliasing::off && !tx.pending_confirmation());
    host.save_result = Result::ok;
    tx.draft().aa = AntiAliasing::msaa_8x;
    require(tx.apply(155) == Result::unavailable);
    tx.discard_draft();
    tx.draft().render_scale = std::numeric_limits<float>::quiet_NaN();
    require(tx.apply(160) == Result::invalid_argument);
    tx.discard_draft();
    tx.draft().car_detail = 0;
    require(tx.apply(161) == Result::unavailable);
    tx.discard_draft();
    require(tx.update(159) == Result::invalid_argument);
    require(tx.apply(std::numeric_limits<double>::infinity()) == Result::invalid_argument);
    require(tx.apply(162) == Result::ok);
}
struct Reader final : ResourceReader {
    std::map<std::string, std::vector<std::byte>> files;
    Result read(std::string_view path, std::vector<std::byte>& bytes) override {
        const auto entry = files.find(ResourceStore::canonical(path));
        if (entry == files.end())
            return Result::not_found;
        bytes = entry->second;
        return Result::ok;
    }
};
std::vector<std::byte> bytes(std::string_view text, bool encoded) {
    std::vector<std::byte> result;
    for (unsigned char ch : text)
        result.push_back(static_cast<std::byte>(encoded ? (ch + 1) & 255 : ch));
    return result;
}
void overlay_contract(bool encoded, bool archived) {
    Reader reader;
    const auto data = bytes("[ROADWARS]\nVIEWDISTANCESCALAR=.4\nUNCHANGED=77\n", encoded);
    const std::string path = encoded ? "Ini/RoadWars.pse" : "Ini/RoadWars.ini";
    reader.files[ResourceStore::canonical(path)] = data;
    ResourceStore resources(reader);
    if (archived) {
        auto& index = reader.files["DATA.PKH"];
        const auto put = [&](unsigned value) {
            for (int i = 0; i < 4; ++i)
                index.push_back(static_cast<std::byte>((value >> (8 * i)) & 255));
        };
        put(static_cast<unsigned>(path.size() + 1));
        const auto name = bytes(path, true);
        index.insert(index.end(), name.begin(), name.end());
        index.push_back(std::byte{});
        put(0);
        put(0);
        put(static_cast<unsigned>(data.size()));
        reader.files["DATA.PAK"] = data;
        require(resources.open() == Result::ok);
        reader.files.erase(ResourceStore::canonical(path));
    }
    require(resources.set_configuration_override("ini\\roadwars.ini", "roadwars",
                                                 "viewdistancescalar", ".75") == Result::ok);
    Configuration config;
    require(config.load(resources, "Ini/RoadWars.ini") == Result::ok);
    float scalar{};
    require(config.number("ROADWARS", "VIEWDISTANCESCALAR", scalar) == Result::ok && scalar == .75f);
    require(config.value("ROADWARS", "UNCHANGED") == "77");
    std::vector<std::byte> original;
    require(resources.read(path, original) == Result::ok && original == data);
    resources.clear_configuration_overrides("INI/ROADWARS.INI");
    require(config.load(resources, "Ini/RoadWars.ini") == Result::ok);
    require(config.number("ROADWARS", "VIEWDISTANCESCALAR", scalar) == Result::ok && scalar == .4f);
    require(resources.set_configuration_override("../invalid.ini", "x", "y", "1") ==
            Result::invalid_argument);
}
} // namespace
int main() {
    transaction_contract();
    overlay_contract(false, false);
    overlay_contract(true, false);
    overlay_contract(false, true);
    overlay_contract(true, true);
}
