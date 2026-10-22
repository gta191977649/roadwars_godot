#include "platform/godot/extension/game_host.hpp"
#include "game_src/core/not_implemented.hpp"
#include "platform/godot/extension/frontend_video_audio.hpp"
#include <cmath>
#include <cstdio>
#include <exception>
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <iostream>

namespace roadwars {
void GameHost::_bind_methods() {
    godot::ClassDB::bind_method(godot::D_METHOD("competition_test_step", "delta", "autopilot"),
                                &GameHost::competition_test_step);
    godot::ClassDB::bind_method(godot::D_METHOD("get_competition_state"),
                                &GameHost::get_competition_state);
    godot::ClassDB::bind_method(godot::D_METHOD("get_race_debug_inspect_position", "index"),
                                &GameHost::get_race_debug_inspect_position);
    godot::ClassDB::bind_method(godot::D_METHOD("get_race_debug_checkbox_position", "index"),
                                &GameHost::get_race_debug_checkbox_position);
    godot::ClassDB::bind_method(godot::D_METHOD("get_race_debug_line_count"),
                                &GameHost::get_race_debug_line_count);
    godot::ClassDB::bind_method(godot::D_METHOD("get_frontend_screen"),
                                &GameHost::get_frontend_screen);
    godot::ClassDB::bind_method(godot::D_METHOD("get_frontend_action"),
                                &GameHost::get_frontend_action);
    godot::ClassDB::bind_method(
        godot::D_METHOD("preview_frontend_screen", "id", "seconds", "variant"),
        &GameHost::preview_frontend_screen);
    godot::ClassDB::bind_method(godot::D_METHOD("frontend_test_step", "elapsed", "command"),
                                &GameHost::frontend_test_step);
    godot::ClassDB::bind_method(godot::D_METHOD("is_race_music_playing"),
                                &GameHost::is_race_music_playing);
    godot::ClassDB::bind_method(godot::D_METHOD("get_race_music_position"),
                                &GameHost::get_race_music_position);
    godot::ClassDB::bind_method(godot::D_METHOD("get_race_music_gain"),
                                &GameHost::get_race_music_gain);
    godot::ClassDB::bind_method(godot::D_METHOD("get_race_music_duration"),
                                &GameHost::get_race_music_duration);
    godot::ClassDB::bind_method(godot::D_METHOD("seek_race_music_for_test", "seconds"),
                                &GameHost::seek_race_music_for_test);
    godot::ClassDB::bind_method(godot::D_METHOD("start_billboard_fixture"),
                                &GameHost::start_billboard_fixture);
    godot::ClassDB::bind_method(godot::D_METHOD("set_billboard_fixture_camera", "heading", "pitch"),
                                &GameHost::set_billboard_fixture_camera);
    godot::ClassDB::bind_method(godot::D_METHOD("set_frontend_visible", "visible"),
                                &GameHost::set_frontend_visible);
    godot::ClassDB::bind_method(godot::D_METHOD("get_frontend_player_position"),
                                &GameHost::get_frontend_player_position);
    godot::ClassDB::bind_method(godot::D_METHOD("get_simulation_ticks"),
                                &GameHost::get_simulation_ticks);
    godot::ClassDB::bind_method(godot::D_METHOD("get_simulation_alpha"),
                                &GameHost::get_simulation_alpha);
    godot::ClassDB::bind_method(godot::D_METHOD("present_test_frame"),
                                &GameHost::present_test_frame);
    godot::ClassDB::bind_method(godot::D_METHOD("get_console_input_position"),
                                &GameHost::get_console_input_position);
    godot::ClassDB::bind_method(godot::D_METHOD("get_console_collapsed"),
                                &GameHost::get_console_collapsed);
    godot::ClassDB::bind_method(godot::D_METHOD("console_contains", "text"),
                                &GameHost::console_contains);
    godot::ClassDB::bind_method(godot::D_METHOD("get_debug_dpi_scale"),
                                &GameHost::get_debug_dpi_scale);
    godot::ClassDB::bind_method(godot::D_METHOD("get_debug_live_rids"),
                                &GameHost::get_debug_live_rids);
    godot::ClassDB::bind_method(godot::D_METHOD("get_render_triangles"),
                                &GameHost::get_render_triangles);
    godot::ClassDB::bind_method(godot::D_METHOD("get_ui_mouse_visible"),
                                &GameHost::get_ui_mouse_visible);
    godot::ClassDB::bind_method(godot::D_METHOD("debug_stdio_test"), &GameHost::debug_stdio_test);
    godot::ClassDB::bind_method(godot::D_METHOD("get_tire_audio_playing"),
                                &GameHost::get_tire_audio_playing);
    godot::ClassDB::bind_method(godot::D_METHOD("get_collision_sound_count"),
                                &GameHost::get_collision_sound_count);
    godot::ClassDB::bind_method(godot::D_METHOD("get_crash_sound_count"),
                                &GameHost::get_crash_sound_count);
    godot::ClassDB::bind_method(godot::D_METHOD("get_active_collision_voices"),
                                &GameHost::get_active_collision_voices);
    godot::ClassDB::bind_method(godot::D_METHOD("get_tire_sound_start_count"),
                                &GameHost::get_tire_sound_start_count);
    godot::ClassDB::bind_method(godot::D_METHOD("get_tire_audio_playback_position"),
                                &GameHost::get_tire_audio_playback_position);
    godot::ClassDB::bind_method(godot::D_METHOD("begin_sfx_fixture"), &GameHost::begin_sfx_fixture);
    godot::ClassDB::bind_method(godot::D_METHOD("sfx_fixture_tire", "active"),
                                &GameHost::sfx_fixture_tire);
    godot::ClassDB::bind_method(godot::D_METHOD("sfx_fixture_collision", "land", "count"),
                                &GameHost::sfx_fixture_collision);
    godot::ClassDB::bind_method(godot::D_METHOD("get_camera_heading"),
                                &GameHost::get_camera_heading);
    godot::ClassDB::bind_method(godot::D_METHOD("get_camera_orbit_state"),
                                &GameHost::get_camera_orbit_state);
    godot::ClassDB::bind_method(godot::D_METHOD("get_camera_far_distance"),
                                &GameHost::get_camera_far_distance);
    godot::ClassDB::bind_method(godot::D_METHOD("get_view_distance_at", "position"),
                                &GameHost::get_view_distance_at);
    godot::ClassDB::bind_method(godot::D_METHOD("orbit_test_input", "relative_x", "relative_y"),
                                &GameHost::orbit_test_input);
    godot::ClassDB::bind_method(godot::D_METHOD("get_camera_position"),
                                &GameHost::get_camera_position);
    godot::ClassDB::bind_method(godot::D_METHOD("get_camera_orbit_yaw"),
                                &GameHost::get_camera_orbit_yaw);
    godot::ClassDB::bind_method(godot::D_METHOD("start_fog_fixture"), &GameHost::start_fog_fixture);
    godot::ClassDB::bind_method(godot::D_METHOD("restart_smoke_scene"),
                                &GameHost::restart_smoke_scene);
    godot::ClassDB::bind_method(godot::D_METHOD("get_live_rids"), &GameHost::get_live_rids);
    godot::ClassDB::bind_method(godot::D_METHOD("is_engine_audio_playing"),
                                &GameHost::is_engine_audio_playing);
    godot::ClassDB::bind_method(godot::D_METHOD("get_engine_audio_frequency"),
                                &GameHost::get_engine_audio_frequency);
    godot::ClassDB::bind_method(godot::D_METHOD("get_engine_audio_gain"),
                                &GameHost::get_engine_audio_gain);
    godot::ClassDB::bind_method(godot::D_METHOD("get_engine_audio_playback_position"),
                                &GameHost::get_engine_audio_playback_position);
    godot::ClassDB::bind_method(godot::D_METHOD("get_engine_audio_sample_frames"),
                                &GameHost::get_engine_audio_sample_frames);
    godot::ClassDB::bind_method(godot::D_METHOD("drive_test_step", "delta", "steering", "throttle",
                                                "brake", "reset", "handbrake"),
                                &GameHost::drive_test_step, godot::Variant(false));
    godot::ClassDB::bind_method(godot::D_METHOD("get_vehicle_position"),
                                &GameHost::get_vehicle_position);
    godot::ClassDB::bind_method(godot::D_METHOD("get_vehicle_heading"),
                                &GameHost::get_vehicle_heading);
    godot::ClassDB::bind_method(godot::D_METHOD("get_vehicle_speed"), &GameHost::get_vehicle_speed);
    godot::ClassDB::bind_method(godot::D_METHOD("get_particle_count"),
                                &GameHost::get_particle_count);
    godot::ClassDB::bind_method(godot::D_METHOD("get_spark_count"), &GameHost::get_spark_count);
    godot::ClassDB::bind_method(godot::D_METHOD("get_tire_particle_count"),
                                &GameHost::get_tire_particle_count);
    godot::ClassDB::bind_method(godot::D_METHOD("get_skid_vertex_count"),
                                &GameHost::get_skid_vertex_count);
    godot::ClassDB::bind_method(godot::D_METHOD("set_skid_marks_visible", "visible"),
                                &GameHost::set_skid_marks_visible);
    godot::ClassDB::bind_method(godot::D_METHOD("set_particles_visible", "visible"),
                                &GameHost::set_particles_visible);
}
void GameHost::start_fog_fixture() {
    try {
        set_mouse_capture(false);
        frontend_video_voice_.reset();
        frontend_sfx_.reset();
        frontend_audio_screen_.clear();
        menu_.reset();
        engine_audio_adapter_.reset();
        race_music_adapter_.reset();
        race_music_.stop();
        sfx_audio_adapter_.reset();
        engine_audio_.reset();
        entry_.reset();
        pause_menu_.reset();
        if (get_tree() && session_ && session_->paused())
            get_tree()->set_pause(previous_tree_paused_);
        session_.reset();
        vehicle_.reset();
        race_.reset();
        runtime_.reset();
        renderer_.reset();
        renderer_ = std::make_unique<ServerRenderer>(get_viewport()->get_viewport_rid());
        const auto size = get_viewport()->get_visible_rect().size;
        renderer_->create_fog_fixture(size.x / size.y);

    } catch (const std::exception& error) {
        godot::UtilityFunctions::push_error(error.what());
        get_tree()->quit(1);
    } catch (...) {
        godot::UtilityFunctions::push_error("Unknown diagnostic fixture failure");
        get_tree()->quit(1);
    }
}
void GameHost::start_billboard_fixture() {
    try {
        set_mouse_capture(false);
        frontend_video_voice_.reset();
        frontend_sfx_.reset();
        frontend_audio_screen_.clear();
        menu_.reset();
        entry_.reset();
        engine_audio_adapter_.reset();
        race_music_adapter_.reset();
        race_music_.stop();
        sfx_audio_adapter_.reset();
        pause_menu_.reset();
        if (get_tree() && session_ && session_->paused())
            get_tree()->set_pause(previous_tree_paused_);
        session_.reset();
        vehicle_.reset();
        race_.reset();
        runtime_.reset();
        renderer_.reset();
        renderer_ = std::make_unique<ServerRenderer>(get_viewport()->get_viewport_rid(), true);
        renderer_->create_billboard_fixture();
        set_billboard_fixture_camera(0, 0);
    } catch (const std::exception& error) {
        godot::UtilityFunctions::push_error(error.what());
        get_tree()->quit(1);
    } catch (...) {
        godot::UtilityFunctions::push_error("Billboard fixture initialization failed");
        get_tree()->quit(1);
    }
}
void GameHost::set_billboard_fixture_camera(double heading, double pitch) {
    if (!renderer_ || !std::isfinite(heading) || !std::isfinite(pitch))
        return;
    const auto orientation = original_camera_frame({}, {}, heading, pitch);
    const auto forward = orientation.transform.basis[2];
    const auto camera =
        original_camera_frame({forward.x * 8, forward.y * 8, forward.z * 8}, {}, heading, pitch);
    renderer_->update_camera(camera.transform, camera.vertical_fov_degrees);
}

void GameHost::_ready() {
    set_process_mode(godot::Node::PROCESS_MODE_ALWAYS);
    bool enable_debug_overlay = true;
    for (const auto argument : godot::OS::get_singleton()->get_cmdline_user_args()) {
        if (argument == "--no-debug-overlay")
            enable_debug_overlay = false;
        if (argument == "--race") {
            competition_requested_ = competition_ = race_mode_ = true;
        }
        if (argument.begins_with("--race-laps=")) {
            const auto value = argument.trim_prefix("--race-laps=");
            race_laps_ = value.is_valid_int() ? value.to_int() : 0;
        }
        if (argument.begins_with("--race-ai=")) {
            const auto value = argument.trim_prefix("--race-ai=");
            race_ai_count_ = value.is_valid_int() ? value.to_int() : -1;
        }
        if (argument == "--debug")
            developer_build_ = true;
        if (argument == "--original-loop")
            original_loop_ = true;
        if (argument == "--diagnostic")
            diagnostic_ = true;
        if (argument.begins_with("--frontend-screen="))
            frontend_preview_ = argument.trim_prefix("--frontend-screen=");
        if (argument.begins_with("--frontend-variant="))
            frontend_variant_ = argument.trim_prefix("--frontend-variant=");
        if (argument.begins_with("--frontend-time="))
            frontend_preview_time_ = argument.trim_prefix("--frontend-time=").to_float();
        if (argument.begins_with("--game-dir="))
            game_directory_ = argument.trim_prefix("--game-dir=");
        if (argument.begins_with("--race-track=")) {
            track_name_ = argument.trim_prefix("--race-track=");
            race_mode_ = true;
        }
        if (argument.begins_with("--race-vehicle="))
            vehicle_name_ = argument.trim_prefix("--race-vehicle=");
        if (argument == "--race-preview")
            race_preview_ = true;
    }
    try {
        if (game_directory_.is_empty() && !diagnostic_ && !original_loop_) {
            const auto config_text =
                godot::FileAccess::get_file_as_string("res://../../../config.local.json");
            const auto config_value = godot::JSON::parse_string(config_text);
            if (config_value.get_type() == godot::Variant::DICTIONARY) {
                const godot::Dictionary config = config_value;
                game_directory_ = config.get("game_dir", "");
            }
        }
        if (enable_debug_overlay &&
            (developer_build_ || diagnostic_ || (race_mode_ && !competition_) || original_loop_))
            debug_overlay_ = std::make_unique<DebugOverlayAdapter>(get_viewport());
        if (competition_ &&
            (diagnostic_ || original_loop_ || race_preview_ || !frontend_preview_.is_empty() ||
             race_laps_ < 1 || race_laps_ > 10 || race_ai_count_ < 0 || race_ai_count_ > 5))
            throw std::runtime_error("Invalid or conflicting race launch options");
        restart_smoke_scene();
    } catch (const std::exception& error) {
        godot::UtilityFunctions::push_error(error.what());
        get_tree()->quit(1);
    }
}
void GameHost::restart_smoke_scene() {
    try {
        if (competition_requested_) {
            competition_ = race_mode_ = true;
            if (track_name_.is_empty())
                track_name_ = "Dark Forest";
        }
        race_completed_ = false;
        simulation_failed_ = false;
        race_results_pending_ = false;
        if (debug_overlay_)
            debug_overlay_->ui().clear_race_debug();
        entry_.reset();
        frontend_video_voice_.reset();
        frontend_sfx_.reset();
        frontend_audio_screen_.clear();
        menu_.reset();
        engine_audio_adapter_.reset();
        race_music_adapter_.reset();
        race_music_.stop();
        sfx_audio_adapter_.reset();
        engine_audio_.reset();
        tire_audio_.reset();
        tire_sample_path_.clear();
        tire_sound_starts_ = 0;
        crash_sound_starts_ = 0;
        sfx_fixture_ = false;
        chase_camera_.reset();
        orbit_camera_.reset();
        pending_mouse_ = {};
        sparks_.reset();
        pause_menu_.reset();
        if (get_tree() && session_ && session_->paused())
            get_tree()->set_pause(previous_tree_paused_);
        session_.reset();
        vehicle_.reset();
        race_.reset();
        resources_.reset();
        files_.reset();
        runtime_.reset();
        renderer_.reset();
        renderer_ = std::make_unique<ServerRenderer>(
            get_viewport()->get_viewport_rid(), race_mode_ || (!diagnostic_ && !original_loop_));
        runtime_ = std::make_unique<Runtime>(*renderer_);
        if (diagnostic_ && runtime_->start_smoke_scene() != Result::ok)
            throw std::runtime_error("Smoke scene initialization failed");
        entry_ = std::make_unique<GameEntry>(*this, race_mode_ && !race_preview_
                                                        ? GameEntry::Timing::fixed_simulation
                                                    : !diagnostic_ && !original_loop_ && !race_mode_
                                                        ? GameEntry::Timing::frontend_realtime
                                                        : GameEntry::Timing::legacy_reference);
        if (entry_->start() != Result::ok)
            throw std::runtime_error("Game entry failed");
        renderer_->reset_interpolation();
        last_simulation_usec_ = godot::Time::get_singleton()->get_ticks_usec();
        set_mouse_capture(race_mode_ && !race_preview_ && window_focused_ && !ui_mouse_visible_);
    } catch (const std::exception& error) {
        entry_.reset();
        frontend_video_voice_.reset();
        frontend_sfx_.reset();
        frontend_audio_screen_.clear();
        menu_.reset();
        engine_audio_adapter_.reset();
        race_music_adapter_.reset();
        race_music_.stop();
        sfx_audio_adapter_.reset();
        pause_menu_.reset();
        if (get_tree() && session_ && session_->paused())
            get_tree()->set_pause(previous_tree_paused_);
        session_.reset();
        vehicle_.reset();
        race_.reset();
        resources_.reset();
        files_.reset();
        runtime_.reset();
        renderer_.reset();
        godot::UtilityFunctions::push_error(error.what());
        simulation_failed_ = true;
        ui_mouse_visible_ = true;
        set_mouse_capture(false);
        if (debug_overlay_)
            debug_overlay_->show_error(error.what());
    } catch (...) {
        entry_.reset();
        frontend_video_voice_.reset();
        frontend_sfx_.reset();
        frontend_audio_screen_.clear();
        menu_.reset();
        engine_audio_adapter_.reset();
        race_music_adapter_.reset();
        race_music_.stop();
        sfx_audio_adapter_.reset();
        pause_menu_.reset();
        if (get_tree() && session_ && session_->paused())
            get_tree()->set_pause(previous_tree_paused_);
        session_.reset();
        vehicle_.reset();
        race_.reset();
        resources_.reset();
        files_.reset();
        runtime_.reset();
        renderer_.reset();
        godot::UtilityFunctions::push_error("Unknown host initialization failure");
        simulation_failed_ = true;
        ui_mouse_visible_ = true;
        set_mouse_capture(false);
        if (debug_overlay_)
            debug_overlay_->show_error("Unknown host initialization failure");
    }
}
void GameHost::set_mouse_capture(bool enabled) {
    auto input = godot::Input::get_singleton();
    if (enabled && godot::DisplayServer::get_singleton()->get_name() != "headless") {
        if (!owns_mouse_capture_) {
            previous_mouse_mode_ = input->get_mouse_mode();
            owns_mouse_capture_ = true;
            ignore_next_mouse_motion_ = true;
            pending_mouse_ = {};
        }
        input->set_mouse_mode(godot::Input::MOUSE_MODE_CAPTURED);
    } else if (owns_mouse_capture_) {
        input->set_mouse_mode(ui_mouse_visible_
                                  ? godot::Input::MOUSE_MODE_VISIBLE
                                  : static_cast<godot::Input::MouseMode>(previous_mouse_mode_));
        owns_mouse_capture_ = false;
        pending_mouse_ = {};
    }
}
void GameHost::_notification(int what) {
    if (what == NOTIFICATION_PAUSED || what == NOTIFICATION_UNPAUSED)
        last_simulation_usec_ = godot::Time::get_singleton()->get_ticks_usec();
    if (what == NOTIFICATION_APPLICATION_FOCUS_OUT || what == NOTIFICATION_WM_WINDOW_FOCUS_OUT) {
        window_focused_ = false;
        pending_mouse_ = {};
        set_mouse_capture(false);
        if (debug_overlay_)
            debug_overlay_->focus(false);
    } else if (what == NOTIFICATION_APPLICATION_FOCUS_IN ||
               what == NOTIFICATION_WM_WINDOW_FOCUS_IN) {
        window_focused_ = true;
        pending_mouse_ = {};
        if (debug_overlay_)
            debug_overlay_->focus(ui_mouse_visible_);
        if (entry_ && race_)
            set_mouse_capture(race_mode_ && !race_preview_ && !ui_mouse_visible_ && !pause_menu_);
    }
}
void GameHost::_input(const godot::Ref<godot::InputEvent>& event) {
    const godot::Ref<godot::InputEventKey> key = event;
    if (key.is_valid()) {
        const auto code = key->get_keycode();
        const auto physical = key->get_physical_keycode();
        const bool toggle_debug = physical == godot::KEY_QUOTELEFT ||
                                  code == godot::KEY_QUOTELEFT || code == godot::KEY_ASCIITILDE ||
                                  code == godot::KEY_F8;
        if (toggle_debug && (debug_overlay_ || race_mode_)) {
            if (key->is_pressed() && !key->is_echo()) {
                ui_mouse_visible_ = !ui_mouse_visible_;
                frontend_input_ = {};
                pending_mouse_ = {};
                if (debug_overlay_)
                    debug_overlay_->focus(ui_mouse_visible_);
                set_mouse_capture(!menu_ && !pause_menu_ && !ui_mouse_visible_ && race_mode_ &&
                                  !race_preview_ && window_focused_);
                if (ui_mouse_visible_)
                    godot::Input::get_singleton()->set_mouse_mode(godot::Input::MOUSE_MODE_VISIBLE);
            }
            get_viewport()->set_input_as_handled();
            return;
        }
    }
    if (menu_ || pause_menu_) {
        if (debug_overlay_ && ui_mouse_visible_ &&
            !(pause_menu_ && key.is_valid() && key->get_keycode() == godot::KEY_ESCAPE)) {
            debug_overlay_->input(event, true);
            return;
        }
        if (key.is_valid() && key->is_pressed() && !key->is_echo()) {
            frontend_input_.any_key = true;
            const auto code = key->get_keycode();
            frontend_input_.left |= code == godot::KEY_LEFT;
            frontend_input_.right |= code == godot::KEY_RIGHT;
            frontend_input_.up |= code == godot::KEY_UP;
            frontend_input_.down |= code == godot::KEY_DOWN;
            frontend_input_.accept |= code == godot::KEY_ENTER || code == godot::KEY_SPACE;
            frontend_input_.back |= code == godot::KEY_ESCAPE;
        }
        const godot::Ref<godot::InputEventMouseMotion> motion = event;
        const godot::Ref<godot::InputEventMouseButton> button = event;
        if (pause_menu_ && motion.is_valid() && ignore_next_mouse_motion_) {
            ignore_next_mouse_motion_ = false;
            return;
        }
        if (motion.is_valid() || button.is_valid()) {
            const auto position =
                motion.is_valid() ? motion->get_position() : button->get_position();
            const auto size = get_viewport()->get_visible_rect().size;
            if (!frontend_input_.pointer_accept && size.x > 0 && size.y > 0) {
                frontend_input_.pointer_x = position.x * 640 / size.x;
                frontend_input_.pointer_y = position.y * 480 / size.y;
            }
            frontend_input_.pointer_moved |= motion.is_valid();
            const bool clicked = button.is_valid() && button->is_pressed() &&
                                 button->get_button_index() == godot::MOUSE_BUTTON_LEFT;
            frontend_input_.pointer_accept |= clicked;
            frontend_input_.any_key |= clicked;
        }
        return;
    }
    if (competition_ && session_ && key.is_valid() && key->get_keycode() == godot::KEY_ESCAPE) {
        if (key->is_pressed() && !key->is_echo()) {
            try {
                set_competition_paused(true);
            } catch (const std::exception& error) {
                godot::UtilityFunctions::push_error(error.what());
            }
            last_simulation_usec_ = godot::Time::get_singleton()->get_ticks_usec();
        }
        get_viewport()->set_input_as_handled();
        return;
    }
    if (debug_overlay_ && ui_mouse_visible_) {
        const bool handled = debug_overlay_->input(event, true);
        const godot::Ref<godot::InputEventMouseButton> button = event;
        if (!handled && button.is_valid() && button->is_pressed() &&
            button->get_button_index() == godot::MOUSE_BUTTON_LEFT) {
            ui_mouse_visible_ = false;
            debug_overlay_->focus(false);
            set_mouse_capture(race_mode_ && !race_preview_ && window_focused_);
        }
        get_viewport()->set_input_as_handled();
        return;
    }
    const godot::Ref<godot::InputEventMouseMotion> motion = event;
    if (race_mode_ && !race_preview_ && window_focused_ && owns_mouse_capture_ &&
        motion.is_valid()) {
        if (ignore_next_mouse_motion_) {
            ignore_next_mouse_motion_ = false;
            return;
        }
        const auto relative = motion->get_screen_relative();
        orbit_test_input(relative.x, relative.y);
    }
}
void GameHost::orbit_test_input(double x, double y) {
    if (std::isfinite(x) && std::isfinite(y)) {
        pending_mouse_.relative_x += static_cast<float>(x);
        pending_mouse_.relative_y += static_cast<float>(y);
    }
}
godot::Vector3 GameHost::get_camera_position() const {
    const auto p = race_ ? race_->camera_position() : Vec3{};
    return {p.x, p.y, p.z};
}
double GameHost::get_camera_orbit_yaw() const { return orbit_camera_.yaw_offset_degrees(); }
double GameHost::get_camera_heading() const {
    return race_ ? race_->camera_frame().heading_degrees : 0;
}
int GameHost::get_camera_orbit_state() const { return static_cast<int>(orbit_camera_.state()); }
double GameHost::get_camera_far_distance() const {
    return race_ ? race_->camera_far_distance() : 0;
}
double GameHost::get_view_distance_at(godot::Vector3 p) {
    return race_ ? race_->view_distance_at({p.x, p.y, p.z}) : 0;
}

void GameHost::_process(double delta) {
    try {
        const auto now = godot::Time::get_singleton()->get_ticks_usec();
        const auto elapsed = last_simulation_usec_
                                 ? static_cast<double>(now - last_simulation_usec_) / 1000000.0
                                 : 0.0;
        last_simulation_usec_ = now;
        update_race_debug();
        if (debug_overlay_)
            debug_overlay_->frame(delta, ui_mouse_visible_ && window_focused_);
        if (!entry_ || simulation_failed_)
            return;
        if (update_pause_menu(delta)) {
            present_test_frame();
            return;
        }
        InputState input;
        input.quit_requested = false; // ESC belongs to the portable race pause menu.
        if (race_mode_ && !ui_mouse_visible_ && window_focused_) {
            const auto keyboard = godot::Input::get_singleton();
            input.steering = (keyboard->is_physical_key_pressed(godot::KEY_D) ||
                                      keyboard->is_physical_key_pressed(godot::KEY_RIGHT)
                                  ? 1.0f
                                  : 0.0f) -
                             (keyboard->is_physical_key_pressed(godot::KEY_A) ||
                                      keyboard->is_physical_key_pressed(godot::KEY_LEFT)
                                  ? 1.0f
                                  : 0.0f);
            input.throttle = keyboard->is_physical_key_pressed(godot::KEY_W) ||
                                     keyboard->is_physical_key_pressed(godot::KEY_UP)
                                 ? 1.0f
                                 : 0.0f;
            input.brake = keyboard->is_physical_key_pressed(godot::KEY_S) ||
                                  keyboard->is_physical_key_pressed(godot::KEY_DOWN)
                              ? 1.0f
                              : 0.0f;
            input.handbrake = keyboard->is_physical_key_pressed(godot::KEY_SPACE);
            input.reset_requested = keyboard->is_physical_key_pressed(godot::KEY_R);
        }
        const auto scale = godot::Engine::get_singleton()->get_time_scale();
        if (race_mode_ && !race_preview_ && scale == 0) {
            present_test_frame();
            return;
        }
        const double simulation_delta = menu_                          ? elapsed
                                        : race_mode_ && !race_preview_ ? elapsed * scale
                                                                       : delta;
        if (entry_->advance(simulation_delta, input) != Result::ok) {
            throw std::runtime_error("Runtime update failed");
        }
        if (race_results_pending_)
            show_race_results();
        present_test_frame();
        if (menu_) {
            frontend_input_ = {};
            if (menu_->quit_requested())
                get_tree()->quit();
        }
        if (input.quit_requested)
            get_tree()->quit();
    } catch (const std::exception& error) {
        simulation_failed_ = true;
        frontend_video_voice_.reset();
        frontend_sfx_.reset();
        ui_mouse_visible_ = true;
        set_mouse_capture(false);
        godot::UtilityFunctions::push_error(error.what());
        if (debug_overlay_)
            debug_overlay_->show_error(error.what());
    } catch (...) {
        simulation_failed_ = true;
        frontend_video_voice_.reset();
        frontend_sfx_.reset();
        ui_mouse_visible_ = true;
        set_mouse_capture(false);
        godot::UtilityFunctions::push_error("Host update exception");
        if (debug_overlay_)
            debug_overlay_->show_error("Host update exception");
    }
}
godot::Vector2 GameHost::get_console_input_position() const {
    const auto p =
        debug_overlay_ ? debug_overlay_->ui().console_input_position() : std::array<float, 2>{};
    return {p[0], p[1]};
}
bool GameHost::get_console_collapsed() const {
    return debug_overlay_ && debug_overlay_->ui().collapsed();
}
bool GameHost::console_contains(godot::String text) const {
    return debug_overlay_ && debug_overlay_->messages()->contains(text.utf8().get_data());
}
double GameHost::get_debug_dpi_scale() const {
    return debug_overlay_ ? debug_overlay_->ui().scale() : 0;
}
int GameHost::get_debug_live_rids() const {
    return debug_overlay_ ? debug_overlay_->live_rids() : 0;
}
int64_t GameHost::get_render_triangles() const {
    return debug_overlay_ ? debug_overlay_->triangles() : 0;
}
bool GameHost::get_ui_mouse_visible() const { return ui_mouse_visible_; }
void GameHost::debug_stdio_test() {
    std::cout << "ROADWARS_STDOUT_CAPTURE_TEST" << std::endl;
    std::cerr << "ROADWARS_STDERR_CAPTURE_TEST" << std::endl;
}
void GameHost::_exit_tree() {
    set_mouse_capture(false);
    entry_.reset();
    frontend_video_voice_.reset();
    frontend_sfx_.reset();
    frontend_audio_screen_.clear();
    menu_.reset();
    engine_audio_adapter_.reset();
    race_music_adapter_.reset();
    race_music_.stop();
    sfx_audio_adapter_.reset();
    engine_audio_.reset();
    pause_menu_.reset();
    if (get_tree() && session_ && session_->paused())
        get_tree()->set_pause(previous_tree_paused_);
    session_.reset();
    vehicle_.reset();
    race_.reset();
    resources_.reset();
    files_.reset();
    runtime_.reset();
    renderer_.reset();
    debug_overlay_.reset();
}
Result GameHost::invoke(LoopStage stage, float delta, const InputState& input, LoopState& state) {
    if (!diagnostic_ && !original_loop_ && !race_mode_) {
        if (stage == LoopStage::initialize_0045c8ab) {
            state.mode = GameMode::frontend;
            if (game_directory_.is_empty())
                throw std::runtime_error("Set game_dir in config.local.json");
            files_ = std::make_unique<GameFiles>(game_directory_);
            resources_ = std::make_unique<ResourceStore>(*files_);
            if (resources_->open() != Result::ok)
                throw std::runtime_error("Original frontend archive could not be opened");
            menu_ = std::make_unique<MenuFrontend>(*resources_, *renderer_);
            std::string error;
            if (menu_->start(error) != Result::ok)
                throw std::runtime_error(error);
            if (!frontend_preview_.is_empty() &&
                !preview_frontend_screen(frontend_preview_, frontend_preview_time_,
                                         frontend_variant_))
                throw std::runtime_error("Frontend preview initialization failed");
            frontend_sfx_ = std::make_unique<SfxAudioAdapter>(*this);
            update_frontend_audio();
            return Result::ok;
        }
        if (stage == LoopStage::frontend_004ab60d) {
            std::string error;
            const auto result = menu_->advance(delta, frontend_input_, error);
            if (result != Result::ok)
                throw std::runtime_error(error);
            update_frontend_audio();
        }
        return Result::ok;
    }
    if (race_mode_) {
        if (stage == LoopStage::initialize_0045c8ab) {
            if (game_directory_.is_empty())
                throw std::runtime_error(
                    "The launcher must supply game_dir from config.local.json");
            files_ = std::make_unique<GameFiles>(game_directory_);
            resources_ = std::make_unique<ResourceStore>(*files_);
            if (resources_->open() != Result::ok)
                throw std::runtime_error("Original game archive could not be opened");
            std::string error;
            const BootOptions options{track_name_.utf8().get_data(),
                                      vehicle_name_.utf8().get_data()};
            if (initialize_test_game(*resources_, options, initialized_, error) != Result::ok)
                throw std::runtime_error(error);
            race_ = std::make_unique<RacePresentation>(*renderer_);
            if (race_->load(*resources_, options.track, options.vehicle, error) != Result::ok)
                throw std::runtime_error(error);
            // Explicit free-driving character selection; car/character selection remains
            // separate in the original game and will come from the recovered race setup.
            race_->set_developer_build(developer_build_);
            if (race_->configure_frontend(*resources_, options.track, "Animal", error) !=
                Result::ok)
                throw std::runtime_error("Original frontend unavailable: " + error);
            const auto hud_size = get_viewport()->get_visible_rect().size;
            renderer_->resize_hud(hud_size.x, hud_size.y);
            if (chase_camera_.load(*resources_, options.vehicle, error) != Result::ok)
                throw std::runtime_error(error);
            VehicleState preview_state;
            preview_state.position = race_->collision().spawn().position;
            preview_state.heading_degrees = race_->collision().spawn().heading_degrees;
            CameraFrame preview_camera;
            if (chase_camera_.update(preview_state, {}, nullptr, 0, preview_camera) != Result::ok)
                throw std::runtime_error("Original preview camera unavailable");
            race_->apply_camera(preview_camera);
            if (!race_preview_) {
                float effects_scalar{};
                if (initialized_.global_config.number("ROADWARS", "SPECIAL_EFFECTS_SCALAR",
                                                      effects_scalar) != Result::ok)
                    throw std::runtime_error("Original SPECIAL_EFFECTS_SCALAR is unavailable");
                for (auto& emitter : tire_smoke_)
                    if (emitter.configure(effects_scalar, particle_random_) != Result::ok)
                        throw std::runtime_error("Original tire particles unavailable");
                if (race_->configure_tire_particles(
                        *resources_, static_cast<std::uint32_t>(tire_smoke_[0].particles().size() *
                                                                4)) != Result::ok)
                    throw std::runtime_error("Native tire particles unavailable");
                const auto effects = vehicle_spark_settings(effects_scalar);
                if (sparks_.configure(effects) != Result::ok ||
                    race_->configure_particles(*resources_, effects.capacity) != Result::ok)
                    throw std::runtime_error("Native Godot particle initialization failed");
                VehicleParameters parameters;
                if (load_vehicle_metrics(*resources_, options.vehicle, parameters, error) !=
                    Result::ok)
                    throw std::runtime_error(error);
                if (load_vehicle_geometry(*resources_, options.vehicle, parameters, error) !=
                    Result::ok)
                    throw std::runtime_error(error);
                float manual_transmission{};
                if (initialized_.global_config.number("ROADWARS", "MANUALTRANSMISSION",
                                                      manual_transmission) != Result::ok)
                    throw std::runtime_error("Original MANUALTRANSMISSION is unavailable");
                parameters.automatic_transmission = manual_transmission == 0;
                vehicle_ = std::make_unique<VehicleDynamics>();
                float skid_scalar{};
                if (initialized_.global_config.number("ROADWARS", "SKIDMARK_SCALAR", skid_scalar) !=
                    Result::ok)
                    throw std::runtime_error("Original SKIDMARK_SCALAR unavailable");
                for (auto& emitter : skid_marks_)
                    if (emitter.configure(skid_scalar, parameters.tire_half_width) != Result::ok)
                        throw std::runtime_error("Original skid-mark initialization failed");
                if (race_->configure_skid_marks(*resources_) != Result::ok)
                    throw std::runtime_error("Native skid-mark rendering unavailable");
                if (vehicle_->initialize(std::move(parameters), race_->collision(), error) !=
                    Result::ok)
                    throw std::runtime_error(error);
                if (load_engine_audio(*resources_, options.vehicle, engine_audio_resources_,
                                      error) != Result::ok)
                    throw std::runtime_error(error);
                if (engine_audio_.initialize(engine_audio_resources_.sample.sample_rate,
                                             engine_audio_resources_.sfx_volume) != Result::ok)
                    throw std::runtime_error("Original engine voice initialization failed");
                engine_audio_adapter_ =
                    std::make_unique<EngineAudioAdapter>(*this, engine_audio_resources_.sample);
                if (race_music_.load(*resources_, error) != Result::ok)
                    throw std::runtime_error(error);
                race_music_adapter_ = std::make_unique<EngineAudioAdapter>(
                    *this, race_music_.sample(), true, "OriginalRaceMusic");
                EngineAudioCommand music;
                if (!competition_) {
                    if (race_music_.start(music) != Result::ok)
                        throw std::runtime_error("Original race music start failed");
                    race_music_adapter_->submit(music);
                }
                if (load_collision_audio(*resources_, collision_audio_resources_, error) !=
                        Result::ok ||
                    load_tire_audio(*resources_, options.track, tire_audio_resources_, error) !=
                        Result::ok)
                    throw std::runtime_error(error);
                if (tire_audio_.initialize(tire_audio_resources_.sfx_volume) != Result::ok)
                    throw std::runtime_error("Original tire voice initialization failed");
                sfx_audio_adapter_ = std::make_unique<SfxAudioAdapter>(*this);
            }
            if (competition_) {
                session_ = std::make_unique<RaceSession>();
                RaceOptions setup{options.track, options.vehicle, race_laps_, race_ai_count_};
                if (session_->load(*resources_, race_->collision(), setup, *vehicle_, error) !=
                    Result::ok)
                    throw std::runtime_error(error);
                std::vector<std::string> characters;
                for (const auto& racer : session_->racers())
                    characters.push_back(racer.character);
                if (race_->configure_racers(*resources_, options.track, characters, error) !=
                    Result::ok)
                    throw std::runtime_error(error);
                for (std::size_t i = 0; i < session_->player_index(); ++i) {
                    if (race_->add_opponent(*resources_, characters[i], error) != Result::ok)
                        throw std::runtime_error(error);
                    race_->update_opponent(i, *session_->racers()[i].vehicle, 0, true);
                }
                race_->update_vehicle(vehicle_->transform());
                race_->apply_camera(session_->start_sequence().camera());
                renderer_->reset_interpolation();
                std::vector<FrontendRacer> racers;
                auto frame = session_->frontend_frame(racers);
                frame.developer_build = developer_build_;
                race_->update_frontend(frame);
            }
            godot::UtilityFunctions::print(
                "ROADWARS_RACE_LOADED: ", track_name_, " / ", vehicle_name_,
                race_preview_ ? " (static resource preview)" : " (original logic trace)");
        } else if (stage == LoopStage::single_race_004a4f57 && vehicle_) {
            if (session_ && session_->paused()) {
                std::vector<FrontendRacer> racers;
                auto frame = session_->frontend_frame(racers);
                frame.developer_build = developer_build_;
                return race_->update_frontend(frame);
            }
            renderer_->begin_simulation_tick();
            race_->update_track_animation(delta);
            if (session_) {
                std::string error;
                session_->set_player_autopilot(
                    competition_test_autopilot_ ||
                    (debug_overlay_ && debug_overlay_->ui().race_options().auto_drive));
                if (session_->advance(delta, input, error) != Result::ok)
                    throw std::runtime_error(error);
                for (std::size_t i = 0; i < session_->player_index(); ++i)
                    race_->update_opponent(i, *session_->racers()[i].vehicle, delta,
                                           input.reset_requested);
                race_results_pending_ = session_->phase() == RacePhase::finished;
            } else
                vehicle_->update(delta, input);
            if (input.reset_requested)
                for (auto& emitter : skid_marks_)
                    emitter.reset();
            for (std::size_t i = 0; i < 4; ++i)
                if (skid_marks_[i].advance(delta, vehicle_->tire_particle_sources()[i]) !=
                    Result::ok)
                    throw std::runtime_error("Original skid-mark update failed");
            if (race_->update_skid_marks(skid_marks_) != Result::ok)
                throw std::runtime_error("Native skid-mark submission failed");
            if (input.reset_requested) {
                sparks_.reset();
                for (auto& emitter : tire_smoke_)
                    emitter.reset();
            }
            for (const auto& impact : vehicle_->particle_impacts())
                sparks_.begin_burst(impact);
            if (sparks_.advance(delta, true, {}, {}, particle_random_) != Result::ok ||
                race_->update_particles(sparks_.particles()) != Result::ok)
                throw std::runtime_error("Original particle update failed");
            std::vector<ParticleRenderState> tire_states;
            for (std::size_t i = 0; i < 4; ++i) {
                if (tire_smoke_[i].advance(delta, vehicle_->tire_particle_sources()[i]) !=
                    Result::ok)
                    throw std::runtime_error("Original tire particle update failed");
                const auto states = tire_smoke_[i].particles();
                tire_states.insert(tire_states.end(), states.begin(), states.end());
            }
            if (race_->update_tire_particles(tire_states) != Result::ok)
                throw std::runtime_error("Native tire particle update failed");
            race_->update_wheels(*vehicle_, delta, vehicle_->state().brake_amount,
                                 input.reset_requested);
            race_->update_vehicle(vehicle_->transform());
            if (session_) {
                std::vector<FrontendRacer> racers;
                auto frame = session_->frontend_frame(racers);
                frame.developer_build = developer_build_;
                if (race_->update_frontend(frame) != Result::ok)
                    throw std::runtime_error("Race HUD update failed");
            } else if (race_->update_frontend(vehicle_->state()) != Result::ok)
                throw std::runtime_error("Original frontend update failed");
            const auto hud_size = get_viewport()->get_visible_rect().size;
            renderer_->resize_hud(hud_size.x, hud_size.y);
            if (input.reset_requested) {
                chase_camera_.reset();
                orbit_camera_.reset();
                pending_mouse_ = {};
            }
            CameraFrame original_camera, camera;
            if (session_) {
                const int inspected =
                    debug_overlay_ ? debug_overlay_->ui().race_options().selected_ai : -1;
                original_camera = session_->camera_for(inspected);
                if (session_->countdown() >= 1) {
                    orbit_camera_.reset();
                    camera = original_camera;
                } else if (orbit_camera_.update(original_camera, delta, pending_mouse_, camera) !=
                           Result::ok)
                    throw std::runtime_error("Race camera orbit update failed");
            } else if (chase_camera_.update(vehicle_->state(), vehicle_->parameters(),
                                            &race_->collision(), delta,
                                            original_camera) != Result::ok ||
                       orbit_camera_.update(original_camera, delta, pending_mouse_, camera) !=
                           Result::ok)
                throw std::runtime_error("Original chase camera update failed");
            pending_mouse_ = {};
            race_->apply_camera(camera);
            if (input.reset_requested || (entry_ && entry_->simulation_ticks() == 0))
                renderer_->reset_interpolation();
            if (sfx_audio_adapter_) {
                if (input.reset_requested) {
                    sfx_audio_adapter_->reset();
                    tire_audio_.reset();
                    tire_sound_starts_ = 0;
                    crash_sound_starts_ = 0;
                }
                if (session_ && race_music_adapter_) {
                    if (input.reset_requested)
                        race_music_adapter_->submit(race_music_.stop());
                    if (session_->phase() == RacePhase::racing && !race_music_.playing()) {
                        EngineAudioCommand music;
                        if (race_music_.start(music) != Result::ok)
                            throw std::runtime_error("Original race music start failed");
                        race_music_adapter_->submit(music);
                    }
                }
                if (session_) {
                    auto& start = session_->start_sequence();
                    for (int cue : start.sounds()) {
                        EngineAudioCommand command;
                        command.action = EngineVoiceAction::start;
                        command.set_volume = true;
                        command.linear_gain = start.volume();
                        command.qmdx_volume = static_cast<int>(start.volume() * 32767);
                        sfx_audio_adapter_->play_one_shot(start.sample(cue), command,
                                                          "OriginalCountdown");
                    }
                    start.clear_sounds();
                }
                sfx_audio_adapter_->reap_finished();
                if (!sfx_fixture_) {
                    submit_tire_audio(vehicle_->tire_audio_input());
                    for (const auto& event : vehicle_->collision_audio_events())
                        submit_collision_audio(event);
                }
            }
            if (engine_audio_adapter_ && !sfx_fixture_) {
                if (input.reset_requested) {
                    EngineAudioCommand stop;
                    stop.action = EngineVoiceAction::stop;
                    engine_audio_adapter_->submit(stop);
                    engine_audio_.reset();
                }
                const auto& state = vehicle_->state();
                EngineAudioCommand sound;
                if (engine_audio_.update(
                        delta, state.rpm, state.position, state.velocity,
                        engine_audio_audible(state.position, race_->camera_position()), sound,
                        false, state.cached_speed) != Result::ok)
                    throw std::runtime_error("Original engine voice update failed");
                engine_audio_adapter_->submit(sound);
            }
        }
        // Keyboard/window polling belongs to the Godot host. Menus and inactive
        // gameplay subsystems are disabled by the explicit free-driving profile.
        return Result::ok;
    }
    if (original_loop_) {
        const auto id =
            godot::String(
                "93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:") +
            GameEntry::original_address(stage);
        const auto text = id.utf8();
        not_implemented(text.get_data(), GameEntry::original_address(stage));
    }
    // Explicit diagnostic mode: exercise scheduling and rendering, not original logic.
    if (stage == LoopStage::single_race_004a4f57)
        return runtime_->update(delta, input);
    return Result::ok;
}
void GameHost::set_frontend_visible(bool visible) {
    if (renderer_)
        renderer_->set_hud_visible(visible);
}
godot::String GameHost::get_frontend_screen() const {
    return menu_ ? godot::String::utf8(menu_->screen().data(), menu_->screen().size())
                 : godot::String();
}
godot::String GameHost::get_frontend_action() const {
    return menu_ ? godot::String::utf8(menu_->action().data(), menu_->action().size())
                 : godot::String();
}
bool GameHost::preview_frontend_screen(godot::String id, double seconds, godot::String variant) {
    try {
        if (!menu_)
            return false;
        std::string error;
        const auto result =
            menu_->preview(id.utf8().get_data(), seconds, variant.utf8().get_data(), error);
        if (result != Result::ok)
            godot::UtilityFunctions::push_error(error.c_str());
        if (result == Result::ok) {
            frontend_audio_screen_.clear();
            update_frontend_audio();
        }
        present_test_frame();
        return result == Result::ok;
    } catch (const std::exception& error) {
        godot::UtilityFunctions::push_error(error.what());
    } catch (...) {
        godot::UtilityFunctions::push_error("Frontend preview exception");
    }
    return false;
}
bool GameHost::frontend_test_step(double elapsed, godot::String command) {
    try {
        if (!menu_)
            return false;
        FrontendInput input;
        input.left = command == "left";
        input.right = command == "right";
        input.up = command == "up";
        input.down = command == "down";
        input.accept = command == "accept";
        input.back = command == "back";
        std::string error;
        const auto result = menu_->advance(elapsed, input, error);
        update_frontend_audio();
        if (result != Result::ok)
            godot::UtilityFunctions::push_error(error.c_str());
        present_test_frame();
        return result == Result::ok;
    } catch (const std::exception& error) {
        godot::UtilityFunctions::push_error(error.what());
    } catch (...) {
        godot::UtilityFunctions::push_error("Frontend test-step exception");
    }
    return false;
}
void GameHost::update_frontend_audio() {
    if (!menu_)
        return;
    const std::string screen(menu_->screen());
    if (frontend_audio_screen_ != screen) {
        frontend_video_voice_.reset();
        frontend_audio_screen_ = screen;
        if (const auto* audio = menu_->video_audio(); audio && !audio->encoded.empty()) {
            WavePcm sample;
            std::string error;
            const auto result = decode_frontend_video_audio(*audio, sample, error);
            if (result == Result::ok) {
                frontend_video_voice_ = std::make_unique<EngineAudioAdapter>(
                    *this, sample, false, "OriginalPublisherAudio");
                EngineAudioCommand command;
                command.action = EngineVoiceAction::start;
                command.linear_gain = 1;
                command.set_volume = true;
                frontend_video_voice_->submit(command);
                if (menu_->screen_time() > 0)
                    frontend_video_voice_->seek(menu_->screen_time());
            } else
                godot::UtilityFunctions::push_warning(error.c_str());
        }
    }
    if (frontend_sfx_) {
        frontend_sfx_->reap_finished();
        for (const auto& path : menu_->take_sound_events()) {
            std::vector<std::byte> bytes;
            WavePcm sample;
            if (resources_->read(path, bytes) != Result::ok ||
                decode_wave_pcm(bytes, sample) != Result::ok)
                throw std::runtime_error("Frontend sound unavailable: " + path);
            EngineAudioCommand command;
            command.action = EngineVoiceAction::start;
            command.linear_gain = menu_->sfx_gain();
            command.set_volume = true;
            frontend_sfx_->play_one_shot(sample, command, "OriginalMenuSound");
        }
    }
}
godot::Vector2 GameHost::get_frontend_player_position() const {
    if (!race_ || !vehicle_)
        return {};
    const auto position = race_->frontend().map_settings().project(vehicle_->state().position);
    return {static_cast<float>(position[0]), static_cast<float>(position[1])};
}
int64_t GameHost::get_simulation_ticks() const {
    return entry_ ? static_cast<int64_t>(entry_->simulation_ticks()) : 0;
}
double GameHost::get_simulation_alpha() const { return entry_ ? entry_->interpolation_alpha() : 0; }
void GameHost::present_test_frame() {
    try {
        if ((race_ || menu_) && renderer_) {
            const auto size = get_viewport()->get_visible_rect().size;
            renderer_->resize_hud(size.x, size.y);
        }
        if (race_ && vehicle_ && entry_ && !race_preview_) {
            const auto alpha = entry_->interpolation_alpha();
            race_->configure_view_at(renderer_->interpolated_camera_position(alpha));
            renderer_->present_interpolated(alpha);
        }
    } catch (const std::exception& error) {
        godot::UtilityFunctions::push_error(error.what());
        get_tree()->quit(1);
    } catch (...) {
        godot::UtilityFunctions::push_error("Presentation interpolation failed");
        get_tree()->quit(1);
    }
}
int GameHost::get_live_rids() const {
    return renderer_ ? static_cast<int>(renderer_->live_rids()) : 0;
}
void GameHost::submit_tire_audio(const TireAudioInput& input) {
    EngineAudioCommand command;
    if (tire_audio_.update(input, command) != Result::ok)
        throw std::runtime_error("Original tire audio update failed");
    if (command.action == EngineVoiceAction::start) {
        const auto& path = tire_audio_.active_sample_path();
        const auto found = tire_audio_resources_.samples.find(path);
        if (found == tire_audio_resources_.samples.end())
            throw std::runtime_error("Original tire PCM unavailable: " + path);
        if (tire_sample_path_ != path) {
            sfx_audio_adapter_->set_tire_sample(found->second);
            tire_sample_path_ = path;
        }
        ++tire_sound_starts_;
    }
    sfx_audio_adapter_->submit_tire(command);
}
void GameHost::submit_collision_audio(const CollisionAudioEvent& event) {
    if (event.sample == CollisionSample::crash)
        ++crash_sound_starts_;
    sfx_audio_adapter_->play_one_shot(
        collision_audio_resources_.samples.at(static_cast<std::size_t>(event.sample)),
        collision_audio_command(event, collision_audio_resources_),
        event.sample == CollisionSample::crash ? "OriginalCrashVoice" : "OriginalLandVoice");
}
bool GameHost::get_tire_audio_playing() const {
    return sfx_audio_adapter_ && sfx_audio_adapter_->tire_playing();
}
int GameHost::get_collision_sound_count() const {
    return sfx_audio_adapter_ ? static_cast<int>(sfx_audio_adapter_->one_shots_started()) : 0;
}
int GameHost::get_crash_sound_count() const { return crash_sound_starts_; }
int GameHost::get_active_collision_voices() const {
    return sfx_audio_adapter_ ? static_cast<int>(sfx_audio_adapter_->playing_one_shots()) : 0;
}
int GameHost::get_tire_sound_start_count() const { return tire_sound_starts_; }
double GameHost::get_tire_audio_playback_position() const {
    return sfx_audio_adapter_ ? sfx_audio_adapter_->tire_playback_position() : 0;
}
void GameHost::begin_sfx_fixture() {
    try {
        if (!sfx_audio_adapter_)
            return;
        // Explicit PCM acceptance fixture; gameplay triggers remain in the portable vehicle.
        EngineAudioCommand stop;
        stop.action = EngineVoiceAction::stop;
        if (engine_audio_adapter_)
            engine_audio_adapter_->submit(stop);
        race_music_adapter_.reset();
        race_music_.stop();
        engine_audio_.reset();
        sfx_audio_adapter_->reset();
        tire_audio_.reset();
        tire_sound_starts_ = 0;
        crash_sound_starts_ = 0;
        sfx_fixture_ = true;

    } catch (const std::exception& error) {
        godot::UtilityFunctions::push_error(error.what());
        get_tree()->quit(1);
    } catch (...) {
        godot::UtilityFunctions::push_error("Unknown SFX fixture initialization failure");
        get_tree()->quit(1);
    }
}

void GameHost::sfx_fixture_tire(bool active) {
    if (!sfx_fixture_ || !sfx_audio_adapter_ || tire_audio_resources_.samples.empty())
        return;
    try {
        submit_tire_audio({active, 1, tire_audio_resources_.samples.begin()->first,
                           vehicle_ ? vehicle_->state().position : Vec3{}});
    } catch (const std::exception& error) {
        godot::UtilityFunctions::push_error(error.what());
        get_tree()->quit(1);
    } catch (...) {
        godot::UtilityFunctions::push_error("Unknown native SFX fixture failure");
        get_tree()->quit(1);
    }
}
void GameHost::sfx_fixture_collision(bool land, int count) {
    if (!sfx_fixture_ || !sfx_audio_adapter_ || count < 0 || count > 4)
        return;
    try {
        for (int i = 0; i < count; ++i)
            submit_collision_audio({land ? CollisionSample::land : CollisionSample::crash,
                                    vehicle_ ? vehicle_->state().position : Vec3{}});
    } catch (const std::exception& error) {
        godot::UtilityFunctions::push_error(error.what());
        get_tree()->quit(1);
    } catch (...) {
        godot::UtilityFunctions::push_error("Unknown native SFX fixture failure");
        get_tree()->quit(1);
    }
}
bool GameHost::is_engine_audio_playing() const {
    return engine_audio_adapter_ && engine_audio_adapter_->playing();
}
bool GameHost::is_race_music_playing() const {
    return race_music_adapter_ && race_music_adapter_->playing();
}
double GameHost::get_race_music_position() const {
    return race_music_adapter_ ? race_music_adapter_->playback_position() : 0;
}
double GameHost::get_race_music_gain() const {
    return race_music_adapter_ ? race_music_adapter_->linear_gain() : 0;
}
double GameHost::get_race_music_duration() const {
    const auto& sample = race_music_.sample();
    return sample.sample_rate ? static_cast<double>(sample.frame_count()) / sample.sample_rate : 0;
}
void GameHost::seek_race_music_for_test(double seconds) {
    if (race_music_adapter_ && std::isfinite(seconds) && seconds >= 0 &&
        seconds < get_race_music_duration())
        race_music_adapter_->seek(seconds);
}
double GameHost::get_engine_audio_frequency() const {
    return engine_audio_adapter_ ? engine_audio_adapter_->frequency_hz() : 0;
}
double GameHost::get_engine_audio_gain() const {
    return engine_audio_adapter_ ? engine_audio_adapter_->linear_gain() : 0;
}
double GameHost::get_engine_audio_playback_position() const {
    return engine_audio_adapter_ ? engine_audio_adapter_->playback_position() : 0;
}
double GameHost::get_engine_audio_sample_frames() const {
    return engine_audio_adapter_ ? static_cast<double>(engine_audio_adapter_->sample_frames()) : 0;
}
void GameHost::set_skid_marks_visible(bool visible) {
    if (race_)
        race_->set_skid_marks_visible(visible);
}
int GameHost::get_skid_vertex_count() const {
    int result = 0;
    for (const auto& emitter : skid_marks_)
        for (const auto& strip : emitter.strips())
            result += static_cast<int>(strip.vertices.size());
    return result;
}
int GameHost::get_particle_count() const { return get_spark_count() + get_tire_particle_count(); }
int GameHost::get_spark_count() const { return static_cast<int>(sparks_.active_count()); }
int GameHost::get_tire_particle_count() const {
    int result = 0;
    for (const auto& emitter : tire_smoke_)
        result += static_cast<int>(emitter.active_count());
    return result;
}
void GameHost::set_particles_visible(bool visible) {
    if (renderer_)
        renderer_->set_particles_visible(visible);
}
bool GameHost::drive_test_step(double delta, double steering, double throttle, double brake,
                               bool reset, bool handbrake) {
    if (!vehicle_ || !entry_)
        return false;
    if (!std::isfinite(delta) || delta < 0 || !std::isfinite(steering) || std::abs(steering) > 1 ||
        !std::isfinite(throttle) || throttle < 0 || throttle > 1 || !std::isfinite(brake) ||
        brake < 0 || brake > 1)
        return false;
    try {
        InputState input;
        input.steering = static_cast<float>(steering);
        input.throttle = static_cast<float>(throttle);
        input.brake = static_cast<float>(brake);
        input.reset_requested = reset;
        input.handbrake = handbrake;
        return entry_->advance(delta, input) == Result::ok;
    } catch (const std::exception& error) {
        godot::UtilityFunctions::push_error(error.what());
        return false;
    } catch (...) {
        godot::UtilityFunctions::push_error("Drive test step failed");
        return false;
    }
}
godot::Vector3 GameHost::get_vehicle_position() const {
    if (!vehicle_)
        return {};
    const auto p = vehicle_->state().position;
    return {p.x, p.y, p.z};
}
double GameHost::get_vehicle_heading() const {
    return vehicle_ ? vehicle_->state().heading_degrees : 0;
}
double GameHost::get_vehicle_speed() const {
    if (!vehicle_)
        return 0;
    const auto v = vehicle_->state().velocity;
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}
} // namespace roadwars

