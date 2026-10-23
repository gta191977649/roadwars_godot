#pragma once
#include "game_src/core/runtime.hpp"
#include "game_src/unresolved/collision_audio.hpp"
#include "game_src/unresolved/engine_audio.hpp"
#include "game_src/unresolved/game_entry.hpp"
#include "game_src/unresolved/game_initialization.hpp"
#include "game_src/unresolved/menu_frontend.hpp"
#include "game_src/unresolved/race_ai_combat.hpp"
#include "game_src/unresolved/race_music.hpp"
#include "game_src/unresolved/race_presentation.hpp"
#include "game_src/unresolved/race_session.hpp"
#include "game_src/unresolved/tire_audio.hpp"
#include "game_src/unresolved/vehicle_dynamics.hpp"
#include "platform/godot/extension/combat_audio_adapter.hpp"
#include "platform/godot/extension/debug_overlay.hpp"
#include "platform/godot/extension/engine_audio_adapter.hpp"
#include "platform/godot/extension/frame_trace.hpp"
#include "platform/godot/extension/game_files.hpp"
#include "platform/godot/extension/loading_task.hpp"
#include "platform/godot/extension/original_weapon_input.hpp"
#include "platform/godot/extension/sfx_audio_adapter.hpp"
#include "platform/godot/rendering/server_renderer.hpp"
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/accept_dialog.hpp>
#include <godot_cpp/classes/file_dialog.hpp>
#include <memory>
#include <set>

namespace roadwars {
class GameHost : public godot::Node, public LoopHooks {
    GDCLASS(GameHost, godot::Node)
  protected:
    static void _bind_methods();
    void _notification(int what);

  private:
    OriginalWeaponKeys weapon_keys_;
    bool startup_waiting_{};
    double title_elapsed_{1.0};
    void update_window_title(double delta);
    godot::String startup_error_;
    godot::AcceptDialog* startup_prompt_{};
    godot::FileDialog* startup_browser_{};
    bool resolve_startup_directory();
    void show_startup_prompt(godot::String error);
    void finish_startup();

  public:
    void _ready() override;
    void browse_game_directory();
    void select_game_directory(godot::String path);
    void cancel_game_directory();
    void quit_startup();
    godot::Dictionary get_startup_state() const;
    godot::Dictionary validate_game_directory_for_test(godot::String path) const;
    bool is_race_music_playing() const;
    double get_race_music_position() const;
    double get_race_music_gain() const;
    double get_race_music_duration() const;
    void seek_race_music_for_test(double seconds);
    void set_frontend_visible(bool);
    godot::Vector2 get_frontend_player_position() const;
    bool get_console_collapsed() const;
    godot::Vector2 get_console_input_position() const;
    bool console_contains(godot::String text) const;
    double get_debug_dpi_scale() const;
    godot::Dictionary get_frame_timing_state() const;
    godot::Array get_frame_profile_trace() const;
    int get_debug_live_rids() const;
    int64_t get_render_triangles() const;
    bool get_ui_mouse_visible() const;
    void debug_stdio_test();
    void _process(double delta) override;
    void _input(const godot::Ref<godot::InputEvent>& event) override;
    void orbit_test_input(double relative_x, double relative_y);
    godot::Vector3 get_camera_position() const;
    double get_camera_orbit_yaw() const;
    double get_camera_heading() const;
    int get_camera_orbit_state() const;
    double get_camera_far_distance() const;
    double get_view_distance_at(godot::Vector3 position);
    void _exit_tree() override;
    void restart_smoke_scene();
    godot::String get_frontend_screen() const;
    godot::String get_frontend_action() const;
    bool preview_frontend_screen(godot::String id, double seconds, godot::String variant);
    bool frontend_test_step(double elapsed, godot::String command);
    void start_fog_fixture();
    void start_billboard_fixture();
    godot::Dictionary start_chrome_fixture();
    void set_billboard_fixture_camera(double heading, double pitch);
    int get_live_rids() const;
    bool is_engine_audio_playing() const;
    bool get_tire_audio_playing() const;
    int get_collision_sound_count() const;
    int get_crash_sound_count() const;
    int get_active_collision_voices() const;
    int get_tire_sound_start_count() const;
    double get_tire_audio_playback_position() const;
    void begin_sfx_fixture();
    void sfx_fixture_tire(bool active);
    void sfx_fixture_collision(bool land, int count);
    double get_engine_audio_frequency() const;
    double get_engine_audio_gain() const;
    double get_engine_audio_playback_position() const;
    double get_engine_audio_sample_frames() const;
    bool drive_test_step(double delta, double steering, double throttle, double brake, bool reset,
                         bool handbrake = false);
    godot::Vector3 get_vehicle_position() const;
    double get_vehicle_heading() const;
    double get_vehicle_speed() const;
    int64_t get_simulation_ticks() const;
    double get_simulation_alpha() const;
    void present_test_frame();
    void cycle_camera_view();
    void set_camera_view_for_test(int view);
    godot::Dictionary get_presentation_state() const;
    bool test_interpolation_history() const;
    int get_particle_count() const;
    int get_spark_count() const;
    int get_tire_particle_count() const;
    int get_skid_vertex_count() const;
    void set_skid_marks_visible(bool);
    void set_particles_visible(bool visible);
    bool competition_test_step(double delta, bool autopilot);
    bool competition_weapon_test_step(double delta, int slot, bool nitrous);
    godot::Dictionary get_competition_state() const;
    godot::Vector2 get_race_debug_checkbox_position(int index) const;
    godot::Vector2 get_race_debug_inspect_position(int index) const;
    int get_race_debug_line_count() const;
    bool request_race_debug_track(const godot::String& track);
    godot::Vector2 get_race_debug_track_position(int index) const;
    Result invoke(LoopStage stage, float delta_seconds, const InputState& input,
                  LoopState& state) override;

