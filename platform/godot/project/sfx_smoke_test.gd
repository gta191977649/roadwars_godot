extends SceneTree

func _initialize() -> void:
    call_deferred("run_test")

func fail(message: String) -> void:
    push_error(message)
    quit(1)

func run_test() -> void:
    var host = ClassDB.instantiate("GameHost")
    root.add_child(host)
    host.set_process(false)
    # Real physics drives the portable skid/contact gates before any diagnostic injection.
    var tire_seen := false
    # Sixty acceleration frames keep the first wheel on the original 0.9-grip road.
    # Space is the recovered full brake; S is a direction-sensitive keyboard ramp.
    for step in range(180):
        var throttle := 1.0 if step < 60 else 0.0
        var full_brake := step >= 60 and step < 120
        if not host.drive_test_step(1.0 / 60.0, 0, throttle, 0, false, full_brake):
            fail("Actual SFX driving fixture failed")
            return
        tire_seen = tire_seen or host.get_tire_audio_playing()
    var actual_tire_starts: int = host.get_tire_sound_start_count()
    var actual_crashes: int = host.get_crash_sound_count()
    if not tire_seen or actual_tire_starts == 0:
        fail("Actual driving did not start the original skid loop")
        return
    if actual_crashes == 0:
        host.drive_test_step(1.0 / 60.0, 0, 0, 0, true)
        for step in range(1200):
            if not host.drive_test_step(1.0 / 60.0, 0, 1, 0, false):
                fail("Actual wall-contact audio fixture failed")
                return
        actual_crashes = host.get_crash_sound_count()
    if actual_crashes == 0:
        fail("Actual vehicle wall contacts produced no crash voice")
        return
    # Isolate decoded original PCM from the engine to verify each native SFX voice.
    # drive_test_step advances simulation only; present the current camera/listener
    # before measuring distance-dependent native output at the vehicle's position.
    host.present_test_frame()
    host.begin_sfx_fixture()
    if host.is_engine_audio_playing():
        fail("SFX PCM fixture retained the engine voice")
        return
    host.sfx_fixture_tire(true)
    var tire_peak := -200.0
    for frame in range(40):
        # Godot 3D voice panning/start work runs on native physics frames.
        await physics_frame
        tire_peak = maxf(tire_peak, AudioServer.get_bus_peak_volume_left_db(0, 0))
    if not host.get_tire_audio_playing() or host.get_tire_audio_playback_position() <= 0 or tire_peak <= -70:
        fail("Original skid PCM loop did not reach the native mixer")
        return
    host.sfx_fixture_tire(false)
    if host.get_tire_audio_playing():
        fail("Skid exit did not stop its loop")
        return
    # Allow the previous audio block to drain before measuring collision signal.
    await create_timer(0.15).timeout
    host.sfx_fixture_collision(false, 1)
    host.sfx_fixture_collision(true, 4)
    if host.get_active_collision_voices() != 5:
        fail("Collision one-shots replaced simultaneous landing voices")
        return
    var collision_peak := -200.0
    for frame in range(30):
        await physics_frame
        collision_peak = maxf(collision_peak, AudioServer.get_bus_peak_volume_left_db(0, 0))
    if collision_peak <= -70:
        fail("Original crash/landing PCM did not reach the native mixer")
        return
    await create_timer(2.0).timeout
    if host.get_active_collision_voices() != 0:
        fail("Collision one-shots looped after completion")
        return
    host.sfx_fixture_tire(true)
    host.sfx_fixture_collision(true, 4)
    host.drive_test_step(1.0 / 60.0, 0, 0, 0, true)
    if host.get_tire_audio_playing() or host.get_active_collision_voices() != 0:
        fail("Vehicle reset retained native sound effects")
        return
    host.sfx_fixture_tire(true)
    host.sfx_fixture_collision(false, 1)
    host.start_fog_fixture()
    if host.has_node("OriginalTireSkidVoice") or host.get_active_collision_voices() != 0:
        fail("Diagnostic fixture retained SFX voices")
        return
    root.remove_child(host)
    host.free()
    await create_timer(0.2).timeout
    print("ROADWARS_SFX_SMOKE_OK: actual_tire_starts=", actual_tire_starts, "; actual_crashes=", actual_crashes, "; tire_peak=", tire_peak, "; collision_peak=", collision_peak, "; loop stop; overlapping one-shots; completion; reset; teardown")
    quit(0)
