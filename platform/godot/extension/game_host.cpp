#include "platform/godot/extension/game_host.hpp"
#include "game_src/core/not_implemented.hpp"
#include "platform/godot/extension/frontend_video_audio.hpp"
#include "platform/godot/extension/original_weapon_input.hpp"
#include <algorithm>
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
#include <utility>

namespace roadwars {
void GameHost::_bind_methods() {
    godot::ClassDB::bind_method(godot::D_METHOD("get_frame_profile_trace"),
                                &GameHost::get_frame_profile_trace);
    godot::ClassDB::bind_method(godot::D_METHOD("get_frame_timing_state"),
                                &GameHost::get_frame_timing_state);
    godot::ClassDB::bind_method(godot::D_METHOD("test_interpolation_history"),
                                &GameHost::test_interpolation_history);
    godot::ClassDB::bind_method(godot::D_METHOD("get_presentation_state"),
                                &GameHost::get_presentation_state);
    godot::ClassDB::bind_method(godot::D_METHOD("request_race_debug_track", "track"),
                                &GameHost::request_race_debug_track);
    godot::ClassDB::bind_method(godot::D_METHOD("get_race_debug_track_position", "index"),
                                &GameHost::get_race_debug_track_position);
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
    godot::ClassDB::bind_method(
        godot::D_METHOD("competition_weapon_test_step", "delta", "slot", "nitrous"),
        &GameHost::competition_weapon_test_step);
    godot::ClassDB::bind_method(godot::D_METHOD("cycle_camera_view"), &GameHost::cycle_camera_view);
    godot::ClassDB::bind_method(godot::D_METHOD("set_camera_view_for_test", "view"), &GameHost::set_camera_view_for_test);
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
        frontend_music_voice_.reset();
        frontend_music_.stop();
        frontend_music_path_.clear();
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
        combat_audio_adapter_.reset();
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
        frontend_music_voice_.reset();
        frontend_music_.stop();
        frontend_music_path_.clear();
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
        combat_audio_adapter_.reset();
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
    // Modern launch seed; explicit --race-seed retains reproducible diagnostics.
    diagnostic_race_seed_ = static_cast<std::uint32_t>(
        static_cast<std::uint64_t>(godot::Time::get_singleton()->get_unix_time_from_system() *
                                   1000000.0) ^
        godot::Time::get_singleton()->get_ticks_usec());
    bool enable_debug_overlay = true;
    for (const auto argument : godot::OS::get_singleton()->get_cmdline_user_args()) {
        if (argument == "--frame-trace")
            frame_trace_.enable();
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
        if (argument.begins_with("--race-seed=")) {
            const auto value = argument.trim_prefix("--race-seed=");
            diagnostic_race_seed_ = value.is_valid_int() ? value.to_int() : -1;
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
        race_loading_task_.reset();
        race_assets_prepared_ = false;
        race_completed_ = false;
        simulation_failed_ = false;
        race_results_pending_ = false;
        if (debug_overlay_)
            debug_overlay_->ui().clear_race_debug();
        entry_.reset();
        frontend_video_voice_.reset();
        frontend_sfx_.reset();
        frontend_music_voice_.reset();
        frontend_music_.stop();
        frontend_music_path_.clear();
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
        combat_audio_adapter_.reset();
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
        frontend_music_voice_.reset();
        frontend_music_.stop();
        frontend_music_path_.clear();
        frontend_audio_screen_.clear();
        menu_.reset();
        engine_audio_adapter_.reset();
        race_music_adapter_.reset();
        race_music_.stop();
        sfx_audio_adapter_.reset();
        pause_menu_.reset();
        if (get_tree() && session_ && session_->paused())
            get_tree()->set_pause(previous_tree_paused_);
        combat_audio_adapter_.reset();
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
        frontend_music_voice_.reset();
        frontend_music_.stop();
        frontend_music_path_.clear();
        frontend_audio_screen_.clear();
        menu_.reset();
        engine_audio_adapter_.reset();
        race_music_adapter_.reset();
        race_music_.stop();
        sfx_audio_adapter_.reset();
        pause_menu_.reset();
        if (get_tree() && session_ && session_->paused())
            get_tree()->set_pause(previous_tree_paused_);
        combat_audio_adapter_.reset();
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
        weapon_modifiers_ = {};
        pressed_keyboard_keys_.clear();
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
        weapon_modifiers_.update(**key);
        const auto physical = key->get_physical_keycode();
        const auto code = physical == godot::KEY_NONE ? key->get_keycode() : physical;
        if (code != godot::KEY_NONE) {
            const auto identity = static_cast<std::uint64_t>(code) |
                                  (static_cast<std::uint64_t>(key->get_location()) << 32);
            if (key->is_pressed())
                pressed_keyboard_keys_.insert(identity);
            else
                pressed_keyboard_keys_.erase(identity);
        }
    }
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
        const auto* frontend = pause_menu_ ? pause_menu_.get() : menu_.get();
        if (frontend->assertion_message().empty() && debug_overlay_ && ui_mouse_visible_ &&
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
                if (session_->ending() == RaceEndReason::none)
                    set_competition_paused(true);
                else {
                    race_results_pending_ = true;
                    show_race_results();
                }
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
    if (key.is_valid() && key->is_pressed() && !key->is_echo()) {
        const auto code = key->get_physical_keycode() == godot::KEY_NONE ? key->get_keycode()
                                                                       : key->get_physical_keycode();
        if (code == godot::KEY_C || code == godot::KEY_F1) {
            cycle_camera_view();
            get_viewport()->set_input_as_handled();
            return;
        }
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
    FrameTraceScope trace(frame_trace_);
    if (frame_trace_.enabled()) {
        const auto probe_start = godot::Time::get_singleton()->get_ticks_usec();
        auto* server = godot::RenderingServer::get_singleton();
        const auto viewport = get_viewport()->get_viewport_rid();
        server->viewport_set_measure_render_time(viewport, true);
        trace.record.frame = godot::Engine::get_singleton()->get_process_frames();
        trace.record.ticks = entry_ ? entry_->simulation_ticks() : 0;
        trace.record.render_cpu_ms = server->viewport_get_measured_render_time_cpu(viewport);
        trace.record.render_gpu_ms = server->viewport_get_measured_render_time_gpu(viewport);
        for (std::size_t i = 0; i < trace.record.pipelines.size(); ++i)
            trace.record.pipelines[i] =
                server->get_rendering_info(static_cast<godot::RenderingServer::RenderingInfo>(
                    godot::RenderingServer::RENDERING_INFO_PIPELINE_COMPILATIONS_CANVAS + i));
        trace.record.probe_usec = godot::Time::get_singleton()->get_ticks_usec() - probe_start;
    }
    try {
        const auto now = godot::Time::get_singleton()->get_ticks_usec();
        const auto elapsed = last_simulation_usec_
                                 ? static_cast<double>(now - last_simulation_usec_) / 1000000.0
                                 : 0.0;
        last_simulation_usec_ = now;
        if (debug_overlay_)
            debug_overlay_->begin_frame(now);
        {
            FrameStageTimer timer(FrameStage::debug_snapshot);
            update_race_debug();
        }
        if (debug_overlay_) {
            FrameStageTimer timer(FrameStage::overlay);
            debug_overlay_->frame(delta, ui_mouse_visible_ && window_focused_);
        }
        // Rebuilding a race must occur after ImGui has finished traversing its snapshot.
        if (apply_race_debug_track_request()) {
            update_race_debug();
            present_test_frame();
            return;
        }
        if (!entry_ || simulation_failed_)
            return;
        if (apply_frontend_race_request(elapsed))
            return;
        if (update_pause_menu(delta)) {
            present_test_frame();
            return;
        }
        InputState input;
        input.any_key_down = window_focused_ && !pressed_keyboard_keys_.empty();
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
            if (competition_)
                sample_weapon_keys(weapon_keys_, input, weapon_modifiers_);
        }
        const auto scale = godot::Engine::get_singleton()->get_time_scale();
        if (race_mode_ && !race_preview_ && scale == 0) {
            present_test_frame();
            return;
        }
        const double simulation_delta = menu_                          ? elapsed
                                        : race_mode_ && !race_preview_ ? elapsed * scale
                                                                       : delta;
        {
            FrameStageTimer timer(FrameStage::simulation);
            if (entry_->advance(simulation_delta, input) != Result::ok)
                throw std::runtime_error("Runtime update failed");
        }
        if (race_results_pending_)
            show_race_results();
        {
            FrameStageTimer timer(FrameStage::presentation);
            present_test_frame();
        }
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
        frontend_music_voice_.reset();
        frontend_music_.stop();
        frontend_music_path_.clear();
        ui_mouse_visible_ = true;
        set_mouse_capture(false);
        godot::UtilityFunctions::push_error(error.what());
        if (debug_overlay_)
            debug_overlay_->show_error(error.what());
    } catch (...) {
        simulation_failed_ = true;
        frontend_video_voice_.reset();
        frontend_sfx_.reset();
        frontend_music_voice_.reset();
        frontend_music_.stop();
        frontend_music_path_.clear();
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
godot::Array GameHost::get_frame_profile_trace() const {
    godot::Array result;
    const auto& records = frame_trace_.records();
    for (std::size_t n = 0; n < records.size(); ++n) {
        const auto& record = records[(frame_trace_.offset() + n) % records.size()];
        godot::Dictionary row;
        row["frame"] = static_cast<int64_t>(record.frame);
        row["start_us"] = static_cast<int64_t>(record.start_usec);
        row["interval_us"] = static_cast<int64_t>(record.interval_usec);
        row["host_us"] = static_cast<int64_t>(record.total_usec);
        row["ticks"] = static_cast<int64_t>(record.ticks);
        row["probe_us"] = static_cast<int64_t>(record.probe_usec);
        row["render_cpu_ms"] = record.render_cpu_ms;
        row["render_gpu_ms"] = record.render_gpu_ms;
        row["events"] = godot::String::utf8(record.events.c_str());
        const std::array<const char*, 10> stages{
            "debug_us",       "overlay_us",      "display_poll_us", "dpi_poll_us",
            "simulation_us",  "presentation_us", "race_logic_us",   "combat_visuals_us",
            "skid_upload_us", "audio_us"};
        const std::array<const char*, 8> resources{"meshes",     "instances", "textures",
                                                   "materials",  "particles", "audio_voices",
                                                   "file_reads", "ui_items"};
        const std::array<const char*, 5> pipelines{"pipeline_canvas", "pipeline_mesh",
                                                   "pipeline_surface", "pipeline_draw",
                                                   "pipeline_specialization"};
        for (std::size_t i = 0; i < stages.size(); ++i)
            row[stages[i]] = static_cast<int64_t>(record.stages[i]);
        for (std::size_t i = 0; i < resources.size(); ++i)
            row[resources[i]] = static_cast<int64_t>(record.resources[i]);
        for (std::size_t i = 0; i < pipelines.size(); ++i)
            row[pipelines[i]] = record.pipelines[i];
        result.push_back(row);
    }
    return result;
}
godot::Dictionary GameHost::get_frame_timing_state() const {
    godot::Dictionary result;
    if (!debug_overlay_)
        return result;
    const auto& ui = debug_overlay_->ui();
    const auto& history = ui.frame_timing();
    const float target = ui.refresh_hz() > 0 ? 1000.0f / ui.refresh_hz() : 0;
    const auto summary = history.summary(target);
    result["refresh_hz"] = ui.refresh_hz();
    result["vsync"] = godot::String::utf8(ui.vsync().c_str());
    result["target_ms"] = target;
    result["mean_ms"] = summary.mean;
    result["p95_ms"] = summary.p95;
    result["p99_ms"] = summary.p99;
    result["max_ms"] = summary.maximum;
    result["late_frames"] = static_cast<int64_t>(summary.late);
    result["sample_count"] = history.count();
    godot::Array samples;
    for (int i = 0; i < history.count(); ++i)
        samples.push_back(
            history.data()[(history.offset() + i) % extensions::FrameTiming::kCapacity]);
    result["samples_ms"] = samples;
    return result;
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
    race_loading_task_.reset();
    set_mouse_capture(false);
    entry_.reset();
    frontend_video_voice_.reset();
    frontend_sfx_.reset();
    frontend_music_voice_.reset();
    frontend_music_.stop();
    frontend_music_path_.clear();
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
    combat_audio_adapter_.reset();
    session_.reset();
    vehicle_.reset();
    race_.reset();
    resources_.reset();
    files_.reset();
    runtime_.reset();
    renderer_.reset();
    debug_overlay_.reset();
}
LoadingTask GameHost::prepare_race() {
    previous_cockpit_states_.clear();
    current_cockpit_states_.clear();
    camera_cut_pending_ = true;
    co_yield LoadingProgress{0, "STARTING"};
    if (game_directory_.is_empty())
        throw std::runtime_error("The launcher must supply game_dir from config.local.json");
    if (!resources_) {
        files_ = std::make_unique<GameFiles>(game_directory_);
        resources_ = std::make_unique<ResourceStore>(*files_);
        if (resources_->open() != Result::ok)
            throw std::runtime_error("Original game archive could not be opened");
    }
    std::string error;
    const BootOptions options{track_name_.utf8().get_data(), vehicle_name_.utf8().get_data()};
    if (initialize_test_game(*resources_, options, initialized_, error) != Result::ok)
        throw std::runtime_error(error);
    renderer_->set_interpolation_enabled(!race_preview_);
    race_ = std::make_unique<RacePresentation>(*renderer_);
    TireSelection initial_tires;
    if (competition_) {
        load_race_debug_tracks();
        RaceLoadoutTable table;
        if (table.load(*resources_, options.track, error) != Result::ok)
            throw std::runtime_error(error);
        initial_tires = table.tires();
        if (load_original_weapon_keys(*resources_, weapon_keys_, error) != Result::ok ||
            validate_weapon_keys(weapon_keys_, error) != Result::ok)
            throw std::runtime_error(error);
    }
    if (race_->load(*resources_, options.track, options.vehicle, error, initial_tires) !=
        Result::ok)
        throw std::runtime_error(error);
    co_yield LoadingProgress{.1f, "LOADING TRACK"};
    // Explicit free-driving character selection; car/character selection remains
    // separate in the original game and will come from the recovered race setup.
    race_->set_developer_build(developer_build_);
    if (race_->configure_frontend(*resources_, options.track, "Animal", error) != Result::ok)
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
    co_yield LoadingProgress{.2f, "LOADING PLAYER"};
    if (!race_preview_) {
        float effects_scalar{};
        if (initialized_.global_config.number("ROADWARS", "SPECIAL_EFFECTS_SCALAR",
                                              effects_scalar) != Result::ok)
            throw std::runtime_error("Original SPECIAL_EFFECTS_SCALAR is unavailable");
        if (!competition_) {
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
        }
        VehicleParameters parameters;
        if (load_vehicle_metrics(*resources_, options.vehicle, parameters, error) != Result::ok)
            throw std::runtime_error(error);
        parameters.tires = initial_tires;
        if (load_vehicle_geometry(*resources_, options.vehicle, parameters, error) != Result::ok)
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
        if (vehicle_->initialize(std::move(parameters), race_->collision(), error) != Result::ok)
            throw std::runtime_error(error);
        if (load_engine_audio(*resources_, options.vehicle, engine_audio_resources_, error) !=
            Result::ok)
            throw std::runtime_error(error);
        if (engine_audio_.initialize(engine_audio_resources_.sample.sample_rate,
                                     engine_audio_resources_.sfx_volume) != Result::ok)
            throw std::runtime_error("Original engine voice initialization failed");
        engine_audio_adapter_ =
            std::make_unique<EngineAudioAdapter>(*this, engine_audio_resources_.sample);
        if (race_music_.load(*resources_, error) != Result::ok)
            throw std::runtime_error(error);
        race_music_adapter_ = std::make_unique<EngineAudioAdapter>(*this, race_music_.sample(),
                                                                   true, "OriginalRaceMusic");
        EngineAudioCommand music;
        if (!competition_) {
            if (race_music_.start(music) != Result::ok)
                throw std::runtime_error("Original race music start failed");
            race_music_adapter_->submit(music);
        }
        if (load_collision_audio(*resources_, collision_audio_resources_, error) != Result::ok ||
            load_tire_audio(*resources_, options.track, tire_audio_resources_, error) != Result::ok)
            throw std::runtime_error(error);
        if (tire_audio_.initialize(tire_audio_resources_.sfx_volume) != Result::ok)
            throw std::runtime_error("Original tire voice initialization failed");
        sfx_audio_adapter_ = std::make_unique<SfxAudioAdapter>(*this);
    }
    co_yield LoadingProgress{.3f, "LOADING AUDIO"};
    if (competition_) {
        session_ = std::make_unique<RaceSession>();
        RaceOptions setup{options.track, options.vehicle, race_laps_, race_ai_count_};
        if (diagnostic_race_seed_ < 0 || diagnostic_race_seed_ > 0xffffffffLL)
            throw std::runtime_error("Diagnostic race seed must fit an unsigned 32-bit value");
        setup.seed = static_cast<std::uint32_t>(diagnostic_race_seed_);
        setup.player_loadout = menu_player_loadout_;
        if (session_->load(*resources_, race_->collision(), setup, *vehicle_, error) != Result::ok)
            throw std::runtime_error(error);
        co_yield LoadingProgress{.45f, "LOADING VEHICLES"};
        combat_audio_adapter_ =
            std::make_unique<CombatAudioAdapter>(*this, session_->audio_resources());
        std::vector<std::string> characters;
        for (const auto& racer : session_->racers())
            characters.push_back(racer.character);
        if (race_->replace_player_visual(*resources_, options.vehicle,
                                         session_->racers()[session_->player_index()].loadout,
                                         error) != Result::ok)
            throw std::runtime_error(error);
        if (race_->configure_racers(*resources_, options.track, characters, error) != Result::ok)
            throw std::runtime_error(error);
        if (race_->configure_equipment_hud(
                *resources_, options.vehicle,
                session_->racers()[session_->player_index()].loadout.weapons, error) != Result::ok)
            throw std::runtime_error(error);
        co_yield LoadingProgress{.5f, "LOADED PLAYER"};
        for (std::size_t i = 0; i < session_->player_index(); ++i) {
            if (race_->add_opponent(*resources_, characters[i], error,
                                    session_->racers()[i].loadout.tires,
                                    session_->racers()[i].loadout.paint_job) != Result::ok)
                throw std::runtime_error(error);
            race_->update_opponent(i, *session_->racers()[i].vehicle, 0, true);
            co_yield LoadingProgress{.5f + .35f * static_cast<float>(i + 1) /
                                               static_cast<float>(session_->player_index()),
                                     "LOADED VEHICLE " + std::to_string(i + 2)};
        }
        race_->update_wheels(*vehicle_, 0, 0, true);
        race_->update_vehicle(vehicle_->transform());
        if (race_->prepare_combat(*resources_, *session_, error) != Result::ok)
            throw std::runtime_error(error);
        co_yield LoadingProgress{.95f, "LOADING EFFECTS"};
        if (race_->update_combat(*resources_, *session_, error) != Result::ok)
            throw std::runtime_error(error);
        race_->apply_camera(session_->start_sequence().camera());
        renderer_->reset_interpolation();
        std::vector<FrontendRacer> racers;
        auto frame = session_->frontend_frame(racers);
        frame.developer_build = developer_build_;
        race_->update_frontend(frame);
    }
    godot::UtilityFunctions::print("ROADWARS_RACE_LOADED: ", track_name_, " / ", vehicle_name_,
                                   race_preview_ ? " (static resource preview)"
                                                 : " (original logic trace)");
    co_yield LoadingProgress{1, "FINISHED"};
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
            menu_->set_race_seed(static_cast<std::uint32_t>(diagnostic_race_seed_));
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
            if (race_assets_prepared_) {
                race_assets_prepared_ = false;
            } else {
                auto loading = prepare_race();
                while (loading.advance()) {
                }
            }
        } else if (stage == LoopStage::single_race_004a4f57 && vehicle_) {
            if (session_ && session_->paused()) {
                std::vector<FrontendRacer> racers;
                auto frame = session_->frontend_frame(racers);
                frame.developer_build = developer_build_;
                return race_->update_frontend(frame);
            }
            const auto player_generation = vehicle_->pose_generation();
            std::vector<std::uint64_t> generations;
            if (session_)
                for (const auto& racer : session_->racers())
                    generations.push_back(racer.vehicle->pose_generation());
            capture_cockpit_states(true);
            while (pending_camera_cycles_ > 0) {
                camera_view_ = next_camera_view(camera_view_);
                --pending_camera_cycles_;
                camera_cut_pending_ = true;
            }
            if (camera_cut_pending_) { orbit_camera_.reset(); pending_mouse_ = {}; }
            renderer_->begin_simulation_tick();
            race_->update_track_animation(delta);
            if (session_) {
                std::string error;
                session_->set_player_autopilot(
                    competition_test_autopilot_ ||
                    (debug_overlay_ && debug_overlay_->ui().race_options().auto_drive));
                if (combat_audio_adapter_) {
                    if (input.reset_requested)
                        combat_audio_adapter_->reset();
                    for (std::size_t i = 0; i < 2 + session_->racers().size(); ++i)
                        session_->set_audio_voice_alive(i, combat_audio_adapter_->loop_alive(i));
                }
                {
                    FrameStageTimer timer(FrameStage::race_logic);
                    if (session_->advance(delta, input, error) != Result::ok)
                        throw std::runtime_error(error);
                }
                {
                    FrameStageTimer timer(FrameStage::combat_visuals);
                    if (race_->update_combat(*resources_, *session_, error) != Result::ok)
                        throw std::runtime_error(error);
                }
                for (std::size_t i = 0; i < session_->player_index(); ++i)
                    race_->update_opponent(i, *session_->racers()[i].vehicle, delta,
                                           input.reset_requested);
                race_results_pending_ = session_->idle_return_requested();
            } else
                vehicle_->update(delta, input);
            if (input.reset_requested)
                for (auto& emitter : skid_marks_)
                    emitter.reset();
            for (std::size_t i = 0; i < 4; ++i)
                if (skid_marks_[i].advance(delta, vehicle_->tire_particle_sources()[i]) !=
                    Result::ok)
                    throw std::runtime_error("Original skid-mark update failed");
            {
                FrameStageTimer timer(FrameStage::skid_upload);
                if (race_->update_skid_marks(skid_marks_) != Result::ok)
                    throw std::runtime_error("Native skid-mark submission failed");
            }
            if (!session_) {
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
            }
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
                } else if (camera_view_ == CameraView::near_chase &&
                           orbit_camera_.update(original_camera, delta, pending_mouse_, camera) !=
                               Result::ok)
                    throw std::runtime_error("Race camera orbit update failed");
            } else if (chase_camera_.update(vehicle_->state(), vehicle_->parameters(),
                                            &race_->collision(), delta,
                                            original_camera) != Result::ok ||
                       (camera_view_ == CameraView::near_chase &&
                        orbit_camera_.update(original_camera, delta, pending_mouse_, camera) !=
                           Result::ok))
                throw std::runtime_error("Original chase camera update failed");
            if (camera_view_ != CameraView::near_chase) {
                const auto target = camera_target_index();
                const auto& car = session_ ? *session_->racers()[target].vehicle : *vehicle_;
                auto settings = session_ ? session_->racers()[target].chase_camera.settings()
                                         : chase_camera_.settings();
                settings.body_local_angles = {0, car.body_orientation().body_pitch,
                                               car.body_orientation().body_roll};
                if (camera_view_frame(camera_view_, car.state(), car.parameters(), settings,
                                      &race_->collision(), original_camera, camera) != Result::ok)
                    throw std::runtime_error("Vehicle camera view failed");
                if (camera_view_ == CameraView::far_chase) {
                    const auto far_frame = camera;
                    if (orbit_camera_.update(far_frame, delta, pending_mouse_, camera) != Result::ok)
                        throw std::runtime_error("Far chase orbit failed");
                }
            }
            pending_mouse_ = {};
            race_->apply_camera(camera);
            const bool first_person = camera_view_ == CameraView::hood || camera_view_ == CameraView::cockpit;
            renderer_->set_first_person(first_person);
            race_->set_camera_vehicle_hidden(first_person ? static_cast<int>(camera_target_index()) : -1,
                                             session_ ? session_->player_index() : 0);
            capture_cockpit_states(false);
            if (camera_cut_pending_) {
                orbit_camera_.reset();
                renderer_->reset_camera_interpolation();
                previous_cockpit_states_ = current_cockpit_states_;
                camera_cut_pending_ = false;
            }
            if (session_) {
                const int inspected =
                    debug_overlay_ ? debug_overlay_->ui().race_options().selected_ai : -1;
                const auto target =
                    inspected >= 0 && static_cast<std::size_t>(inspected) < session_->player_index()
                        ? static_cast<std::size_t>(inspected)
                        : session_->player_index();
                for (std::size_t i = 0; i < generations.size(); ++i)
                    if (generations[i] != session_->racers()[i].vehicle->pose_generation()) {
                        race_->snap_racer(i, session_->player_index());
                        if (i == target)
                            renderer_->reset_camera_interpolation();
                    }
            } else if (player_generation != vehicle_->pose_generation()) {
                race_->snap_racer(0, 0);
                renderer_->reset_camera_interpolation();
            }
            if (input.reset_requested || (entry_ && entry_->simulation_ticks() == 0)) {
                renderer_->reset_interpolation();
                previous_cockpit_states_ = current_cockpit_states_;
            }
            if (session_)
                for (std::size_t i = 0; i < generations.size(); ++i)
                    if (generations[i] != session_->racers()[i].vehicle->pose_generation())
                        previous_cockpit_states_[i] = current_cockpit_states_[i];
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
                    if (combat_audio_adapter_) {
                        for (const auto& event : session_->audio_events()) {
                            CombatAudioCommand command;
                            if (combat_audio_command(event, session_->audio_resources(), command) !=
                                    Result::ok ||
                                combat_audio_adapter_->submit_one_shot(command) != Result::ok)
                                throw std::runtime_error("Native combat one-shot output failed");
                        }
                        for (const auto& loop : session_->audio_loop_commands())
                            if (combat_audio_adapter_->submit_loop(loop.voice, loop.command) !=
                                Result::ok)
                                throw std::runtime_error("Native combat loop output failed");
                        combat_audio_adapter_->reap_finished();
                    }
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
        if (apply_frontend_race_request(elapsed))
            return !simulation_failed_;
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
    const auto music_path = menu_->music_path();
    if (music_path != frontend_music_path_ || (!music_path.empty() && !frontend_music_voice_)) {
        frontend_music_voice_.reset();
        frontend_music_.stop();
        frontend_music_path_.clear();
        frontend_music_path_ = music_path;
        if (!music_path.empty()) {
            std::string error;
            const auto result =
                music_path == "Sfx/SoundBackgroundMusic.wav"
                    ? frontend_music_.load_menu(*resources_, error)
                    : frontend_music_.load_character(*resources_, menu_->selected_character(), error);
            if (result != Result::ok)
                throw std::runtime_error(error);
            frontend_music_voice_ = std::make_unique<EngineAudioAdapter>(
                *this, frontend_music_.sample(), true, "OriginalFrontendMusic");
            EngineAudioCommand command;
            if (frontend_music_.start(command) != Result::ok)
                throw std::runtime_error("Original frontend music start failed");
            command.linear_gain *= menu_->music_gain_scale();
            frontend_music_voice_->submit(command);
        }
    }
    if (frontend_music_voice_)
        frontend_music_voice_->submit(frontend_music_.gain(menu_->music_gain_scale()));
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
bool GameHost::test_interpolation_history() const {
    return ServerRenderer::test_interpolation_history();
}
godot::Dictionary GameHost::get_presentation_state() const {
    return renderer_ ? renderer_->presentation_snapshot() : godot::Dictionary{};
}
std::size_t GameHost::camera_target_index() const {
    const int inspected = debug_overlay_ ? debug_overlay_->ui().race_options().selected_ai : -1;
    return session_ && inspected >= 0 && static_cast<std::size_t>(inspected) < session_->player_index()
               ? static_cast<std::size_t>(inspected) : session_ ? session_->player_index() : 0;
}
void GameHost::cut_camera_to_target() {
    if (!race_ || !vehicle_ || !renderer_)
        return;
    const auto target = camera_target_index();
    const auto& car = session_ ? *session_->racers()[target].vehicle : *vehicle_;
    auto settings = session_ ? session_->racers()[target].chase_camera.settings()
                             : chase_camera_.settings();
    settings.body_local_angles = {0, car.body_orientation().body_pitch, car.body_orientation().body_roll};
    const auto& baseline = session_ ? session_->camera_for(static_cast<int>(target))
                                    : race_->camera_frame();
    CameraFrame frame;
    if (camera_view_frame(camera_view_, car.state(), car.parameters(), settings,
                          &race_->collision(), baseline, frame) != Result::ok)
        throw std::runtime_error("Camera target cut failed");
    race_->apply_camera(frame);
    renderer_->reset_camera_interpolation();
    const bool first_person = camera_view_ == CameraView::hood || camera_view_ == CameraView::cockpit;
    renderer_->set_first_person(first_person);
    race_->set_camera_vehicle_hidden(first_person ? static_cast<int>(target) : -1,
                                     session_ ? session_->player_index() : 0);
    if (current_cockpit_states_.empty())
        capture_cockpit_states(false);
    if (target < current_cockpit_states_.size() && target < previous_cockpit_states_.size())
        previous_cockpit_states_[target] = current_cockpit_states_[target];
    camera_cut_pending_ = false;
}
void GameHost::cycle_camera_view() {
    if (race_ && vehicle_ && !menu_ && !pause_menu_ && (!session_ || !session_->paused()))
        ++pending_camera_cycles_;
}
void GameHost::set_camera_view_for_test(int view) {
    if (view < 0 || view > 3 || !race_ || !vehicle_)
        return;
    camera_view_ = static_cast<CameraView>(view);
    pending_camera_cycles_ = 0;
    camera_cut_pending_ = true;
    orbit_camera_.reset();
    pending_mouse_ = {};
}
void GameHost::capture_cockpit_states(bool previous) {
    auto& output = previous ? previous_cockpit_states_ : current_cockpit_states_;
    output.clear();
    if (session_) {
        for (const auto& racer : session_->racers())
            output.push_back(racer.vehicle->state());
    } else if (vehicle_)
        output.push_back(vehicle_->state());
    if (!previous && previous_cockpit_states_.size() != output.size())
        previous_cockpit_states_ = output;
}
void GameHost::present_race_frontend(double alpha) {
    if (!race_ || !vehicle_ || !entry_ || race_preview_ || pause_menu_)
        return;
    if (current_cockpit_states_.empty())
        capture_cockpit_states(false);
    const auto target = camera_target_index();
    if (target >= current_cockpit_states_.size())
        return;
    const auto& before = previous_cockpit_states_[target];
    const auto& after = current_cockpit_states_[target];
    const auto blend = [&](float a, float b) { return a + (b - a) * static_cast<float>(alpha); };
    std::vector<FrontendRacer> racers;
    FrontendFrame frame;
    if (session_)
        frame = session_->frontend_frame(racers);
    else {
        racers.push_back({vehicle_->state().position, 0});
        frame.racers = racers;
        frame.speed = after.cached_speed;
        frame.rpm = after.rpm;
        frame.gear_index = static_cast<int>(after.gear_index);
        frame.reverse = after.reverse_engaged;
    }
    frame.developer_build = developer_build_;
    frame.cockpit = camera_view_ == CameraView::cockpit;
    frame.cockpit_character = target;
    const auto viewport_size = get_viewport()->get_visible_rect().size;
    frame.viewport_aspect = viewport_size.x > 0 && viewport_size.y > 0
                                ? viewport_size.x / viewport_size.y : 4.f / 3;
    if (frame.cockpit) {
        const auto& car = session_ ? *session_->racers()[target].vehicle : *vehicle_;
        frame.speed = blend(before.cached_speed, after.cached_speed);
        frame.rpm = blend(before.rpm, after.rpm);
        frame.steering_fraction = blend(before.steering_degrees, after.steering_degrees) /
                                  car.parameters().max_wheel_angle_degrees;
        frame.gear_index = static_cast<int>(after.gear_index);
        frame.reverse = after.reverse_engaged;
        if (session_) {
            frame.cockpit_weapon_names = session_->racers()[target].loadout.weapons;
            auto equipment = frame.equipment.value_or(FrontendEquipmentFrame{});
            for (std::size_t slot = 0; slot < 3; ++slot) {
                const auto weapon = car.combat()->slot(static_cast<WeaponSlot>(slot));
                equipment.ammunition[slot] = weapon.ammunition;
                equipment.present[slot] = weapon.descriptor != nullptr;
            }
            frame.equipment = equipment;
        }
    }
    presented_cockpit_rpm_ = frame.rpm;
    presented_cockpit_steering_ = frame.steering_fraction;
    if (race_->update_frontend(frame) != Result::ok)
        throw std::runtime_error("Cockpit frontend presentation failed");
}
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
            present_race_frontend(alpha);
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
int GameHost::get_spark_count() const {
    if (!session_)
        return static_cast<int>(sparks_.active_count());
    int result{};
    for (const auto& particles : session_->vehicle_particles())
        result += static_cast<int>(particles.sparks.active_count());
    return result;
}
int GameHost::get_tire_particle_count() const {
    int result = 0;
    if (session_) {
        for (const auto& particles : session_->vehicle_particles())
            for (const auto& emitter : particles.tires)
                result += static_cast<int>(emitter.active_count());
        return result;
    }
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
    if (combat_audio_adapter_)
        combat_audio_adapter_->set_paused(paused);
    if (paused) {
        auto menu = std::make_unique<MenuFrontend>(*resources_, *renderer_);
        std::string error;
        if (menu->show_pause(race_->frontend().commands(), error) != Result::ok)
            throw std::runtime_error(error);
        pause_menu_ = std::move(menu);
        ui_mouse_visible_ = false; // Pause menu takes focus; F8 can refocus debug UI.
        if (debug_overlay_)
            debug_overlay_->focus(false);
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
bool GameHost::apply_frontend_race_request(double elapsed) {
    if (!menu_ || !menu_->race_requested()) {
        frontend_loading_presented_ = false;
        return false;
    }
    const auto frame = godot::Engine::get_singleton()->get_process_frames();
    if (!frontend_loading_presented_) {
        frontend_loading_presented_ = true;
        frontend_loading_frame_ = frame;
        return false;
    }
    if (frame <= frontend_loading_frame_ + 1)
        return false;
    std::string error;
    if (menu_->advance(elapsed, {}, error) != Result::ok)
        throw std::runtime_error(error);
    // Both race routes must finish the Loading intro before synchronous asset work.
    // Otherwise the first expensive stage can leave the new theme at zero gain.
    update_frontend_audio();
    if (!menu_->loading_intro_complete())
        return true;
    if (!race_loading_task_ && !race_assets_prepared_) {
        const auto request = menu_->race_request();
        track_name_ = godot::String::utf8(request.track.c_str());
        vehicle_name_ = godot::String::utf8(request.character.c_str());
        race_laps_ = request.laps;
        race_ai_count_ = 5;
        diagnostic_race_seed_ = request.seed;
        menu_player_loadout_ = request.player_loadout;
        competition_requested_ = competition_ = race_mode_ = true;
        race_preview_ = race_completed_ = race_results_pending_ = false;
        inspected_camera_ = -2;
        pressed_keyboard_keys_.clear();
        frontend_input_ = {};
        race_loading_task_ = prepare_race();
    }
    if (race_loading_task_) {
        if (race_loading_task_.advance()) {
            const auto& progress = race_loading_task_.progress();
            if (menu_->set_loading_progress(progress.fraction, progress.message, error) !=
                Result::ok)
                throw std::runtime_error(error);
        } else {
            race_loading_task_.reset();
            race_assets_prepared_ = true;
        }
    }
    update_frontend_audio();
    if (!race_assets_prepared_ || !menu_->loading_fade_complete())
        return true;
    frontend_video_voice_.reset();
    frontend_sfx_.reset();
    frontend_music_voice_.reset();
    frontend_music_.stop();
    frontend_music_path_.clear();
    frontend_audio_screen_.clear();
    menu_.reset();
    entry_ = std::make_unique<GameEntry>(*this, GameEntry::Timing::fixed_simulation);
    if (entry_->start() != Result::ok)
        throw std::runtime_error("Prepared race entry failed");
    renderer_->reset_interpolation();
    std::vector<FrontendRacer> racers;
    race_->update_frontend(session_->frontend_frame(racers));
    last_simulation_usec_ = godot::Time::get_singleton()->get_ticks_usec();
    frontend_loading_presented_ = false;
    set_mouse_capture(window_focused_ && !ui_mouse_visible_);
    present_test_frame();
    return true;
}
void GameHost::return_to_main_menu() {
    if (renderer_)
        renderer_->set_first_person(false);
    camera_view_ = CameraView::near_chase;
    pending_camera_cycles_ = 0;
    previous_cockpit_states_.clear();
    current_cockpit_states_.clear();
    renderer_->set_interpolation_enabled(false);
    set_mouse_capture(false);
    godot::Input::get_singleton()->set_mouse_mode(godot::Input::MOUSE_MODE_VISIBLE);
    frontend_sfx_ = std::make_unique<SfxAudioAdapter>(*this);
    combat_audio_adapter_.reset();
    session_.reset();
    engine_audio_adapter_.reset();
    race_music_adapter_.reset();
    race_music_.stop();
    sfx_audio_adapter_.reset();
    vehicle_.reset();
    renderer_->clear_sky();
    race_.reset();
    menu_ = std::make_unique<MenuFrontend>(*resources_, *renderer_);
    std::string error;
    if (menu_->start(error, "main_menu") != Result::ok)
        throw std::runtime_error(error);
    competition_ = race_mode_ = race_results_pending_ = race_completed_ = false;
    menu_player_loadout_.reset();
    menu_->set_race_seed(static_cast<std::uint32_t>(diagnostic_race_seed_));
    update_frontend_audio();
    entry_->state().mode = GameMode::frontend;
    entry_->set_timing(GameEntry::Timing::frontend_realtime);
    if (debug_overlay_)
        debug_overlay_->ui().clear_race_debug();
}
void GameHost::load_race_debug_tracks() {
    Configuration config;
    float count{};
    if (!resources_ || config.load(*resources_, "Screens/SelectTrack/Data.ini") != Result::ok ||
        config.number("INFO", "NUMSELECTIONS", count) != Result::ok || !std::isfinite(count) ||
        count < 1 || count > 256 || std::floor(count) != count)
        throw std::runtime_error("Original track selection table unavailable");
    std::vector<std::string> tracks;
    for (int index = 0; index < static_cast<int>(count); ++index) {
        const auto section = "SELECTION" + std::to_string(index);
        const auto value = config.value(section, "VALUE");
        float available{};
        if (value.empty() || value.find_first_of("/\\:\r\n") != std::string::npos ||
            config.number(section, "ISAVAILABLE", available) != Result::ok ||
            !std::isfinite(available))
            throw std::runtime_error("Invalid original track selection: " + section);
        if (available == 0)
            continue;
        std::vector<std::byte> bytes;
        if (resources_->read("Tracks/" + value + "/World/Data.txt", bytes) != Result::ok ||
            resources_->read("Tracks/" + value + "/World/SpatialObject.msh", bytes) != Result::ok)
            throw std::runtime_error("Available original track resources missing: " + value);
        tracks.push_back(value);
    }
    if (tracks.empty())
        throw std::runtime_error("Original track table has no available tracks");
    available_race_tracks_ = std::move(tracks);
}
bool GameHost::request_race_debug_track(const godot::String& track) {
    if (!session_ || !developer_build_ || !debug_overlay_ || simulation_failed_)
        return false;
    const auto key = ResourceStore::canonical(track.utf8().get_data());
    const auto found = std::find_if(
        available_race_tracks_.begin(), available_race_tracks_.end(),
        [&](const auto& candidate) { return ResourceStore::canonical(candidate) == key; });
    if (found == available_race_tracks_.end())
        return false;
    if (ResourceStore::canonical(session_->options().track) == key)
        return true;
    debug_overlay_->ui().race_options().requested_track = *found;
    return true;
}
bool GameHost::apply_race_debug_track_request() {
    if (!debug_overlay_)
        return false;
    auto requested = std::exchange(debug_overlay_->ui().race_options().requested_track, {});
    if (requested.empty() || !session_ || !developer_build_)
        return false;
    const auto key = ResourceStore::canonical(requested);
    const auto found = std::find_if(
        available_race_tracks_.begin(), available_race_tracks_.end(),
        [&](const auto& candidate) { return ResourceStore::canonical(candidate) == key; });
    if (found == available_race_tracks_.end())
        throw std::runtime_error("Unknown debug track request");
    if (ResourceStore::canonical(session_->options().track) == key)
        return false;
    const bool was_paused = session_->paused();
    const bool was_ui_visible = ui_mouse_visible_;
    track_name_ = godot::String::utf8(found->c_str());
    inspected_camera_ = -2; // Force the preserved inspection preference onto the new scene.
    pressed_keyboard_keys_.clear();
    frontend_input_ = {};
    restart_smoke_scene();
    if (!session_ || simulation_failed_)
        throw std::runtime_error("Debug track reload failed");
    ++race_track_generation_;
    if (was_paused)
        set_competition_paused(true);
    ui_mouse_visible_ = was_ui_visible;
    debug_overlay_->focus(was_ui_visible && window_focused_);
    set_mouse_capture(!was_paused && !was_ui_visible && window_focused_);
    last_simulation_usec_ = godot::Time::get_singleton()->get_ticks_usec();
    return true;
}
godot::Vector2 GameHost::get_race_debug_track_position(int index) const {
    if (!debug_overlay_)
        return {};
    const auto position = debug_overlay_->ui().race_track_position(index);
    return {position[0], position[1]};
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
        camera_cut_pending_ = true;
        orbit_camera_.reset();
        // Selection is a visual cut even on frames without a simulation tick.
        cut_camera_to_target();
    }
    const auto& options = debug_overlay_->ui().race_options();
    auto snapshot = session_->debug_snapshot(options.show_route || options.show_checkpoints);
    snapshot.available_tracks = available_race_tracks_;
    for (std::size_t index = 0; index < available_race_tracks_.size(); ++index)
        if (ResourceStore::canonical(available_race_tracks_[index]) ==
            ResourceStore::canonical(snapshot.track))
            snapshot.selected_track = static_cast<int>(index);
    snapshot.combat_status += "; diagnostic seed " + std::to_string(session_->options().seed);
    snapshot.combat_status += "; audio: PCM output, QMDX spatial attenuation/Doppler unavailable";
    if (race_ && !race_->combat_render_status().empty())
        snapshot.combat_status += "; " + race_->combat_render_status();
    snapshot.camera = renderer_->race_debug_camera(entry_ ? entry_->interpolation_alpha() : 1);
    debug_overlay_->ui().set_race_debug(std::move(snapshot));
}
void GameHost::show_race_results() {
    if (!session_ || !race_results_pending_)
        return;
    renderer_->set_first_person(false);
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
    combat_audio_adapter_.reset();
    session_.reset();
    engine_audio_adapter_.reset();
    race_music_adapter_.reset();
    race_music_.stop();
    sfx_audio_adapter_.reset();
    vehicle_.reset();
    renderer_->clear_sky();
    race_.reset();
    if (debug_overlay_)
        debug_overlay_->ui().clear_race_debug();
    renderer_->set_interpolation_enabled(false);
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
    menu_->set_race_seed(static_cast<std::uint32_t>(diagnostic_race_seed_));
    menu_player_loadout_.reset();
    update_frontend_audio();
    last_simulation_usec_ = godot::Time::get_singleton()->get_ticks_usec();
}
bool GameHost::competition_weapon_test_step(double delta, int slot, bool nitrous) {
    try {
        if (!session_ || !entry_ || pause_menu_ || session_->paused() ||
            !std::isfinite(delta) || delta < 0 || slot < -1 || slot > 2)
            return false;
        InputState input;
        if (slot >= 0)
            input.weapon_requested[static_cast<std::size_t>(slot)] = true;
        input.nitrous_requested = nitrous;
        competition_test_autopilot_ = false;
        if (entry_->advance(delta, input) != Result::ok)
            return false;
        present_test_frame();
        update_race_debug();
        return true;
    } catch (const std::exception& error) {
        godot::UtilityFunctions::push_error(error.what());
        return false;
    }
}
bool GameHost::competition_test_step(double delta, bool autopilot) {
    try {
        if (!session_ || !entry_ || !std::isfinite(delta) || delta < 0)
            return false;
        if (apply_race_debug_track_request()) {
            update_race_debug();
            present_test_frame();
            return !simulation_failed_;
        }
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
    state["track"] = track_name_;
    state["vehicle"] = vehicle_name_;
    state["diagnostic_seed"] = diagnostic_race_seed_;
    state["menu_music_playing"] = frontend_music_path_ == "Sfx/SoundBackgroundMusic.wav" &&
                                  frontend_music_voice_ && frontend_music_voice_->playing();
    state["frontend_music_path"] = godot::String::utf8(frontend_music_path_.c_str());
    state["frontend_music_playing"] = frontend_music_voice_ && frontend_music_voice_->playing();
    state["loading_progress"] =
        menu_ && menu_->screen() == "loading" ? menu_->loading_progress() : -1;
    state["loading_intro_complete"] = menu_ && menu_->loading_intro_complete();
    state["menu_music_position"] =
        frontend_music_voice_ ? frontend_music_voice_->playback_position() : 0;
    state["menu_music_gain"] = frontend_music_voice_ ? frontend_music_voice_->linear_gain() : 0;
    state["menu_music_frames"] = frontend_music_.sample().frame_count();
    const auto* frontend = pause_menu_ ? pause_menu_.get() : menu_.get();
    state["frontend_assertion"] =
        frontend ? godot::String::utf8(std::string(frontend->assertion_message()).c_str())
                 : godot::String();
    state["track_generation"] = static_cast<int64_t>(race_track_generation_);
    godot::Array tracks;
    for (const auto& track : available_race_tracks_)
        tracks.push_back(godot::String::utf8(track.c_str()));
    state["available_tracks"] = tracks;
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
    godot::Array ammunition_notices, weapon_ammunition;
    if (session_) {
        for (const auto& notice : session_->ammunition_notices()) {
            godot::Dictionary entry;
            entry["text"] = godot::String::utf8(notice.text.c_str());
            entry["remaining"] = notice.remaining;
            ammunition_notices.push_back(entry);
        }
        const auto& player = *session_->racers()[session_->player_index()].vehicle;
        for (std::size_t slot = 0; slot < 3; ++slot)
            weapon_ammunition.push_back(player.combat()->slot(static_cast<WeaponSlot>(slot)).ammunition);
        state["nitrous_ammunition"] = player.equipment().nitrous_ammo;
    }
    state["ammunition_notices"] = ammunition_notices;
    state["weapon_ammunition"] = weapon_ammunition;
    state["camera_view"] = static_cast<int>(camera_view_);
    state["cockpit_target"] = static_cast<int>(camera_target_index());
    state["cockpit_alpha"] = entry_ ? entry_->interpolation_alpha() : 0;
    state["cockpit_rpm"] = presented_cockpit_rpm_;
    state["cockpit_steering_fraction"] = presented_cockpit_steering_;
    const auto cockpit_target = camera_target_index();
    if (cockpit_target < current_cockpit_states_.size() && cockpit_target < previous_cockpit_states_.size()) {
        state["cockpit_previous_rpm"] = previous_cockpit_states_[cockpit_target].rpm;
        state["cockpit_current_rpm"] = current_cockpit_states_[cockpit_target].rpm;
    }
    state["camera_target"] = debug_overlay_ ? debug_overlay_->ui().race_options().selected_ai : -1;
    state["pause_selection"] = pause_menu_ ? pause_menu_->selected() : -1;
    if (session_) {
        state["countdown_remaining"] = session_->start_sequence().displayed_remaining();
        state["countdown_cues"] = session_->start_sequence().cue_count();
        state["countdown_digit"] =
            RaceStart::digit(session_->start_sequence().displayed_remaining());
    }
    godot::Array loadouts;
    if (session_)
        for (const auto& racer : session_->racers()) {
            godot::Dictionary item;
            item["character"] = godot::String::utf8(racer.character.c_str());
            item["paint"] = racer.loadout.paint_job;
            godot::Array weapons;
            for (const auto& name : racer.loadout.weapons)
                weapons.push_back(godot::String::utf8(name.c_str()));
            item["weapons"] = weapons;
            loadouts.push_back(item);
        }
    state["loadouts"] = loadouts;
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
        state["idle_return_remaining"] = session_->idle_return_remaining();
        state["eliminated"] = session_->ending() == RaceEndReason::eliminated;
    }
    if (menu_ && menu_->screen() == "loading")
        state["phase"] = "loading";
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
