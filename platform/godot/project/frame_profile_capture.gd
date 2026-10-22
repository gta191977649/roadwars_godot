extends SceneTree

func _initialize() -> void:
    call_deferred("run_capture")

func run_capture() -> void:
    root.size = Vector2i(1280, 720)
    var duration = 25.0
    var label = "baseline"
    for argument in OS.get_cmdline_user_args():
        if argument.begins_with("--profile-seconds="):
            duration = argument.trim_prefix("--profile-seconds=").to_float()
        if argument.begins_with("--profile-label="):
            label = argument.trim_prefix("--profile-label=")
    var host = ClassDB.instantiate("GameHost")
    root.add_child(host)
    host.competition_test_step(0, true)
    var start = Time.get_ticks_usec()
    while Time.get_ticks_usec() - start < duration * 1000000:
        await process_frame
    host.set_process(false)
    var data = host.get_frame_profile_trace()
    var timing = host.get_frame_timing_state()
    var file = FileAccess.open("res://../../../build/frame_profile_" + label + ".json", FileAccess.WRITE)
    file.store_string(JSON.stringify({"frames": data, "timing": timing}, "  "))
    file.close()
    timing.erase("samples_ms")
    print("FRAME_PROFILE_CAPTURE ", label, " frames=", data.size(), " ", JSON.stringify(timing))
    root.remove_child(host)
    host.free()
    await process_frame
    await process_frame
    print("ROADWARS_FRAME_PROFILE_OK")
    quit(0)
