#pragma once
#include "game_src/core/runtime.hpp"
#include "game_src/unresolved/collision_audio.hpp"
#include "game_src/unresolved/engine_audio.hpp"
#include "game_src/unresolved/race_music.hpp"
#include "game_src/unresolved/game_entry.hpp"
#include "game_src/unresolved/game_initialization.hpp"
#include "game_src/unresolved/race_presentation.hpp"
#include "game_src/unresolved/tire_audio.hpp"
#include "game_src/unresolved/vehicle_dynamics.hpp"
#include "platform/godot/extension/debug_overlay.hpp"
#include "platform/godot/extension/engine_audio_adapter.hpp"
#include "platform/godot/extension/game_files.hpp"
#include "platform/godot/extension/sfx_audio_adapter.hpp"
#include "platform/godot/rendering/server_renderer.hpp"
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/node.hpp>
#include <memory>

namespace roadwars {
class GameHost : public godot::Node, public LoopHooks {
    GDCLASS(GameHost, godot::Node)
  protected:
    static void _bind_methods();
    void _notification(int what);

  public:
    void _ready() override;
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
    void start_fog_fixture();
    void start_billboard_fixture();
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
    int get_particle_count() const;
    int get_spark_count() const;
    int get_tire_particle_count() const;
    int get_skid_vertex_count() const;
    void set_skid_marks_visible(bool);
    void set_particles_visible(bool visible);
    Result invoke(LoopStage stage, float delta_seconds, const InputState& input,
                  LoopState& state) override;

  private:
    std::unique_ptr<DebugOverlayAdapter> debug_overlay_;
    bool ui_mouse_visible_{}, simulation_failed_{};
    std::uint64_t last_simulation_usec_{};
    std::unique_ptr<ServerRenderer> renderer_;
    std::unique_ptr<Runtime> runtime_;
    std::unique_ptr<GameEntry> entry_;
    bool original_loop_{}, developer_build_{};
    bool race_mode_{}, race_preview_{};
    godot::String game_directory_, track_name_, vehicle_name_{"Animal"};
    std::unique_ptr<GameFiles> files_;
    std::unique_ptr<ResourceStore> resources_;
    std::unique_ptr<RacePresentation> race_;
    std::unique_ptr<VehicleDynamics> vehicle_;
    void set_mouse_capture(bool enabled);
    bool window_focused_{true}, owns_mouse_capture_{}, ignore_next_mouse_motion_{};
    int previous_mouse_mode_{};
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
