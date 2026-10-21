#include "game_src/unresolved/game_initialization.hpp"

#include <cmath>
#include <limits>
#include <utility>

namespace roadwars {
namespace {

bool is_resource_segment(const std::string& value) {
    return !value.empty() && value != "." && value != ".." &&
           value.find_first_of("/\\:\r\n") == std::string::npos &&
           value.find('\0') == std::string::npos;
}

Result required_number(const Configuration& config, const char* section, const char* key,
                       float& value, std::string& error) {
    const auto result = config.number(section, key, value);
    if (result != Result::ok) {
        error = std::string("Missing or invalid initialization value: [") + section + "] " + key;
    }
    return result;
}

Result load_config(ResourceStore& resources, const std::string& path, Configuration& config,
                   std::string& error) {
    const auto result = config.load(resources, path);
    if (result != Result::ok)
        error = "Could not load initialization configuration: " + path;
    return result;
}

} // namespace

// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0045c8ab
// Partial host adaptation: this implements shared data plus explicit test-profile
// selection. Original window/device creation, audio, menu and failure dialogs are
// not this function and must not be counted as recovered by this successful path.
Result initialize_test_game(ResourceStore& resources, const BootOptions& options,
                            InitializedGame& output, std::string& error) {
    error.clear();
    if (!is_resource_segment(options.track) || !is_resource_segment(options.vehicle)) {
        error = "Track and vehicle must be individual installation-directory names";
        return Result::invalid_argument;
    }

    InitializedGame initialized;
    initialized.options = options;
    auto result = load_config(resources, "Ini/RoadWars.ini", initialized.global_config, error);
    if (result != Result::ok)
        return result;

    float width{}, height{}, fullscreen{}, language{};
    for (const auto& item :
         {std::pair{"SCREENX", &width}, std::pair{"SCREENY", &height},
          std::pair{"FULLSCREEN", &fullscreen}, std::pair{"LANGUAGE", &language}}) {
        result =
            required_number(initialized.global_config, "ROADWARS", item.first, *item.second, error);
        if (result != Result::ok)
            return result;
    }
    if (width < 1 || width > 32768 || height < 1 || height > 32768 || std::floor(width) != width ||
        std::floor(height) != height || std::floor(fullscreen) != fullscreen ||
        std::floor(language) != language || language < 0 ||
        language > std::numeric_limits<std::int16_t>::max()) {
        error = "Invalid original display or language settings";
        return Result::invalid_argument;
    }
    initialized.display = {static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height),
                           fullscreen != 0, static_cast<std::int32_t>(language)};

    // 0045c8ab opens both binary tables before device creation. Keep them as
    // opaque bytes until a recovered localization query requires their grammar.
    result = resources.read("Localization/Garage.bin", initialized.garage_localization);
    if (result != Result::ok || initialized.garage_localization.empty()) {
        error = "Missing initialization localization table: Localization/Garage.bin";
        return result == Result::ok ? Result::invalid_argument : result;
    }
    result = resources.read("Localization/GarageDescriptions.bin", initialized.garage_descriptions);
    if (result != Result::ok || initialized.garage_descriptions.empty()) {
        error = "Missing initialization localization table: Localization/GarageDescriptions.bin";
        return result == Result::ok ? Result::invalid_argument : result;
    }

    // The original reaches resource/menu selection through 004ab60d. The test
    // profile explicitly supplies that selection; it does not fabricate a menu.
    result = load_config(resources, "Cars/" + options.vehicle + "/Data/Data.ini",
                         initialized.vehicle_config, error);
    if (result != Result::ok)
        return result;
    result = load_config(resources, "Tracks/" + options.track + "/World/Data.ini",
                         initialized.track_config, error);
    if (result != Result::ok)
        return result;
    result =
        load_config(resources, "Tracks/Shared/Data.ini", initialized.track_shared_config, error);
    if (result != Result::ok)
        return result;

    // Commit only after every required shared/test data dependency succeeds.
    output = std::move(initialized);
    return Result::ok;
}

} // namespace roadwars
