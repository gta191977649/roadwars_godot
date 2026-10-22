#pragma once
#include "game_src/interfaces/race_debug.hpp"
#include <array>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

struct ImDrawData;
struct ImGuiContext;

namespace roadwars::extensions {
// Diagnostic projection only; no output feeds back into the simulation.
bool project_race_debug_line(const RaceDebugCamera&, Vec3 from, Vec3 to, float width, float height,
                             std::array<float, 2>& screen_from, std::array<float, 2>& screen_to);
float race_debug_distance_alpha(Vec3 point, Vec3 eye, float distance);
bool clip_race_debug_distance(Vec3& from, Vec3& to, Vec3 eye, float distance);
class OverlayConsole;
struct DebugMessage {
    std::string text;
    bool error{};
};
class DebugMessages {
  public:
    void append(std::string_view text, bool error = false);
    std::deque<DebugMessage> drain();
    bool contains(std::string_view text) const;
    std::uint64_t count() const;

  private:
    mutable std::mutex mutex_;
    std::deque<DebugMessage> pending_, history_;
    std::uint64_t count_{};
};

// New diagnostic UI, independent of original game behavior and host APIs.
class DebugOverlay {
  public:
    explicit DebugOverlay(std::shared_ptr<DebugMessages> messages,
                          std::vector<unsigned char> font = {});
    ~DebugOverlay();
    DebugOverlay(const DebugOverlay&) = delete;
    DebugOverlay& operator=(const DebugOverlay&) = delete;
    ImDrawData* frame(float width, float height, float delta, float dpi_scale,
                      std::uint64_t triangles, bool interactive);
    void mouse_position(float x, float y);
    void mouse_button(int button, bool down);
    void mouse_wheel(float x, float y);
    void key(int imgui_key, bool down);
    void text(std::uint32_t character);
    void focus(bool focused);
    bool hit_test(float x, float y) const;
    void set_collapsed(bool collapsed);
    bool collapsed() const;
    std::array<float, 2> console_input_position() const;
    float scale() const { return scale_; }
    const unsigned char* font_pixels(int& width, int& height);
    std::uint64_t font_revision() const { return font_revision_; }
    void set_race_debug(RaceDebugSnapshot snapshot) { race_debug_ = std::move(snapshot); }
    void clear_race_debug() { race_debug_.reset(); }
    RaceDebugOptions& race_options() { return race_options_; }
    const RaceDebugOptions& race_options() const { return race_options_; }
    bool has_race_debug() const { return race_debug_.has_value(); }
    std::size_t race_debug_line_count() const { return race_debug_line_count_; }
    std::array<float, 2> race_checkbox_position(int index) const;
    std::array<float, 2> race_inspect_position(int index) const;

  private:
    void current() const;
    void draw_race_debug(float width, float height);
    std::shared_ptr<DebugMessages> messages_;
    ImGuiContext* context_{};
    std::vector<unsigned char> font_;
    std::unique_ptr<OverlayConsole> console_;
    float scale_{};
    std::uint64_t font_revision_{};
    bool collapse_requested_{true}, collapse_value_{true};
    std::optional<RaceDebugSnapshot> race_debug_;
    RaceDebugOptions race_options_;
    std::array<std::array<float, 2>, 5> race_checkbox_positions_{};
    std::array<std::array<float, 2>, 7> race_inspect_positions_{};
    std::size_t race_debug_line_count_{};
};
} // namespace roadwars::extensions
