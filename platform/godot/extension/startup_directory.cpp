#include "platform/godot/extension/game_host.hpp"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <exception>

namespace roadwars {
namespace {
godot::String absolute_directory(godot::String path) {
    path = path.replace("\\", "/");
    if (!path.is_absolute_path()) {
        path = current_game_directory().path_join(path);
    }
    return path.simplify_path();
}
}
bool GameHost::resolve_startup_directory() {
    godot::String error;
    if (!game_directory_.is_empty()) {
        game_directory_ = absolute_directory(game_directory_);
        if (validate_game_directory(game_directory_, error))
            return true;
        show_startup_prompt("Invalid --game-dir:\n" + error);
        return false;
    }
    if (developer_build_) {
        const godot::String configs[] = {
            "res://../../../config.local.json",
            godot::OS::get_singleton()->get_executable_path().get_base_dir().path_join("config.local.json")};
        for (const auto& path : configs) {
            if (!godot::FileAccess::file_exists(path))
                continue;
            const auto value = godot::JSON::parse_string(godot::FileAccess::get_file_as_string(path));
            if (value.get_type() != godot::Variant::DICTIONARY)
                continue;
            const godot::Dictionary config = value;
            const godot::Variant root = config.get("game_dir", "");
            if (root.get_type() != godot::Variant::STRING || godot::String(root).is_empty())
                continue;
            auto candidate = absolute_directory(root);
            if (validate_game_directory(candidate, error)) {
                game_directory_ = candidate;
                return true;
            }
        }
    }
    game_directory_ = detect_game_directory();
    if (!game_directory_.is_empty())
        return true;
    show_startup_prompt("Original game assets were not found.\nSelect the original Road Wars game directory.");
    return false;
}
void GameHost::show_startup_prompt(godot::String error) {
    startup_waiting_ = true;
    startup_error_ = error;
    game_directory_ = "";
    last_simulation_usec_ = 0;
    if (godot::DisplayServer::get_singleton()->get_name() == "headless") {
        godot::UtilityFunctions::push_error(error);
        get_tree()->quit(1);
        return;
    }
    if (!startup_prompt_) {
        startup_prompt_ = memnew(godot::AcceptDialog);
        startup_prompt_->set_title("Road Wars - Game directory");
        startup_prompt_->get_ok_button()->set_text("Exit");
        startup_prompt_->add_button("Browse...", true, "browse");
        startup_prompt_->connect("custom_action", godot::Callable(this, "browse_game_directory").unbind(1));
        startup_prompt_->connect("confirmed", godot::Callable(this, "quit_startup"));
        startup_prompt_->connect("canceled", godot::Callable(this, "quit_startup"));
        add_child(startup_prompt_);
    }
    startup_prompt_->set_text(error);
    startup_prompt_->popup_centered(godot::Vector2i(640, 240));
}
void GameHost::browse_game_directory() {
    if (!startup_waiting_)
        return;
    if (!startup_browser_) {
        startup_browser_ = memnew(godot::FileDialog);
        startup_browser_->set_title("Select the original Road Wars game directory");
        startup_browser_->set_file_mode(godot::FileDialog::FILE_MODE_OPEN_DIR);
        startup_browser_->set_access(godot::FileDialog::ACCESS_FILESYSTEM);
        startup_browser_->set_use_native_dialog(true);
        startup_browser_->connect("dir_selected", godot::Callable(this, "select_game_directory"));
        startup_browser_->connect("canceled", godot::Callable(this, "cancel_game_directory"));
        add_child(startup_browser_);
    }
    startup_prompt_->hide();
    startup_browser_->popup_centered_ratio(0.8);
}
void GameHost::cancel_game_directory() {
    if (startup_waiting_)
        show_startup_prompt(startup_error_);
}
void GameHost::select_game_directory(godot::String path) {
    if (!startup_waiting_)
        return;
    try {
        const auto candidate = absolute_directory(path);
        godot::String error;
        if (!validate_game_directory(candidate, error)) {
            show_startup_prompt("Invalid game directory:\n" + error);
            return;
        }
        game_directory_ = candidate;
        if (startup_browser_)
            startup_browser_->hide();
        if (startup_prompt_)
            startup_prompt_->hide();
        finish_startup();
    } catch (const std::exception& error) {
        show_startup_prompt(error.what());
    }
}
void GameHost::finish_startup() {
    startup_waiting_ = false;
    startup_error_ = "";
    last_simulation_usec_ = godot::Time::get_singleton()->get_ticks_usec();
    godot::UtilityFunctions::print("ROADWARS_ASSETS_READY: ", game_directory_);
    restart_smoke_scene();
}
void GameHost::update_window_title(double delta) {
    title_elapsed_ += delta;
    if (title_elapsed_ < 0.5)
        return;
    title_elapsed_ = 0;
    auto driver = godot::RenderingServer::get_singleton()->get_current_rendering_driver_name();
    if (driver == "vulkan") driver = "Vulkan";
    else if (driver.begins_with("opengl")) driver = "OpenGL";
    else if (driver == "metal") driver = "Metal";
    else if (driver == "d3d12") driver = "Direct3D 12";
    if (driver.is_empty()) driver = "Headless";
    const auto title = "RoadWars-RE (" + driver + ") FPS: " +
        godot::String::num_int64(godot::Engine::get_singleton()->get_frames_per_second()) +
        (developer_build_ ? " - Debug" : " - Release") + " | By Nurupo";
    if (godot::DisplayServer::get_singleton()->get_name() != "headless")
        get_window()->set_title(title);
}
void GameHost::quit_startup() { get_tree()->quit(); }
godot::Dictionary GameHost::get_startup_state() const {
    godot::Dictionary result;
    result["cwd"] = current_game_directory();
    result["waiting"] = startup_waiting_;
    result["root"] = game_directory_;
    result["error"] = startup_error_;
    result["initialized"] = entry_ != nullptr;
    return result;
}
godot::Dictionary GameHost::validate_game_directory_for_test(godot::String path) const {
    godot::Dictionary result;
    godot::String error;
    result["valid"] = validate_game_directory(absolute_directory(path), error);
    result["error"] = error;
    return result;
}
} // namespace roadwars