  private:
    std::unique_ptr<DebugOverlayAdapter> debug_overlay_;
    bool ui_mouse_visible_{}, simulation_failed_{};
    std::optional<bool> chrome_mapping_override_;
    FrameTrace frame_trace_;
    std::uint64_t last_simulation_usec_{};
    std::unique_ptr<ServerRenderer> renderer_;
    std::unique_ptr<Runtime> runtime_;
    std::unique_ptr<GameEntry> entry_;
    std::unique_ptr<MenuFrontend> menu_, pause_menu_;
    void set_competition_paused(bool paused);
    bool update_pause_menu(double delta);
    void return_to_main_menu();
    bool previous_tree_paused_{};
    int inspected_camera_{-1};
    std::unique_ptr<EngineAudioAdapter> frontend_video_voice_;
    RaceMusic frontend_music_;
    std::string frontend_music_path_;
    std::unique_ptr<EngineAudioAdapter> frontend_music_voice_;
    bool apply_frontend_race_request(double elapsed);
    LoadingTask prepare_race();
    LoadingTask race_loading_task_;
    bool race_assets_prepared_{};
    std::uint64_t frontend_loading_frame_{};
    bool frontend_loading_presented_{};
    std::optional<RaceLoadout> menu_player_loadout_;
    std::unique_ptr<SfxAudioAdapter> frontend_sfx_;
    std::string frontend_audio_screen_;
    void update_frontend_audio();
    FrontendInput frontend_input_;
    bool diagnostic_{};
    godot::String frontend_preview_, frontend_variant_;
    double frontend_preview_time_{};
    bool original_loop_{}, developer_build_{};
    bool race_mode_{}, race_preview_{};
    bool competition_test_autopilot_{};
    bool competition_{}, competition_requested_{}, race_results_pending_{}, race_completed_{};
    int race_laps_{3}, race_ai_count_{5};
    std::int64_t diagnostic_race_seed_{1};
    void update_race_debug();
    void load_race_debug_tracks();
    bool apply_race_debug_track_request();
    std::vector<std::string> available_race_tracks_;
    std::uint64_t race_track_generation_{};
    void show_race_results();
    godot::String game_directory_, track_name_, vehicle_name_{"Animal"};
    std::unique_ptr<GameFiles> files_;
    std::unique_ptr<ResourceStore> resources_;
    std::unique_ptr<RacePresentation> race_;
    std::unique_ptr<VehicleDynamics> vehicle_;
    // Borrows vehicle_ and race_->collision(); must be destroyed before both.
    std::unique_ptr<RaceSession> session_;
    std::unique_ptr<CombatAudioAdapter> combat_audio_adapter_;
    void set_mouse_capture(bool enabled);
    bool window_focused_{true}, owns_mouse_capture_{}, ignore_next_mouse_motion_{};
    OriginalWeaponModifiers weapon_modifiers_;
    std::set<std::uint64_t> pressed_keyboard_keys_;
    int previous_mouse_mode_{};
    CameraView camera_view_{CameraView::near_chase};
    unsigned pending_camera_cycles_{};
    std::vector<VehicleState> previous_cockpit_states_, current_cockpit_states_;
    bool camera_cut_pending_{};
    float presented_cockpit_rpm_{}, presented_cockpit_steering_{};
    std::size_t camera_target_index() const;
    void cut_camera_to_target();
    void capture_cockpit_states(bool previous);
    void present_race_frontend(double alpha);
    OriginalChaseCamera chase_camera_;
    MouseOrbitCamera orbit_camera_;
    MouseOrbitInput pending_mouse_;
    void submit_tire_audio(const TireAudioInput& input);
    void submit_collision_audio(const CollisionAudioEvent& event);
    bool sfx_fixture_{};
    TireAudio tire_audio_;
    TireAudioResources tire_audio_resources_;
    CollisionAudioResources collision_audio_resources_;
    std::unique_ptr<SfxAudioAdapter> sfx_audio_adapter_;
    std::string tire_sample_path_;
    int tire_sound_starts_{};
    int crash_sound_starts_{};
    EngineAudio engine_audio_;
    RaceMusic race_music_;
    std::unique_ptr<EngineAudioAdapter> race_music_adapter_;
    EngineAudioResources engine_audio_resources_;
    std::unique_ptr<EngineAudioAdapter> engine_audio_adapter_;
    InitializedGame initialized_;
    ParticleEmitter sparks_;
    ParticleRandom particle_random_;
    std::array<TireParticleEmitter, 4> tire_smoke_;
    std::array<SkidMarkEmitter, 4> skid_marks_;
};
} // namespace roadwars
