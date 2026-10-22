#include "game_src/extensions/debug_overlay.hpp"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <imgui.h>
#include <imgui_console.h>
#include <imgui_internal.h>

namespace roadwars::extensions {
float race_debug_distance_alpha(Vec3 point, Vec3 eye, float distance) {
    if (!std::isfinite(distance) || distance <= 0)
        return 0;
    const double dx = static_cast<double>(point.x) - eye.x;
    const double dy = static_cast<double>(point.y) - eye.y;
    const double dz = static_cast<double>(point.z) - eye.z;
    const double length = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (!std::isfinite(length))
        return 0;
    return static_cast<float>(std::clamp((distance - length) / (distance * .2), 0.0, 1.0));
}
bool clip_race_debug_distance(Vec3& from, Vec3& to, Vec3 eye, float distance) {
    if (!std::isfinite(distance) || distance <= 0)
        return false;
    const double dx = static_cast<double>(to.x) - from.x;
    const double dy = static_cast<double>(to.y) - from.y;
    const double dz = static_cast<double>(to.z) - from.z;
    const double x = static_cast<double>(from.x) - eye.x;
    const double y = static_cast<double>(from.y) - eye.y;
    const double z = static_cast<double>(from.z) - eye.z;
    const double a = dx * dx + dy * dy + dz * dz;
    const double b = 2 * (dx * x + dy * y + dz * z);
    const double c = x * x + y * y + z * z - static_cast<double>(distance) * distance;
    if (!std::isfinite(a) || !std::isfinite(b) || !std::isfinite(c))
        return false;
    if (a == 0)
        return c <= 0;
    const double discriminant = b * b - 4 * a * c;
    if (discriminant < 0)
        return false;
    const double root = std::sqrt(discriminant);
    const double low = std::max(0.0, (-b - root) / (2 * a));
    const double high = std::min(1.0, (-b + root) / (2 * a));
    if (low > high)
        return false;
    const auto at = [&](double t) {
        return Vec3{static_cast<float>(from.x + dx * t), static_cast<float>(from.y + dy * t),
                    static_cast<float>(from.z + dz * t)};
    };
    const auto clipped_to = at(high), clipped_from = at(low);
    from = clipped_from;
    to = clipped_to;
    return true;
}
bool project_race_debug_line(const RaceDebugCamera& camera, Vec3 from, Vec3 to, float width,
                             float height, std::array<float, 2>& screen_from,
                             std::array<float, 2>& screen_to) {
    if (!(width > 0 && height > 0 && std::isfinite(width) && std::isfinite(height) &&
          std::isfinite(camera.vertical_fov_degrees) && camera.vertical_fov_degrees > 0 &&
          camera.vertical_fov_degrees < 179 && camera.near_distance > 0 &&
          std::isfinite(camera.near_distance)))
        return false;
    const auto local = [&](Vec3 p) {
        p = {p.x - camera.eye.x, p.y - camera.eye.y, p.z - camera.eye.z};
        const auto dot = [&](Vec3 v) { return p.x * v.x + p.y * v.y + p.z * v.z; };
        return Vec3{dot(camera.right), dot(camera.up), dot(camera.forward)};
    };
    auto a = local(from), b = local(to);
    const auto finite = [](Vec3 v) {
        return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
    };
    if (!finite(a) || !finite(b))
        return false;
    const float vertical = std::tan(camera.vertical_fov_degrees * 0.008726646259971648f);
    const float horizontal = vertical * width / height;
    const std::array<std::array<float, 4>, 5> planes{{{0, 0, 1, -camera.near_distance},
                                                      {1, 0, horizontal, 0},
                                                      {-1, 0, horizontal, 0},
                                                      {0, 1, vertical, 0},
                                                      {0, -1, vertical, 0}}};
    for (const auto& plane : planes) {
        const auto distance = [&](Vec3 p) {
            return p.x * plane[0] + p.y * plane[1] + p.z * plane[2] + plane[3];
        };
        const float da = distance(a), db = distance(b);
        if (da < 0 && db < 0)
            return false;
        if ((da < 0) != (db < 0)) {
            const float t = da / (da - db);
            const Vec3 intersection{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t,
                                    a.z + (b.z - a.z) * t};
            if (da < 0)
                a = intersection;
            else
                b = intersection;
        }
    }
    const auto screen = [&](Vec3 p) {
        return std::array<float, 2>{
            std::clamp((p.x / (p.z * horizontal) + 1) * width * .5f, 0.0f, width),
            std::clamp((1 - p.y / (p.z * vertical)) * height * .5f, 0.0f, height)};
    };
    screen_from = screen(a);
    screen_to = screen(b);
    return true;
}

void DebugOverlay::draw_race_debug(float width, float height) {
    race_debug_line_count_ = 0;
    race_checkbox_positions_ = {};
    race_inspect_positions_ = {};
    race_track_positions_.clear();
    if (!race_debug_)
        return;
    const auto& race = *race_debug_;
    auto& options = race_options_;
    options.route_distance = std::isfinite(options.route_distance)
                                 ? std::clamp(options.route_distance, 10.0f, 2000.0f)
                                 : 200.0f;
    const auto selected = std::find_if(race.ai.begin(), race.ai.end(), [&](const auto& ai) {
        return ai.ai_index == options.selected_ai;
    });
    if (selected == race.ai.end())
        options.selected_ai = -1;
    ImGui::SetNextWindowPos(ImVec2(12 * scale_, 70 * scale_), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(
        ImVec2(std::max(200.0f, std::min(370 * scale_, width - 24 * scale_)), 0),
        ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Race Debug", nullptr, ImGuiWindowFlags_NoSavedSettings)) {
        const auto record_track = [&] {
            const auto low = ImGui::GetItemRectMin(), high = ImGui::GetItemRectMax();
            race_track_positions_.push_back({(low.x + high.x) * .5f, (low.y + high.y) * .5f});
        };
        const bool open_track = ImGui::BeginCombo("Track##RaceDebugTrack", race.track.c_str());
        record_track();
        if (open_track) {
            for (std::size_t index = 0; index < race.available_tracks.size(); ++index) {
                const auto& track = race.available_tracks[index];
                if (ImGui::Selectable(track.c_str(),
                                      static_cast<int>(index) == race.selected_track) &&
                    static_cast<int>(index) != race.selected_track)
                    options.requested_track = track;
                record_track();
            }
            ImGui::EndCombo();
        }
        ImGui::Text("Phase: %s", race.phase.c_str());
        ImGui::Text("Laps: %d  Racers: %d", race.total_laps, race.participant_count);
        ImGui::Text("Tick: %llu  Time: %.3f s",
                    static_cast<unsigned long long>(race.simulation_ticks), race.race_time);
        if (!race.combat_status.empty())
            ImGui::TextWrapped("Combat: %s", race.combat_status.c_str());
        const auto checkbox = [&](int i, const char* label, bool& value) {
            ImGui::Checkbox(label, &value);
            const auto low = ImGui::GetItemRectMin(), high = ImGui::GetItemRectMax();
            race_checkbox_positions_[i] = {(low.x + high.x) * .5f, (low.y + high.y) * .5f};
        };
        checkbox(0, "Show AI States", options.show_ai_states);
        checkbox(1, "Show Route Debug", options.show_route);
        checkbox(2, "Show Checkpoints", options.show_checkpoints);
        checkbox(3, "Show Collision Shapes", options.show_collision_shapes);
        checkbox(4, "Auto Drive", options.auto_drive);
        if (options.show_route) {
            ImGui::SetNextItemWidth(120 * scale_);
            ImGui::SliderFloat("Route Distance (m)", &options.route_distance, 10, 2000, "%.0f");
        }
        const auto player_label = "Player: " + race.player_character;
        const auto selection =
            options.selected_ai < 0 ? player_label.c_str() : selected->character.c_str();
        const auto record_inspect = [&](int index) {
            const auto low = ImGui::GetItemRectMin(), high = ImGui::GetItemRectMax();
            race_inspect_positions_[index] = {(low.x + high.x) * .5f, (low.y + high.y) * .5f};
        };
        const bool open_inspect = ImGui::BeginCombo("Inspect", selection);
        record_inspect(0);
        if (open_inspect) {
            if (ImGui::Selectable(player_label.c_str(), options.selected_ai == -1))
                options.selected_ai = -1;
            record_inspect(1);
            int row = 2;
            for (const auto& ai : race.ai) {
                ImGui::PushID(ai.ai_index);
                if (ImGui::Selectable(ai.character.c_str(), options.selected_ai == ai.ai_index))
                    options.selected_ai = ai.ai_index;
                if (row < 7)
                    record_inspect(row++);
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        ImGui::TextWrapped("Route: cyan  Branch: magenta  Checkpoint: yellow  Collision: red");
        ImGui::TextDisabled("` / F8: UI/game focus. Speed values: m/s.");
        if (ImGui::CollapsingHeader("Selected equipment"))
            for (const auto& item : race.equipment) {
                ImGui::PushID(item.participant);
                if (ImGui::TreeNode(item.character.c_str())) {
                    ImGui::Text("Tires: %s", item.tires.c_str());
                    ImGui::Text("Armor: %s", item.armor.c_str());
                    ImGui::Text("HP: %.1f / %.1f  Nitro ammo: %d", item.internal_hp,
                                item.maximum_internal_hp, item.nitrous_ammo);
                    ImGui::Text("Armor HP F/R/Rear/L: %.1f / %.1f / %.1f / %.1f", item.armor_hp[0],
                                item.armor_hp[1], item.armor_hp[2], item.armor_hp[3]);
                    ImGui::TextWrapped("Selected front: %s", item.selected_weapons[0].c_str());
                    ImGui::TextWrapped("Selected side: %s", item.selected_weapons[1].c_str());
                    ImGui::TextWrapped("Selected rear: %s", item.selected_weapons[2].c_str());
                    for (const auto& weapon : item.weapons)
                        if (weapon.present) {
                            ImGui::Text("%s: ammo %d  active %d  ready %d  pending %d",
                                        weapon.name.c_str(), weapon.ammunition, weapon.active,
                                        weapon.ready, weapon.pending);
                            if (weapon.elapsed)
                                ImGui::Text("Native timer %.3f s", *weapon.elapsed);
                            ImGui::Text("Deployment roll %.1f  latched target %d",
                                        weapon.deployment_roll, weapon.target);
                            if (weapon.query_target >= 0 || weapon.target >= 0)
                                ImGui::Text("Query target %d", weapon.query_target);
                        }
                    ImGui::Text("Nitro timer %.3f  dying timer %.3f", item.nitrous_timer,
                                item.dying_timer);
                    ImGui::Text("AI side delay %.3f  rear timer %.3f", item.ai_side_delay,
                                item.ai_rear_timer);
                    for (const auto& upgrade : item.drivetrain)
                        ImGui::TextWrapped("%s", upgrade.c_str());
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
        if (ImGui::CollapsingHeader("Scene turrets")) {
            ImGui::Text("Projectiles %llu  vehicle hits %llu",
                        static_cast<unsigned long long>(race.turret_launched),
                        static_cast<unsigned long long>(race.turret_impacts));
            for (const auto& turret : race.turrets)
                ImGui::Text("%s: target %d  heading %.1f  pitch %.1f", turret.name.c_str(),
                            turret.target, turret.heading, turret.pitch);
        }
    }
    ImGui::End();
    if (options.show_ai_states) {
        const auto controls = ImGui::FindWindowByName("Race Debug");
        const float x = controls ? controls->Pos.x + controls->Size.x + 12 * scale_ : width * .45f;
        const float y = controls ? controls->Pos.y : 70 * scale_;
        ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(
            ImVec2(std::max(200.0f, width - x - 12 * scale_), std::min(300 * scale_, height * .7f)),
            ImGuiCond_FirstUseEver);
        if (ImGui::Begin("AI States", nullptr, ImGuiWindowFlags_NoSavedSettings)) {
            constexpr const char* columns[]{
                "AI",     "Character", "State",      "Substate", "Lap",      "Rank",  "Node",
                "Target", "Speed",     "Target m/s", "Steer",    "Throttle", "Brake", "Recovery s"};
            if (ImGui::BeginTable("AI state table", 14,
                                  ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                      ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY |
                                      ImGuiTableFlags_SizingFixedFit)) {
                for (auto column : columns)
                    ImGui::TableSetupColumn(column);
                ImGui::TableSetupScrollFreeze(2, 1);
                ImGui::TableHeadersRow();
                for (const auto& ai : race.ai) {
                    ImGui::TableNextRow();
                    if (options.selected_ai == ai.ai_index)
                        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                                               IM_COL32(50, 90, 120, 150));
                    ImGui::TableNextColumn();
                    ImGui::Text("%d", ai.ai_index);
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(ai.character.c_str());
                    ImGui::TableNextColumn();
                    ImGui::Text("%s (%d)",
                                ai.state_name.empty() ? "Unknown" : ai.state_name.c_str(),
                                ai.state);
                    ImGui::TableNextColumn();
                    ImGui::Text("%s (%d)",
                                ai.substate_name.empty() ? "Unknown" : ai.substate_name.c_str(),
                                ai.substate);
                    ImGui::TableNextColumn();
                    ImGui::Text("%d", ai.completed_laps);
                    ImGui::TableNextColumn();
                    ImGui::Text("%d", ai.rank);
                    ImGui::TableNextColumn();
                    ImGui::Text("%d", ai.route_node);
                    ImGui::TableNextColumn();
                    ImGui::Text("%d", ai.target_node);
                    ImGui::TableNextColumn();
                    ImGui::Text("%.2f", ai.speed);
                    ImGui::TableNextColumn();
                    ImGui::Text("%.2f", ai.target_speed);
                    ImGui::TableNextColumn();
                    ImGui::Text("%.3f", ai.steering);
                    ImGui::TableNextColumn();
                    ImGui::Text("%.3f", ai.throttle);
                    ImGui::TableNextColumn();
                    ImGui::Text("%.3f", ai.brake);
                    ImGui::TableNextColumn();
                    ImGui::Text("%.3f", ai.recovery_time);
                }
                ImGui::EndTable();
            }
        }
        ImGui::End();
    }
    const auto enabled = [&](RaceDebugKind kind, int ai_index) {
        if (kind == RaceDebugKind::vehicle_state)
            return options.show_ai_states;
        if (ai_index >= 0 && options.selected_ai >= 0 && ai_index != options.selected_ai)
            return false;
        switch (kind) {
        case RaceDebugKind::route:
        case RaceDebugKind::target:
            return options.show_route;
        case RaceDebugKind::checkpoint:
            return options.show_checkpoints;
        case RaceDebugKind::collision:
            return options.show_collision_shapes;
        case RaceDebugKind::vehicle_state:
            return options.show_ai_states;
        }
        return false;
    };
    const auto color = [](RaceDebugKind kind, bool branch, int ai_index) -> ImU32 {
        switch (kind) {
        case RaceDebugKind::vehicle_state:
            return IM_COL32_WHITE;
        case RaceDebugKind::route:
            return branch ? IM_COL32(255, 80, 230, 210) : IM_COL32(40, 230, 255, 210);
        case RaceDebugKind::checkpoint:
            return IM_COL32(255, 230, 30, 230);
        case RaceDebugKind::collision:
            return IM_COL32(255, 70, 50, 230);
        case RaceDebugKind::target: {
            constexpr ImU32 colors[]{IM_COL32(100, 255, 100, 255), IM_COL32(255, 170, 60, 255),
                                     IM_COL32(160, 130, 255, 255), IM_COL32(255, 130, 190, 255),
                                     IM_COL32(200, 255, 70, 255),  IM_COL32(255, 255, 255, 255)};
            return colors[static_cast<unsigned>(std::max(ai_index, 0)) % 6];
        }
        }
        return IM_COL32_WHITE;
    };
    auto* draw = ImGui::GetBackgroundDrawList();
    draw->PushClipRect(ImVec2(0, 0), ImVec2(width, height), true);
    for (const auto& line : race.lines) {
        if (!enabled(line.kind, line.ai_index))
            continue;
        const bool route = line.kind == RaceDebugKind::route || line.kind == RaceDebugKind::target;
        auto world_from = line.from, world_to = line.to;
        if (route && !clip_race_debug_distance(world_from, world_to, race.camera.eye,
                                               options.route_distance))
            continue;
        const auto tint = color(line.kind, line.branch, line.ai_index);
        const bool highlight =
            line.highlighted || (line.ai_index >= 0 && line.ai_index == options.selected_ai);
        const auto fade = [&](Vec3 p) {
            return route ? race_debug_distance_alpha(p, race.camera.eye, options.route_distance)
                         : 1;
        };
        const auto faded = [&](ImU32 value, float alpha) {
            return (value & ~IM_COL32_A_MASK) |
                   (static_cast<ImU32>(((value >> IM_COL32_A_SHIFT) & 255) * alpha)
                    << IM_COL32_A_SHIFT);
        };
        const Vec3 displacement{world_to.x - world_from.x, world_to.y - world_from.y,
                                world_to.z - world_from.z};
        const float length =
            std::sqrt(displacement.x * displacement.x + displacement.y * displacement.y +
                      displacement.z * displacement.z);
        const int steps = route ? std::max(1, static_cast<int>(std::ceil(length / 10))) : 1;
        const auto at = [&](float t) {
            return Vec3{world_from.x + displacement.x * t, world_from.y + displacement.y * t,
                        world_from.z + displacement.z * t};
        };
        for (int part = 0; part < steps; ++part) {
            const auto p = at(static_cast<float>(part) / steps);
            const auto q = at(static_cast<float>(part + 1) / steps);
            std::array<float, 2> a, b;
            if (!project_race_debug_line(race.camera, p, q, width, height, a, b))
                continue;
            const auto start = draw->VtxBuffer.Size;
            draw->AddLine(ImVec2(a[0], a[1]), ImVec2(b[0], b[1]), tint,
                          (highlight ? 3.0f : 1.5f) * scale_);
            const float dx = b[0] - a[0], dy = b[1] - a[1], squared = dx * dx + dy * dy;
            const float alpha_from = fade(p), alpha_to = fade(q);
            for (int vertex = start; vertex < draw->VtxBuffer.Size; ++vertex) {
                auto& v = draw->VtxBuffer[vertex];
                const float t =
                    squared == 0
                        ? 0
                        : std::clamp(((v.pos.x - a[0]) * dx + (v.pos.y - a[1]) * dy) / squared,
                                     0.0f, 1.0f);
                v.col = faded(v.col, alpha_from + (alpha_to - alpha_from) * t);
            }
            ++race_debug_line_count_;
        }
        std::array<float, 2> a, b;
        if (route &&
            project_race_debug_line(race.camera, world_from, world_to, width, height, a, b)) {
            const float dx = b[0] - a[0], dy = b[1] - a[1], length = std::hypot(dx, dy);
            if (length > 20 * scale_) {
                const float x = (a[0] + b[0]) * .5f, y = (a[1] + b[1]) * .5f;
                const float ux = dx / length * 5 * scale_, uy = dy / length * 5 * scale_;
                draw->AddTriangleFilled(ImVec2(x + ux, y + uy), ImVec2(x - ux - uy, y - uy + ux),
                                        ImVec2(x - ux + uy, y - uy - ux),
                                        faded(tint, fade(at(.5f))));
            }
        }
    }
    for (const auto& label : race.labels) {
        std::array<float, 2> p, unused;
        const bool route =
            label.kind == RaceDebugKind::route || label.kind == RaceDebugKind::target;
        const float alpha = route ? race_debug_distance_alpha(label.position, race.camera.eye,
                                                              options.route_distance)
                                  : 1;
        if (label.kind == RaceDebugKind::vehicle_state) {
            if (options.show_ai_states &&
                project_race_debug_line(race.camera, label.position, label.position, width, height,
                                        p, unused)) {
                const auto size = ImGui::CalcTextSize(label.text.c_str());
                const ImVec2 origin(p[0] - size.x * .5f, p[1] - size.y * .5f);
                draw->AddRectFilled(
                    ImVec2(origin.x - 3 * scale_, origin.y - 2 * scale_),
                    ImVec2(origin.x + size.x + 3 * scale_, origin.y + size.y + 2 * scale_),
                    IM_COL32(0, 0, 0, 185), 2 * scale_);
                draw->AddText(origin, IM_COL32_WHITE, label.text.c_str());
            }
            continue;
        }
        if (enabled(label.kind, label.ai_index) && alpha > 0 &&
            project_race_debug_line(race.camera, label.position, label.position, width, height, p,
                                    unused))
            draw->AddText(
                ImVec2(p[0], p[1]),
                (color(label.kind, false, label.ai_index) & ~IM_COL32_A_MASK) |
                    (static_cast<ImU32>(
                         ((color(label.kind, false, label.ai_index) >> IM_COL32_A_SHIFT) & 255) *
                         alpha)
                     << IM_COL32_A_SHIFT),
                label.text.c_str());
    }
    draw->PopClipRect();
}

std::array<float, 2> DebugOverlay::race_checkbox_position(int index) const {
    if (index < 0 || index >= static_cast<int>(race_checkbox_positions_.size()))
        return {};
    return race_checkbox_positions_[index];
}
std::array<float, 2> DebugOverlay::race_inspect_position(int index) const {
    return index >= 0 && index < 7 ? race_inspect_positions_[index] : std::array<float, 2>{};
}
class OverlayConsole final : public ImGuiConsole {
  public:
    OverlayConsole() : ImGuiConsole("Console") {}
    void append(const DebugMessage& message) {
        System().Log(message.error ? csys::ERROR : csys::LOG) << message.text << csys::endl;
        m_ScrollToBottom = true;
    }
};
void DebugMessages::append(std::string_view text, bool error) {
    if (text.empty())
        return;
    std::lock_guard lock(mutex_);
    DebugMessage message{std::string(text.substr(0, 65536)), error};
    pending_.push_back(message);
    history_.push_back(std::move(message));
    if (pending_.size() > 4096)
        pending_.pop_front();
    if (history_.size() > 4096)
        history_.pop_front();
    ++count_;
}
std::deque<DebugMessage> DebugMessages::drain() {
    std::lock_guard lock(mutex_);
    std::deque<DebugMessage> result;
    result.swap(pending_);
    return result;
}
bool DebugMessages::contains(std::string_view text) const {
    std::lock_guard lock(mutex_);
    return std::any_of(history_.begin(), history_.end(),
                       [&](const auto& m) { return m.text.find(text) != std::string::npos; });
}
std::uint64_t DebugMessages::count() const {
    std::lock_guard lock(mutex_);
    return count_;
}
DebugOverlay::DebugOverlay(std::shared_ptr<DebugMessages> messages, std::vector<unsigned char> font)
    : messages_(std::move(messages)), font_(std::move(font)) {
    context_ = ImGui::CreateContext();
    current();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::GetIO().LogFilename = nullptr;
    console_ = std::make_unique<OverlayConsole>();
    console_->System().RegisterCommand(
        "echo", "Print a message",
        [this](const csys::String& value) { messages_->append(value.m_String); },
        csys::Arg<csys::String>("message"));
    messages_->append(
        "RoadWars debug console ready. ` / F8 toggles mouse capture. Type help for commands.");
}
DebugOverlay::~DebugOverlay() {
    current();
    console_.reset();
    ImGui::DestroyContext(context_);
}
void DebugOverlay::current() const { ImGui::SetCurrentContext(context_); }
ImDrawData* DebugOverlay::frame(float width, float height, float delta, float dpi,
                                std::uint64_t, bool interactive) {
    current();
    auto& io = ImGui::GetIO();
    // A high-DPI desktop can host a small pixel viewport. Keep the tools usable
    // there while preserving the reported scale in a correspondingly large view.
    dpi = std::clamp(std::min({dpi, width / 960.0f, height / 540.0f}), 0.75f, 4.0f);
    if (std::abs(dpi - scale_) > 0.01f) {
        if (scale_ > 0) {
            const float ratio = dpi / scale_;
            for (auto window : context_->Windows) {
                window->Pos.x *= ratio;
                window->Pos.y *= ratio;
                window->SizeFull.x *= ratio;
                window->SizeFull.y *= ratio;
            }
        }
        scale_ = dpi;
        ImGui::GetStyle() = ImGuiStyle{};
        ImGui::StyleColorsDark();
        ImGui::GetStyle().ScaleAllSizes(scale_);
        io.Fonts->Clear();
        ImFontConfig config;
        config.SizePixels = std::floor(16.0f * scale_);
        if (font_.empty())
            io.Fonts->AddFontDefault(&config);
        else {
            config.FontDataOwnedByAtlas = false;
            io.Fonts->AddFontFromMemoryTTF(font_.data(), static_cast<int>(font_.size()),
                                           config.SizePixels, &config);
        }
        io.Fonts->Build();
        io.Fonts->SetTexID(reinterpret_cast<ImTextureID>(1));
        ++font_revision_;
    }
    io.DisplaySize = ImVec2(width, height);
    io.DeltaTime = std::isfinite(delta) && delta > 0 ? delta : 1.0f / 60.0f;
    if (!interactive)
        io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
    for (const auto& message : messages_->drain())
        console_->append(message);
    auto& items = console_->System().Items();
    if (items.size() > 4096)
        items.erase(items.begin(), items.begin() + (items.size() - 4096));
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(width - 12 * scale_, 12 * scale_), ImGuiCond_Always,
                            ImVec2(1, 0));
    ImGui::SetNextWindowBgAlpha(0.78f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6 * scale_, 4 * scale_));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4 * scale_, 2 * scale_));
    ImGui::Begin("Render profile", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                     ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoFocusOnAppearing);
    const float target_ms = refresh_hz_ > 0 ? 1000.0f / refresh_hz_ : 0;
    const auto timing = frame_timing_.summary(target_ms);
    const float latest_ms = frame_timing_.count()
        ? frame_timing_.data()[(frame_timing_.offset() + frame_timing_.count() - 1) %
                               FrameTiming::kCapacity] : 0;
    ImGui::Text("%.0f FPS   %.2f ms", timing.mean > 0 ? 1000.0f / timing.mean : 0,
                latest_ms);
    const float ceiling = std::max({33.333f, target_ms * 2, timing.maximum * 1.1f});
    ImGui::Dummy(ImVec2(250 * scale_, 36 * scale_));
    const auto graph_min = ImGui::GetItemRectMin();
    const auto graph_max = ImGui::GetItemRectMax();
    auto* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(graph_min, graph_max, ImGui::GetColorU32(ImGuiCol_FrameBg));
    // Plot every sample, even when there are more samples than horizontal pixels.
    // PlotLines' display-width sampling can skip the very spikes we are diagnosing.
    std::array<ImVec2, FrameTiming::kCapacity> points;
    const auto padding = ImGui::GetStyle().FramePadding;
    for (int i = 0; i < frame_timing_.count(); ++i) {
        const float value = frame_timing_.data()[
            (frame_timing_.offset() + i) % FrameTiming::kCapacity];
        const float x = graph_min.x + padding.x +
                        (graph_max.x - graph_min.x - 2 * padding.x) * i /
                            std::max(1, frame_timing_.count() - 1);
        const float y = graph_max.y - padding.y -
                        (graph_max.y - graph_min.y - 2 * padding.y) * value / ceiling;
        points[i] = ImVec2(x, y);
    }
    draw->AddPolyline(points.data(), frame_timing_.count(), IM_COL32(255, 203, 90, 255), 0,
                       std::max(1.0f, scale_));
    if (target_ms > 0) {
        const float y = graph_max.y - padding.y -
                        (graph_max.y - graph_min.y - 2 * padding.y) * target_ms / ceiling;
        ImGui::GetWindowDrawList()->AddLine(ImVec2(graph_min.x, y), ImVec2(graph_max.x, y),
                                            IM_COL32(90, 220, 140, 255));
    }
    ImGui::Text("P99 %.2f   Max %.2f ms", timing.p99, timing.maximum);
    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::SetNextWindowPos(ImVec2(12 * scale_, 12 * scale_), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(
        ImVec2(std::min(
                   700 * scale_,
                   std::max(240 * scale_,
                            width - ImGui::CalcTextSize("Frame time   999.99 ms").x - 60 * scale_)),
               std::min(380 * scale_, height - 24 * scale_)),
        ImGuiCond_FirstUseEver);
    if (collapse_requested_) {
        ImGui::SetNextWindowCollapsed(collapse_value_, ImGuiCond_Always);
        collapse_requested_ = false;
    }
    console_->Draw();
    draw_race_debug(width, height);
    if (auto profile = ImGui::FindWindowByName("Render profile"))
        ImGui::BringWindowToDisplayFront(profile);
    ImGui::Render();
    return ImGui::GetDrawData();
}
void DebugOverlay::mouse_position(float x, float y) {
    current();
    ImGui::GetIO().AddMousePosEvent(x, y);
}
void DebugOverlay::mouse_button(int button, bool down) {
    current();
    ImGui::GetIO().AddMouseButtonEvent(button, down);
}
void DebugOverlay::mouse_wheel(float x, float y) {
    current();
    ImGui::GetIO().AddMouseWheelEvent(x, y);
}
void DebugOverlay::key(int key, bool down) {
    current();
    if (key)
        ImGui::GetIO().AddKeyEvent(static_cast<ImGuiKey>(key), down);
}
void DebugOverlay::text(std::uint32_t character) {
    current();
    if (character)
        ImGui::GetIO().AddInputCharacter(character);
}
void DebugOverlay::focus(bool focused) {
    current();
    ImGui::GetIO().AddFocusEvent(focused);
}
bool DebugOverlay::hit_test(float x, float y) const {
    current();
    for (const auto window : context_->Windows)
        if (window->WasActive && !(window->Flags & ImGuiWindowFlags_NoInputs) &&
            window->OuterRectClipped.Contains(ImVec2(x, y)))
            return true;
    return false;
}
void DebugOverlay::set_collapsed(bool value) {
    collapse_requested_ = true;
    collapse_value_ = value;
}
bool DebugOverlay::collapsed() const {
    current();
    const auto window = ImGui::FindWindowByName("Console");
    return window ? window->Collapsed : collapse_value_;
}
std::array<float, 2> DebugOverlay::console_input_position() const {
    current();
    const auto window = ImGui::FindWindowByName("Console");
    if (!window)
        return {};
    return {window->Pos.x + ImGui::GetStyle().WindowPadding.x + 20 * scale_,
            window->Pos.y + window->SizeFull.y - ImGui::GetStyle().WindowPadding.y -
                ImGui::GetFrameHeight() * 0.5f};
}
std::array<float, 2> DebugOverlay::race_track_position(int index) const {
    return index >= 0 && static_cast<std::size_t>(index) < race_track_positions_.size()
               ? race_track_positions_[static_cast<std::size_t>(index)]
               : std::array<float, 2>{};
}
const unsigned char* DebugOverlay::font_pixels(int& width, int& height) {
    current();
    unsigned char* pixels;
    ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    return pixels;
}
} // namespace roadwars::extensions
