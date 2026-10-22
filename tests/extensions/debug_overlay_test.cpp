#include "game_src/extensions/console_streams.hpp"
#include "game_src/extensions/debug_overlay.hpp"
#include <cmath>
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
    FrameTiming timing;
    timing.record(0);
    timing.record(-1);
    timing.record(NAN);
    require(timing.count() == 0);
    for (int i = 0; i < 599; ++i)
        timing.record(1000.0f / 120);
    timing.record(50);
    auto summary = timing.summary(1000.0f / 120);
    require(summary.count == 600 && summary.late == 1 && summary.maximum == 50);
    require(std::abs(summary.p95 - 1000.0f / 120) < .001f);
    require(summary.p99 == summary.p95);
    for (int i = 0; i < 600; ++i)
        timing.record(10);
    summary = timing.summary(0);
    require(timing.count() == 600 && timing.offset() == 0 && summary.mean == 10 &&
            summary.maximum == 10 && summary.late == 0);
    using roadwars::Vec3;
    require(race_debug_distance_alpha({0, 0, 0}, {}, 200) == 1);
    require(race_debug_distance_alpha({160, 0, 0}, {}, 200) == 1);
    require(std::abs(race_debug_distance_alpha({180, 0, 0}, {}, 200) - .5f) < .00001f);
    require(race_debug_distance_alpha({200, 0, 0}, {}, 200) == 0);
    require(race_debug_distance_alpha({201, 0, 0}, {}, 200) == 0);
    require(race_debug_distance_alpha({180, 0, 0}, {}, 200) ==
            race_debug_distance_alpha({}, {180, 0, 0}, 200));
    Vec3 sphere_from{-300, 0, 0}, sphere_to{300, 0, 0};
    require(clip_race_debug_distance(sphere_from, sphere_to, {}, 200));
    require(sphere_from.x == -200 && sphere_to.x == 200);
    sphere_from = {300, 300, 0};
    sphere_to = {400, 300, 0};
    require(!clip_race_debug_distance(sphere_from, sphere_to, {}, 200));
    require(!clip_race_debug_distance(sphere_from, sphere_to, {}, 0));
    require(race_debug_distance_alpha({}, {}, 0) == 0);
    roadwars::RaceDebugCamera camera;
    std::array<float, 2> from, to;
    require(project_race_debug_line(camera, {-1, 0, 10}, {1, 0, 10}, 640, 480, from, to));
    require(from[0] < 320 && to[0] > 320 && from[1] == 240);
    require(!project_race_debug_line(camera, {0, 0, -1}, {1, 0, -2}, 640, 480, from, to));
    require(project_race_debug_line(camera, {0, 0, -1}, {1, 0, 10}, 640, 480, from, to));
    require(project_race_debug_line(camera, {-100, 0, 10}, {100, 0, 10}, 640, 480, from, to));
    require(from[0] == 0 && to[0] == 640);
    require(!project_race_debug_line(camera, {100, 0, 10}, {110, 0, 10}, 640, 480, from, to));
    camera.vertical_fov_degrees = 0;
    require(!project_race_debug_line(camera, {}, {}, 640, 480, from, to));
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
        roadwars::RaceDebugSnapshot race;
        race.track = "Diagnostic fixture";
        race.phase = "Running";
        race.total_laps = 3;
        race.participant_count = 3;
        for (int index = 0; index < 2; ++index) {
            roadwars::RaceAiDebugState ai;
            ai.ai_index = index;
            ai.character = "Fixture " + std::to_string(index);
            ai.state = 99; // Unrecognized states must remain visible as raw values.
            race.ai.push_back(ai);
        }
        for (auto kind : {roadwars::RaceDebugKind::route, roadwars::RaceDebugKind::checkpoint,
                          roadwars::RaceDebugKind::collision, roadwars::RaceDebugKind::target}) {
            roadwars::RaceDebugLine line;
            line.from = {-1, 0, 10};
            line.to = {1, 0, 10};
            line.kind = kind;
            line.ai_index = kind == roadwars::RaceDebugKind::target ? 1 : -1;
            race.lines.push_back(line);
        }
        race.labels.push_back({{0, 0, 10}, "Fixture route 0"});
        ui.set_race_debug(race);
        ui.frame(960, 540, 1.0f / 60, 1, 3, true);
        require(ui.has_race_debug() && ui.race_debug_line_count() == 0);
        for (int index = 0; index < 4; ++index) {
            const auto checkbox = ui.race_checkbox_position(index);
            require(checkbox[0] > 0 && checkbox[1] > 0 && ui.hit_test(checkbox[0], checkbox[1]));
            ui.mouse_position(checkbox[0], checkbox[1]);
            ui.frame(960, 540, 1.0f / 60, 1, 3, true);
            ui.mouse_button(0, true);
            ui.frame(960, 540, 1.0f / 60, 1, 3, true);
            ui.mouse_button(0, false);
            ui.frame(960, 540, 1.0f / 60, 1, 3, true);
        }
        require(ui.race_options().show_ai_states && ui.race_options().show_route &&
                ui.race_options().show_checkpoints && ui.race_options().show_collision_shapes);
        require(ui.race_debug_line_count() == 4);
        ui.race_options().selected_ai = 0;
        ui.frame(960, 540, 1.0f / 60, 1, 3, false);
        require(ui.race_debug_line_count() == 3);
        ui.clear_race_debug();
        ui.frame(960, 540, 1.0f / 60, 1, 3, false);
        require(!ui.has_race_debug() && ui.race_debug_line_count() == 0);
        require(ui.race_options().show_route); // Options survive scene changes only in memory.
    }
}
