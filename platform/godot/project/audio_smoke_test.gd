extends SceneTree

func _initialize() -> void:
    call_deferred("run_test")

func run_test() -> void:
    var host = ClassDB.instantiate("GameHost")
    root.add_child(host)
    # Keep this existing mixer test specific to the engine now that races have BGM.
    host.get_node("OriginalRaceMusic").volume_linear = 0
    host.set_process(false)
    if host.get_engine_audio_sample_frames() != 68173:
        push_error("Original Animal engine sample was not loaded")
        quit(1)
        return
    if not host.drive_test_step(1.0 / 60.0, 0, 0, 0, false):
        quit(1)
        return
    var idle_frequency: float = host.get_engine_audio_frequency()
    var maximum_frequency = idle_frequency
    var peak_db = -200.0
    for frame in range(120):
        if not host.drive_test_step(1.0 / 60.0, 0, 1, 0, false):
            quit(1)
            return
        maximum_frequency = maxf(maximum_frequency, host.get_engine_audio_frequency())
        await process_frame
        peak_db = maxf(peak_db, AudioServer.get_bus_peak_volume_left_db(0, 0))
    if not host.is_engine_audio_playing() or host.get_engine_audio_playback_position() <= 0:
        push_error("Native engine voice did not advance")
        quit(1)
        return
    if maximum_frequency <= idle_frequency or host.get_engine_audio_gain() <= 0:
        push_error("Recovered engine pitch or volume did not update")
        quit(1)
        return
    if peak_db <= -70.0:
        push_error("Native audio mixer produced no measurable engine signal")
        quit(1)
        return
    host.start_fog_fixture()
    if host.is_engine_audio_playing() or host.has_node("OriginalEngineVoice"):
        push_error("Diagnostic fixture retained the engine voice")
        quit(1)
        return
    root.remove_child(host)
    host.free()
    await process_frame
    print("ROADWARS_AUDIO_SMOKE_OK: idle_hz=", idle_frequency, "; maximum_hz=", maximum_frequency, "; peak_db=", peak_db, "; native original PCM; voice released")
    quit(0)
