#pragma once
#include <array>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

struct ImDrawData;
struct ImGuiContext;

namespace roadwars::extensions {
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

  private:
    void current() const;
    std::shared_ptr<DebugMessages> messages_;
    ImGuiContext* context_{};
    std::vector<unsigned char> font_;
    std::unique_ptr<OverlayConsole> console_;
    float scale_{};
    std::uint64_t font_revision_{};
    bool collapse_requested_{true}, collapse_value_{true};
};
} // namespace roadwars::extensions
