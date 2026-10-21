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
    if not host.is_race_music_playing() or not host.has_node("OriginalRaceMusic"):
        fail("Default race did not start its original BGM")
        return
    if abs(host.get_race_music_duration() - 1857152.0 / 22050.0) > 0.00001:
        fail("Decoded original BGM duration differs")
        return
    if abs(host.get_race_music_gain() - 19660.0 / 32767.0) > 0.00001:
        fail("Original INGAMEMUSIC volume was not applied")
        return
    var peak = -200.0
    var deadline = Time.get_ticks_msec() + 1500
    while Time.get_ticks_msec() < deadline:
        await process_frame
        peak = maxf(peak, AudioServer.get_bus_peak_volume_left_db(0, 0))
    if host.get_race_music_position() < 0.1 or peak < -70:
        fail("Original BGM did not advance through the native mixer")
        return
    host.seek_race_music_for_test(host.get_race_music_duration() - 0.15)
    deadline = Time.get_ticks_msec() + 700
    while Time.get_ticks_msec() < deadline:
        await process_frame
    if not host.is_race_music_playing() or host.get_race_music_position() > 2:
        fail("Original BGM did not wrap at its actual loop boundary")
        return
    var before = host.get_race_music_position()
    if not host.drive_test_step(1.0 / 30, 0, 0, 0, true):
        fail("Driving reset failed")
        return
    if host.get_race_music_position() + 0.05 < before:
        fail("Vehicle reset restarted the BGM")
        return
    for cycle in range(3):
        host.restart_smoke_scene()
        host.set_process(false)
        if not host.is_race_music_playing() or host.get_node("OriginalRaceMusic").get_name() != "OriginalRaceMusic":
            fail("Scene restart failed to recreate exactly one BGM voice")
            return
        await process_frame
    host.start_fog_fixture()
    if host.is_race_music_playing() or host.has_node("OriginalRaceMusic"):
        fail("Leaving race retained the BGM voice")
        return
    root.remove_child(host)
    host.free()
    await process_frame
    print("ROADWARS_MUSIC_SMOKE_OK: original BGM; peak_db=", peak, "; volume; real loop boundary; reset; 3 restarts; teardown")
    quit(0)
