#pragma once

#include "game_src/unresolved/resource_store.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace roadwars {

// New test-entry options, not an original binary structure or menu selection.
struct BootOptions {
    std::string track;
    std::string vehicle;
};

struct OriginalDisplaySettings {
    std::uint32_t width{};
    std::uint32_t height{};
    bool fullscreen{};
    std::int32_t language{};
};

struct InitializedGame {
    BootOptions options;
    OriginalDisplaySettings display;
    Configuration global_config;
    Configuration vehicle_config;
    Configuration track_config;
    Configuration track_shared_config;
    // Initialization opens these tables. Decoding their query grammar is not
    // needed by the explicit free-driving profile and remains unrecovered.
    std::vector<std::byte> garage_localization;
    std::vector<std::byte> garage_descriptions;
};

// Shared data initialization for the test profile. Window, device and audio
// capabilities are supplied by the host, not emulated here.
Result initialize_test_game(ResourceStore& resources, const BootOptions& options,
                            InitializedGame& output, std::string& error);

} // namespace roadwars
