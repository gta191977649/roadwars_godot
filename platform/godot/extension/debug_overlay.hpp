#pragma once
#include "game_src/extensions/console_streams.hpp"
#include "game_src/extensions/debug_overlay.hpp"
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/logger.hpp>
#include <godot_cpp/classes/script_backtrace.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <memory>
#include <vector>

namespace roadwars {
class OverlayLogger : public godot::Logger {
    GDCLASS(OverlayLogger, godot::Logger)
  protected:
    static void _bind_methods() {}

  public:
    std::shared_ptr<extensions::DebugMessages> messages;
    void _log_message(const godot::String& message, bool error) override;
    void
    _log_error(const godot::String& function, const godot::String& file, int32_t line,
               const godot::String& code, const godot::String& rationale, bool editor_notify,
               int32_t error_type,
               const godot::TypedArray<godot::Ref<godot::ScriptBacktrace>>& backtraces) override;
};
class DebugOverlayAdapter {
  public:
    explicit DebugOverlayAdapter(godot::Viewport* viewport);
    ~DebugOverlayAdapter();
    void frame(double delta, bool interactive);
    bool input(const godot::Ref<godot::InputEvent>& event, bool interactive);
    void focus(bool focused);
    void show_error(const char* message);
    extensions::DebugOverlay& ui() { return *ui_; }
    const std::shared_ptr<extensions::DebugMessages>& messages() const { return messages_; }
    std::uint64_t triangles() const { return triangles_; }
    int live_rids() const;

  private:
    void render(ImDrawData* data);
    godot::Viewport* viewport_;
    std::shared_ptr<extensions::DebugMessages> messages_;
    godot::Ref<OverlayLogger> logger_;
    std::unique_ptr<extensions::ConsoleStreams> streams_;
    std::unique_ptr<extensions::DebugOverlay> ui_;
    godot::RID canvas_, font_;
    struct DrawItem {
        godot::RID clip, triangles;
    };
    std::vector<DrawItem> draw_items_;
    std::uint64_t font_revision_{}, triangles_{};
};
} // namespace roadwars
