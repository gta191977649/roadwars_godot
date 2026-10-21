#include "platform/godot/extension/game_host.hpp"
#include "game_src/core/not_implemented.hpp"
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <exception>

namespace roadwars {
void GameHost::_bind_methods() {
    godot::ClassDB::bind_method(godot::D_METHOD("restart_smoke_scene"), &GameHost::restart_smoke_scene);
    godot::ClassDB::bind_method(godot::D_METHOD("get_live_rids"), &GameHost::get_live_rids);
}
void GameHost::_ready() {
    for (const auto argument : godot::OS::get_singleton()->get_cmdline_user_args()) {
        if (argument == "--original-loop") original_loop_ = true;
    }
    restart_smoke_scene();
}
void GameHost::restart_smoke_scene() {
    try {
        entry_.reset();
        runtime_.reset();
        renderer_.reset();
        renderer_ = std::make_unique<ServerRenderer>(get_viewport()->get_viewport_rid());
        runtime_ = std::make_unique<Runtime>(*renderer_);
        if (runtime_->start_smoke_scene() != Result::ok) throw std::runtime_error("Smoke scene initialization failed");
        entry_ = std::make_unique<GameEntry>(*this);
        if (entry_->start() != Result::ok) throw std::runtime_error("Game entry failed");
    } catch (const std::exception& error) {
        entry_.reset(); runtime_.reset(); renderer_.reset();
        godot::UtilityFunctions::push_error(error.what());
        get_tree()->quit(1);
    } catch (...) {
        entry_.reset(); runtime_.reset(); renderer_.reset();
        godot::UtilityFunctions::push_error("Unknown host initialization failure");
        get_tree()->quit(1);
    }
}
void GameHost::_process(double delta) {
    try {
        if (!entry_) return;
        InputState input;
        input.quit_requested = godot::Input::get_singleton()->is_action_just_pressed("ui_cancel");
        if (entry_->advance(delta, input) != Result::ok) {
            godot::UtilityFunctions::push_error("Runtime update failed"); get_tree()->quit(1);
        }
        if (input.quit_requested) get_tree()->quit();
    } catch (...) {
        godot::UtilityFunctions::push_error("Host update exception"); get_tree()->quit(1);
    }
}
void GameHost::_exit_tree() { entry_.reset(); runtime_.reset(); renderer_.reset(); }
Result GameHost::invoke(LoopStage stage, float delta, const InputState& input, LoopState&) {
    if (original_loop_) {
        const auto id = godot::String("93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:") + GameEntry::original_address(stage);
        const auto text = id.utf8();
        not_implemented(text.get_data(), GameEntry::original_address(stage));
    }
    // Explicit diagnostic mode: exercise scheduling and rendering, not original logic.
    if (stage == LoopStage::single_race_004a4f57) return runtime_->update(delta, input);
    return Result::ok;
}
int GameHost::get_live_rids() const { return renderer_ ? static_cast<int>(renderer_->live_rids()) : 0; }
} // namespace roadwars
