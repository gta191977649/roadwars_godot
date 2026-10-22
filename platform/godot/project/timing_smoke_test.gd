extends SceneTree

func _initialize() -> void:
    call_deferred("run_test")

const Presentation = preload("res://presentation_assertions.gd")

func run_test() -> void:
    var reference: Array = []
    for fps in [60,10,15,30,59,75,119,120,144,165,240,0]:
        var host = ClassDB.instantiate("GameHost")
        root.add_child(host)
        host.set_process(false)
        host.present_test_frame()
        if fps == 60 and not host.test_interpolation_history():
            push_error("Native high-speed/settling/mirrored-wheel/reset regression failed")
            quit(1)
            return
        for phase in range(3):
            var seconds = 3 if phase==0 else 1
            var remaining: float = seconds
            var frame = 0
            var irregular = [1.0/240, 1.0/59, 0.35, 1.0/119, 0.002, 0.9]
            while remaining > 0.0000000001:
                var dt: float = minf(remaining, 1.0/fps if fps > 0 else irregular[frame % irregular.size()])
                remaining -= dt
                frame += 1
                var before: Dictionary = host.get_presentation_state()
                if not host.drive_test_step(dt,1 if phase==1 else 0,1 if phase<2 else 0,1 if phase==2 else 0,false):
                    push_error("Fixed-clock host step failed")
                    quit(1)
                    return
                if not Presentation.verify_presentation(host, before):
                    quit(1)
                    return
                if host.get_simulation_alpha()<0 or host.get_simulation_alpha()>1:
                    push_error("Invalid render interpolation fraction")
                    quit(1)
                    return
        var state = Presentation.snapshot(host)
        if host.get_simulation_ticks()!=300:
            push_error("Render frame count changed simulation tick count")
            quit(1)
            return
        if reference.is_empty():
            reference=state
        elif state!=reference:
            push_error("Host physics/effect state changed at %d FPS: %s vs %s" % [fps,state,reference])
            quit(1)
            return
        await process_frame
        if DisplayServer.get_name()!="headless":
            await RenderingServer.frame_post_draw
        root.remove_child(host)
        if host.get_live_rids()!=0:
            push_error("Interpolated scene retained RIDs")
            quit(1)
            return
        host.free()
        print("Fixed host at ", str(fps) + " FPS" if fps > 0 else "irregular intervals + 350/900ms stalls", ": identical 300-tick vehicle/effect state")
    # Exercise actual Godot callbacks. Godot can clamp process(delta) after stalls;
    # the host must recover wall time from its monotonic clock instead.
    var real_host = ClassDB.instantiate("GameHost")
    root.add_child(real_host)
    for frame in range(3):
        await process_frame
    var before_stall: int = real_host.get_simulation_ticks()
    OS.delay_msec(350)
    await process_frame
    await process_frame
    var recovered: int = real_host.get_simulation_ticks()-before_stall
    if recovered<20:
        push_error("Actual Godot stall discarded elapsed simulation time: %d ticks" % recovered)
        quit(1)
        return
    Engine.time_scale=0
    await process_frame
    await process_frame
    var paused_ticks: int = real_host.get_simulation_ticks()
    OS.delay_msec(100)
    await process_frame
    await process_frame
    if real_host.get_simulation_ticks()!=paused_ticks:
        push_error("Fixed simulation ignored Godot time scale pause")
        Engine.time_scale=1
        quit(1)
        return
    Engine.time_scale=1
    root.remove_child(real_host)
    real_host.free()
    # The audio mixer releases playback/stream references on its own thread.
    # Uncapped headless process frames can both finish before its next mix block.
    await create_timer(0.1).timeout
    print("ROADWARS_TIMING_SMOKE_OK: independent fixed simulation; interpolated native presentation; recovered stall ticks=",recovered,"; engine pause; lifecycle")
    quit(0)