namespace roadwars {
void GameHost::set_competition_paused(bool paused) {
    if (!session_ || paused == session_->paused())
        return;
    if (paused) {
        auto menu = std::make_unique<MenuFrontend>(*resources_, *renderer_);
        std::string error;
        if (menu->show_pause(race_->frontend().commands(), error) != Result::ok)
            throw std::runtime_error(error);
        pause_menu_ = std::move(menu);
        previous_tree_paused_ = get_tree()->is_paused();
        get_tree()->set_pause(true);
    } else {
        get_tree()->set_pause(previous_tree_paused_);
        pause_menu_.reset();
    }
    session_->set_paused(paused);
    renderer_->set_paused(paused);
    frontend_input_ = {};
    pending_mouse_ = {};
    set_mouse_capture(!paused && !ui_mouse_visible_ && window_focused_);
    if (paused) {
        godot::Input::get_singleton()->set_mouse_mode(godot::Input::MOUSE_MODE_VISIBLE);
        ignore_next_mouse_motion_ = true;
    }
    if (!paused) {
        std::vector<FrontendRacer> racers;
        race_->update_frontend(session_->frontend_frame(racers));
    }
    last_simulation_usec_ = godot::Time::get_singleton()->get_ticks_usec();
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0045e555
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004834ea
bool GameHost::update_pause_menu(double delta) {
    if (!pause_menu_)
        return false;
    std::string error;
    if (pause_menu_->advance(delta, frontend_input_, error) != Result::ok)
        throw std::runtime_error(error);
    frontend_input_ = {};
    const std::string action(pause_menu_->action());
    if (action == "race_pause:CONTINUE")
        set_competition_paused(false);
    else if (action == "race_pause:RESTART RACE") {
        set_competition_paused(false);
        entry_->set_timing(GameEntry::Timing::fixed_simulation);
        InputState reset;
        reset.reset_requested = true;
        if (entry_->advance(1.0 / 60, reset) != Result::ok)
            throw std::runtime_error("Race menu restart failed");
    } else if (action == "race_pause:EXIT TO MAIN MENU") {
        set_competition_paused(false);
        return_to_main_menu();
    } else if (action == "race_pause:EXIT GAME") {
        set_competition_paused(false);
        get_tree()->quit();
    }
    return true;
}
void GameHost::return_to_main_menu() {
    set_mouse_capture(false);
    godot::Input::get_singleton()->set_mouse_mode(godot::Input::MOUSE_MODE_VISIBLE);
    frontend_sfx_ = std::make_unique<SfxAudioAdapter>(*this);
    session_.reset();
    engine_audio_adapter_.reset();
    race_music_adapter_.reset();
    race_music_.stop();
    sfx_audio_adapter_.reset();
    vehicle_.reset();
    race_.reset();
    menu_ = std::make_unique<MenuFrontend>(*resources_, *renderer_);
    std::string error;
    if (menu_->start(error, "main_menu") != Result::ok)
        throw std::runtime_error(error);
    competition_ = race_mode_ = race_results_pending_ = false;
    entry_->state().mode = GameMode::frontend;
    entry_->set_timing(GameEntry::Timing::frontend_realtime);
    if (debug_overlay_)
        debug_overlay_->ui().clear_race_debug();
}
void GameHost::update_race_debug() {
    if (!debug_overlay_)
        return;
    if (!session_ || !developer_build_) {
        debug_overlay_->ui().clear_race_debug();
        return;
    }
    const int inspected = debug_overlay_->ui().race_options().selected_ai;
    if (inspected != inspected_camera_) {
        inspected_camera_ = inspected;
        orbit_camera_.reset();
        race_->apply_camera(session_->camera_for(inspected));
        renderer_->reset_interpolation();
    }
    auto snapshot = session_->debug_snapshot();
    snapshot.camera = renderer_->race_debug_camera(entry_ ? entry_->interpolation_alpha() : 1);
    debug_overlay_->ui().set_race_debug(std::move(snapshot));
}
void GameHost::show_race_results() {
    if (!session_ || !race_results_pending_)
        return;
    std::vector<FrontendPreviewRacer> results;
    for (const auto index : session_->ranking()) {
        const auto& racer = session_->racers()[index];
        FrontendPreviewRacer result;
        result.character = result.name = racer.character;
        result.finished = racer.progress.finished;
        if (result.finished) {
            const int centiseconds = static_cast<int>(racer.finish_time * 100);
            const auto seconds = centiseconds / 100;
            result.total_time = std::to_string(seconds / 60) + ":" +
                                (seconds % 60 < 10 ? "0" : "") + std::to_string(seconds % 60) +
                                "." + (centiseconds % 100 < 10 ? "0" : "") +
                                std::to_string(centiseconds % 100);
        }
        results.push_back(std::move(result));
    }
    const auto track = session_->options().track;
    pause_menu_.reset();
    if (get_tree() && session_ && session_->paused())
        get_tree()->set_pause(previous_tree_paused_);
    session_.reset();
    engine_audio_adapter_.reset();
    race_music_adapter_.reset();
    race_music_.stop();
    sfx_audio_adapter_.reset();
    vehicle_.reset();
    race_.reset();
    if (debug_overlay_)
        debug_overlay_->ui().clear_race_debug();
    menu_ = std::make_unique<MenuFrontend>(*resources_, *renderer_);
    std::string error;
    if (menu_->show_race_results(results, track, error) != Result::ok)
        throw std::runtime_error(error);
    race_results_pending_ = false;
    race_completed_ = true;
    competition_ = race_mode_ = false;
    entry_->state().mode = GameMode::frontend;
    entry_->set_timing(GameEntry::Timing::frontend_realtime);
    ui_mouse_visible_ = false;
    set_mouse_capture(false);
    godot::Input::get_singleton()->set_mouse_mode(godot::Input::MOUSE_MODE_VISIBLE);
    frontend_sfx_ = std::make_unique<SfxAudioAdapter>(*this);
    last_simulation_usec_ = godot::Time::get_singleton()->get_ticks_usec();
}
bool GameHost::competition_test_step(double delta, bool autopilot) {
    try {
        if (!session_ || !entry_ || !std::isfinite(delta) || delta < 0)
            return false;
        if (update_pause_menu(1.0 / 60)) {
            present_test_frame();
            update_race_debug();
            if (debug_overlay_)
                debug_overlay_->frame(1.0 / 60, ui_mouse_visible_ && window_focused_);
            return true;
        }
        competition_test_autopilot_ = autopilot;
        if (entry_->advance(delta, {}) != Result::ok)
            return false;
        if (race_results_pending_)
            show_race_results();
        present_test_frame();
        update_race_debug();
        if (debug_overlay_)
            debug_overlay_->frame(1.0 / 60, ui_mouse_visible_ && window_focused_);
        return true;
    } catch (const std::exception& error) {
        godot::UtilityFunctions::push_error(error.what());
    } catch (...) {
        godot::UtilityFunctions::push_error("Competition test step exception");
    }
    return false;
}
godot::Dictionary GameHost::get_competition_state() const {
    godot::Dictionary state;
    state["completed"] = race_completed_;
    state["phase"] = race_completed_ ? "results" : "unavailable";
    state["participants"] = race_ai_count_ + 1;
    state["laps"] = race_laps_;
    state["ai_rows"] = session_ ? static_cast<int>(session_->player_index()) : 0;
    state["paused"] = session_ && session_->paused();
    state["tree_paused"] = get_tree() && get_tree()->is_paused();
    state["pause_menu"] = pause_menu_ != nullptr;
    state["pause_action"] = pause_menu_
                                ? godot::String::utf8(std::string(pause_menu_->action()).c_str())
                                : godot::String();
    state["camera_target"] = debug_overlay_ ? debug_overlay_->ui().race_options().selected_ai : -1;
    state["pause_selection"] = pause_menu_ ? pause_menu_->selected() : -1;
    if (session_) {
        state["countdown_remaining"] = session_->start_sequence().displayed_remaining();
        state["countdown_cues"] = session_->start_sequence().cue_count();
        state["countdown_digit"] =
            RaceStart::digit(session_->start_sequence().displayed_remaining());
    }
    godot::Array flags;
    if (debug_overlay_) {
        const auto& options = debug_overlay_->ui().race_options();
        flags.push_back(options.show_ai_states);
        flags.push_back(options.show_route);
        flags.push_back(options.show_checkpoints);
        flags.push_back(options.show_collision_shapes);
    }
    state["auto_drive"] = debug_overlay_ && debug_overlay_->ui().race_options().auto_drive;
    state["debug_flags"] = flags;
    if (session_) {
        state["phase"] = session_->paused()                          ? "paused"
                         : session_->phase() == RacePhase::countdown ? "countdown"
                                                                     : "racing";
        state["time"] = session_->time();
        state["ticks"] = static_cast<int64_t>(session_->ticks());
        state["player_node"] = session_->racers().back().progress.current_node;
        state["player_laps"] = session_->racers().back().progress.completed_laps;
    }
    return state;
}
godot::Vector2 GameHost::get_race_debug_inspect_position(int index) const {
    if (!debug_overlay_)
        return {};
    const auto position = debug_overlay_->ui().race_inspect_position(index);
    return {position[0], position[1]};
}
godot::Vector2 GameHost::get_race_debug_checkbox_position(int index) const {
    if (!debug_overlay_)
        return {};
    const auto position =
        debug_overlay_->ui().race_checkbox_position(static_cast<std::size_t>(index));
    return {position[0], position[1]};
}
int GameHost::get_race_debug_line_count() const {
    return debug_overlay_ ? static_cast<int>(debug_overlay_->ui().race_debug_line_count()) : 0;
}
} // namespace roadwars
