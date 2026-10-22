extends SceneTree

func _initialize() -> void:
    call_deferred("run_test")

func run_test() -> void:
    root.size = Vector2i(1280, 720)
    var host = ClassDB.instantiate("GameHost")
    root.add_child(host)
    host.competition_test_step(0, true)
    var start = Time.get_ticks_usec()
    while Time.get_ticks_usec() - start < 20000000:
        await process_frame
    var state: Dictionary = host.get_frame_timing_state()
    if state.get("sample_count", 0) != 600 or state.get("refresh_hz", 0) <= 0:
        push_error("Live frame timing did not collect a complete display-backed history")
        quit(1)
        return
    var data = FileAccess.open("res://../../../build/frame_timing_capture.json", FileAccess.WRITE)
    data.store_string(JSON.stringify(state, "  "))
    data.close()
    var csv = FileAccess.open("res://../../../build/frame_timing_capture.csv", FileAccess.WRITE)
    csv.store_csv_line(PackedStringArray(["sample", "interval_ms", "target_ms"]))
    for i in range(state.samples_ms.size()):
        csv.store_csv_line(PackedStringArray([str(i), str(state.samples_ms[i]), str(state.target_ms)]))
    csv.close()
    var summary = state.duplicate()
    summary.erase("samples_ms")
    print("FRAME_TIMING_CAPTURE ", JSON.stringify(summary))
    await RenderingServer.frame_post_draw
    root.get_texture().get_image().save_png("res://../../../build/frame_timing_graph.png")
    # Duplicated overlay draws in one engine frame must not add bogus samples.
    host.competition_test_step(0, true)
    host.competition_test_step(0, true)
    var once: Dictionary = host.get_frame_timing_state()
    host.competition_test_step(0, true)
    if once != host.get_frame_timing_state():
        push_error("Manual overlay draws generated artificial frame intervals")
        quit(1)
        return
    OS.delay_msec(100)
    for frame in range(3):
        await process_frame
    if host.get_frame_timing_state().max_ms < 100:
        push_error("Wall-clock frame history hid a real 100ms stall")
        quit(1)
        return
    root.remove_child(host)
    host.free()
    await process_frame
    await process_frame
    print("ROADWARS_FRAME_TIMING_OK")
    quit(0)
