#include "platform/godot/extension/frame_trace.hpp"
#include "platform/godot/extension/debug_overlay.hpp"
#include "game_src/core/not_implemented.hpp"
#include <algorithm>
#include <cstdio>
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <imgui.h>

namespace roadwars {
void OverlayLogger::_log_message(const godot::String& message, bool error) {
    try {
        if (messages)
            messages->append(message.utf8().get_data(), error);
    } catch (...) {
    }
}
void OverlayLogger::_log_error(
    const godot::String& function, const godot::String& file, int32_t line,
    const godot::String& code, const godot::String& rationale, bool, int32_t,
    const godot::TypedArray<godot::Ref<godot::ScriptBacktrace>>& backtraces) {
    try {
        auto text = std::string(code.utf8().get_data()) + " " + rationale.utf8().get_data() +
                    "\n  at " + file.utf8().get_data() + ":" + std::to_string(line) + " (" +
                    function.utf8().get_data() + ")";
        for (int64_t index = 0; index < backtraces.size(); ++index) {
            const godot::Ref<godot::ScriptBacktrace> trace = backtraces[index];
            if (trace.is_valid())
                text += "\n" + std::string(trace->format().utf8().get_data());
        }
        if (messages)
            messages->append(text, true);
    } catch (...) {
    }
}
namespace {
std::atomic<std::shared_ptr<extensions::DebugMessages>> diagnostic_messages;
void capture_diagnostic(const char* message) noexcept {
    try {
        if (const auto messages = diagnostic_messages.load())
            messages->append(message, true);
    } catch (...) {
    }
}
ImGuiKey imgui_key(godot::Key key) {
    const auto value = static_cast<int>(key);
    if (value >= godot::KEY_A && value <= godot::KEY_Z)
        return static_cast<ImGuiKey>(ImGuiKey_A + value - godot::KEY_A);
    if (value >= godot::KEY_0 && value <= godot::KEY_9)
        return static_cast<ImGuiKey>(ImGuiKey_0 + value - godot::KEY_0);
    switch (key) {
    case godot::KEY_TAB:
        return ImGuiKey_Tab;
    case godot::KEY_LEFT:
        return ImGuiKey_LeftArrow;
    case godot::KEY_RIGHT:
        return ImGuiKey_RightArrow;
    case godot::KEY_UP:
        return ImGuiKey_UpArrow;
    case godot::KEY_DOWN:
        return ImGuiKey_DownArrow;
    case godot::KEY_PAGEUP:
        return ImGuiKey_PageUp;
    case godot::KEY_PAGEDOWN:
        return ImGuiKey_PageDown;
    case godot::KEY_HOME:
        return ImGuiKey_Home;
    case godot::KEY_END:
        return ImGuiKey_End;
    case godot::KEY_INSERT:
        return ImGuiKey_Insert;
    case godot::KEY_DELETE:
        return ImGuiKey_Delete;
    case godot::KEY_BACKSPACE:
        return ImGuiKey_Backspace;
    case godot::KEY_SPACE:
        return ImGuiKey_Space;
    case godot::KEY_ENTER:
    case godot::KEY_KP_ENTER:
        return ImGuiKey_Enter;
    case godot::KEY_ESCAPE:
        return ImGuiKey_Escape;
    default:
        return ImGuiKey_None;
    }
}
} // namespace
DebugOverlayAdapter::DebugOverlayAdapter(godot::Viewport* viewport)
    : viewport_(viewport), messages_(std::make_shared<extensions::DebugMessages>()) {
    logger_.instantiate();
    logger_->messages = messages_;
    godot::OS::get_singleton()->add_logger(logger_);
    streams_ = std::make_unique<extensions::ConsoleStreams>(messages_);
    std::vector<unsigned char> font;
    const auto path = godot::OS::get_singleton()->get_system_font_path("Consolas");
    if (!path.is_empty()) {
        const auto bytes = godot::FileAccess::get_file_as_bytes(path);
        if (bytes.size())
            font.assign(bytes.ptr(), bytes.ptr() + bytes.size());
    }
    ui_ = std::make_unique<extensions::DebugOverlay>(messages_, std::move(font));
    diagnostic_messages.store(messages_);
    diagnostic_handler.store(capture_diagnostic);
    auto server = godot::RenderingServer::get_singleton();
    canvas_ = server->canvas_create();
    server->viewport_attach_canvas(viewport_->get_viewport_rid(), canvas_);
    server->viewport_set_canvas_stacking(viewport_->get_viewport_rid(), canvas_, 1000, 0);
}
DebugOverlayAdapter::~DebugOverlayAdapter() {
    diagnostic_handler.store(nullptr);
    diagnostic_messages.store(nullptr);
    godot::OS::get_singleton()->remove_logger(logger_);
    streams_.reset();
    auto server = godot::RenderingServer::get_singleton();
    for (const auto& item : draw_items_) {
        server->free_rid(item.triangles);
        server->free_rid(item.clip);
    }
    if (font_.is_valid())
        server->free_rid(font_);
    server->viewport_remove_canvas(viewport_->get_viewport_rid(), canvas_);
    server->free_rid(canvas_);
}
void DebugOverlayAdapter::begin_frame(std::uint64_t now) {
    const auto frame = godot::Engine::get_singleton()->get_process_frames();
    // Test helpers may draw the overlay repeatedly in a single engine frame.
    // Record only real callback intervals, without time_scale or process-delta clamps.
    if (frame != timing_frame_) {
        if (timing_usec_ && now > timing_usec_)
            ui_->record_frame_interval(static_cast<float>(now - timing_usec_) / 1000.0f);
        timing_usec_ = now;
        timing_frame_ = frame;
    }
}
void DebugOverlayAdapter::frame(double delta, bool interactive) {
    auto display = godot::DisplayServer::get_singleton();
    const int screen = display->window_get_current_screen();
    const auto now = godot::Time::get_singleton()->get_ticks_usec();
    if (!display_poll_usec_ || now - display_poll_usec_ >= 1000000) {
        FrameStageTimer timer(FrameStage::display_poll);
        float refresh = 0;
        std::string vsync = "Unavailable (headless)";
        if (display->get_name() != "headless") {
            refresh = display->screen_get_refresh_rate(screen);
            switch (display->window_get_vsync_mode()) {
            case godot::DisplayServer::VSYNC_DISABLED: vsync = "Disabled"; break;
            case godot::DisplayServer::VSYNC_ENABLED: vsync = "Enabled"; break;
            case godot::DisplayServer::VSYNC_ADAPTIVE: vsync = "Adaptive"; break;
            case godot::DisplayServer::VSYNC_MAILBOX: vsync = "Mailbox"; break;
            default: vsync = "Unknown"; break;
            }
        }
        ui_->set_display_timing(refresh, std::move(vsync));
        display_poll_usec_ = now;
    }
    float dpi;
    {
        FrameStageTimer timer(FrameStage::dpi_poll);
        dpi = std::max(static_cast<float>(display->screen_get_scale(screen)),
                       display->screen_get_dpi(screen) / 96.0f);
    }
    auto server = godot::RenderingServer::get_singleton();
    triangles_ = static_cast<std::uint64_t>(
        std::max(0, server->viewport_get_render_info(
                        viewport_->get_viewport_rid(),
                        godot::RenderingServer::VIEWPORT_RENDER_INFO_TYPE_VISIBLE,
                        godot::RenderingServer::VIEWPORT_RENDER_INFO_PRIMITIVES_IN_FRAME)));
    const auto size = viewport_->get_visible_rect().size;
    auto data = ui_->frame(size.x, size.y, static_cast<float>(delta), dpi, triangles_, interactive);
    render(data);
}
void DebugOverlayAdapter::render(ImDrawData* data) {
    auto server = godot::RenderingServer::get_singleton();
    if (font_revision_ != ui_->font_revision()) {
        int width, height;
        const auto pixels = ui_->font_pixels(width, height);
        godot::PackedByteArray bytes;
        bytes.resize(width * height * 4);
        std::copy_n(pixels, bytes.size(), bytes.ptrw());
        auto image =
            godot::Image::create_from_data(width, height, false, godot::Image::FORMAT_RGBA8, bytes);
        if (font_.is_valid())
            server->free_rid(font_);
        font_ = server->texture_2d_create(image);
        font_revision_ = ui_->font_revision();
    }
    std::size_t used = 0;
    for (int list_index = 0; list_index < data->CmdListsCount; ++list_index) {
        const auto list = data->CmdLists[list_index];
        for (const auto& command : list->CmdBuffer) {
            if (command.UserCallback) {
                if (command.UserCallback != ImDrawCallback_ResetRenderState)
                    command.UserCallback(list, &command);
                continue;
            }
            if (!command.ElemCount)
                continue;
            if (used == draw_items_.size()) {
                trace_resource(FrameResource::ui_item);
                DrawItem item{server->canvas_item_create(), server->canvas_item_create()};
                server->canvas_item_set_parent(item.clip, canvas_);
                server->canvas_item_set_parent(item.triangles, item.clip);
                server->canvas_item_set_clip(item.clip, true);
                draw_items_.push_back(item);
            }
            const auto& item = draw_items_[used++];
            server->canvas_item_set_draw_index(item.clip, static_cast<int>(used - 1));
            server->canvas_item_set_visible(item.clip, true);
            server->canvas_item_clear(item.triangles);
            server->canvas_item_set_custom_rect(
                item.clip, true,
                godot::Rect2(command.ClipRect.x, command.ClipRect.y,
                             command.ClipRect.z - command.ClipRect.x,
                             command.ClipRect.w - command.ClipRect.y));
            godot::PackedVector2Array points, uvs;
            godot::PackedColorArray colors;
            godot::PackedInt32Array indices;
            points.resize(list->VtxBuffer.Size);
            uvs.resize(list->VtxBuffer.Size);
            colors.resize(list->VtxBuffer.Size);
            for (int i = 0; i < list->VtxBuffer.Size; ++i) {
                const auto& vertex = list->VtxBuffer[i];
                const auto color = vertex.col;
                points.set(i, godot::Vector2(vertex.pos.x, vertex.pos.y));
                uvs.set(i, godot::Vector2(vertex.uv.x, vertex.uv.y));
                colors.set(i, godot::Color((color & 255) / 255.0f, ((color >> 8) & 255) / 255.0f,
                                           ((color >> 16) & 255) / 255.0f,
                                           ((color >> 24) & 255) / 255.0f));
            }
            indices.resize(command.ElemCount);
            for (unsigned i = 0; i < command.ElemCount; ++i)
                indices.set(i, list->IdxBuffer[command.IdxOffset + i] + command.VtxOffset);
            server->canvas_item_add_triangle_array(item.triangles, indices, points, colors, uvs, {},
                                                   {}, font_);
        }
    }
    for (; used < draw_items_.size(); ++used)
        server->canvas_item_set_visible(draw_items_[used].clip, false);
}
bool DebugOverlayAdapter::input(const godot::Ref<godot::InputEvent>& event, bool interactive) {
    if (!interactive)
        return false;
    const godot::Ref<godot::InputEventMouseMotion> motion = event;
    if (motion.is_valid()) {
        const auto p = motion->get_position();
        ui_->mouse_position(p.x, p.y);
        return ui_->hit_test(p.x, p.y);
    }
    const godot::Ref<godot::InputEventMouseButton> button = event;
    if (button.is_valid()) {
        const auto p = button->get_position();
        ui_->mouse_position(p.x, p.y);
        const auto index = button->get_button_index();
        if (index >= godot::MOUSE_BUTTON_WHEEL_UP && index <= godot::MOUSE_BUTTON_WHEEL_RIGHT) {
            if (button->is_pressed())
                ui_->mouse_wheel(index == godot::MOUSE_BUTTON_WHEEL_LEFT    ? 1
                                 : index == godot::MOUSE_BUTTON_WHEEL_RIGHT ? -1
                                                                            : 0,
                                 index == godot::MOUSE_BUTTON_WHEEL_UP     ? 1
                                 : index == godot::MOUSE_BUTTON_WHEEL_DOWN ? -1
                                                                           : 0);
        } else {
            const int mapped = index == godot::MOUSE_BUTTON_LEFT     ? 0
                               : index == godot::MOUSE_BUTTON_RIGHT  ? 1
                               : index == godot::MOUSE_BUTTON_MIDDLE ? 2
                                                                     : -1;
            if (mapped >= 0)
                ui_->mouse_button(mapped, button->is_pressed());
        }
        return ui_->hit_test(p.x, p.y);
    }
    const godot::Ref<godot::InputEventKey> key = event;
    if (key.is_valid()) {
        ui_->key(ImGuiMod_Ctrl, key->is_ctrl_pressed());
        ui_->key(ImGuiMod_Shift, key->is_shift_pressed());
        ui_->key(ImGuiMod_Alt, key->is_alt_pressed());
        ui_->key(ImGuiMod_Super, key->is_meta_pressed());
        ui_->key(imgui_key(key->get_keycode()), key->is_pressed());
        if (key->is_pressed() && !key->is_ctrl_pressed() && !key->is_meta_pressed())
            ui_->text(key->get_unicode());
        return true;
    }
    return false;
}
void DebugOverlayAdapter::focus(bool focused) { ui_->focus(focused); }
void DebugOverlayAdapter::show_error(const char* message) {
    messages_->append(message, true);
    ui_->set_collapsed(false);
}
int DebugOverlayAdapter::live_rids() const {
    return 1 + (font_.is_valid() ? 1 : 0) + static_cast<int>(draw_items_.size() * 2);
}
} // namespace roadwars
