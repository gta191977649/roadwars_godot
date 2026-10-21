#include "game_src/extensions/console_streams.hpp"
#include "game_src/extensions/debug_overlay.hpp"
#include <imgui.h>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace roadwars::extensions;
void require(bool value) {
    if (!value)
        throw std::runtime_error("Debug overlay contract failed");
}
int main() {
    const auto messages = std::make_shared<DebugMessages>();
    const auto previous = std::cout.rdbuf();
    {
        ConsoleStreams streams(messages);
        std::cout << "stream output" << std::endl;
        std::cerr << "stream error" << std::endl;
        require(messages->contains("stream output") && messages->contains("stream error"));
    }
    require(std::cout.rdbuf() == previous);
    std::thread worker([&] {
        for (int i = 0; i < 5000; ++i)
            messages->append("worker");
    });
    worker.join();
    require(messages->drain().size() == 4096);
    for (int cycle = 0; cycle < 3; ++cycle) {
        DebugOverlay ui(messages);
        auto data = ui.frame(1920, 1080, 1.0f / 60, 1, 123, true);
        require(data && data->TotalIdxCount > 0 && ui.collapsed());
        const auto first_revision = ui.font_revision();
        ui.set_collapsed(false);
        ui.frame(1920, 1080, 1.0f / 60, 2, 456, true);
        require(ui.scale() == 2 && ui.font_revision() > first_revision && !ui.collapsed());
        const auto input = ui.console_input_position();
        require(ui.hit_test(input[0], input[1]) && !ui.hit_test(1919, 1079));
        const auto second_revision = ui.font_revision();
        ui.frame(1920, 1080, 1.0f / 60, 2, 456, true);
        require(ui.font_revision() == second_revision);
        ui.frame(960, 540, 1.0f / 120, 1, 3, false);
        require(ui.scale() == 1 && ui.font_revision() > second_revision);
    }
}
