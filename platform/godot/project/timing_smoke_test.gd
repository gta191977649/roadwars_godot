extends SceneTree

func _initialize() -> void:
    call_deferred("run_test")

func snapshot(host) -> Array:
    return [host.get_vehicle_position(),host.get_vehicle_heading(),host.get_vehicle_speed(),host.get_crash_sound_count(),host.get_tire_sound_start_count(),host.get_particle_count(),host.get_skid_vertex_count(),host.get_simulation_ticks()]

func verify_presentation(host, before: Dictionary) -> bool:
    var pending: Dictionary = host.get_presentation_state()
    # Simulation may change snapshots, but only presentation may submit transforms.
    for id in before.instances:
        if pending.instances.has(id) and pending.instances[id].presented != before.instances[id].presented:
            push_error("Simulation submitted a raw instance transform before presentation")
            return false
    if pending.camera.presented != before.camera.presented:
        push_error("Simulation submitted a raw camera transform before presentation")
        return false
    var physics = snapshot(host)
    host.present_test_frame()
    var presented: Dictionary = host.get_presentation_state()
    var alpha: float = host.get_simulation_alpha()
    for id in pending.instances:
        var pose: Dictionary = presented.instances[id]
        var expected: Transform3D = pose.previous.interpolate_with(pose.current, alpha)
        if not pose.presented.is_equal_approx(expected):
            push_error("Instance presentation differs from its simulation interval")
            return false
    var camera: Dictionary = presented.camera
    if not camera.presented.is_equal_approx(camera.previous.interpolate_with(camera.current, alpha)):
        push_error("Camera presentation differs from the shared simulation interval")
        return false
    host.present_test_frame()
    if physics != snapshot(host) or presented != host.get_presentation_state():
        push_error("Presentation mutated physics or its own simulation snapshots")
        return false
    return true

func run_test() -> void:
    var reference: Array = []
    for fps in [60,10,15,30,59,75,119,120,144,165,240]:
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
            for frame in range(fps*seconds):
                var before: Dictionary = host.get_presentation_state()
                if not host.drive_test_step(1.0/fps,1 if phase==1 else 0,1 if phase<2 else 0,1 if phase==2 else 0,false):
                    push_error("Fixed-clock host step failed")
                    quit(1)
                    return
                if not verify_presentation(host, before):
                    quit(1)
                    return
                if host.get_simulation_alpha()<0 or host.get_simulation_alpha()>1:
                    push_error("Invalid render interpolation fraction")
                    quit(1)
                    return
        var state = snapshot(host)
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
        print("Fixed host at ",fps," render FPS: identical 300-tick vehicle/effect state")
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
    await process_frame
    await process_frame
    print("ROADWARS_TIMING_SMOKE_OK: independent fixed simulation; interpolated native presentation; recovered stall ticks=",recovered,"; engine pause; lifecycle")
    quit(0)
