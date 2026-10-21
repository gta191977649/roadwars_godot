extends SceneTree

func _initialize() -> void:
    call_deferred("run_test")

func run_test() -> void:
    var host = ClassDB.instantiate("GameHost")
    root.add_child(host)
    host.set_process(false)
    var found = false
    for step in range(1200):
        var steering = 1.0 if step >= 240 and step < 360 else 0.0
        if not host.drive_test_step(1.0 / 60.0, steering, 1, 0, false):
            quit(1)
            return
        if host.get_spark_count() > 0:
            found = true
            break
    if not found:
        push_error("Original wall collision did not trigger particles")
        quit(1)
        return
    for frame in range(4):
        await process_frame
    if DisplayServer.get_name() != "headless":
        await RenderingServer.frame_post_draw
        var visible = root.get_texture().get_image()
        for argument in OS.get_cmdline_user_args():
            if argument.begins_with("--capture="):
                visible.save_png(argument.trim_prefix("--capture="))
        host.set_particles_visible(false)
        for frame in range(3):
            await process_frame
        await RenderingServer.frame_post_draw
        var hidden = root.get_texture().get_image()
        var changed_pixels = 0
        for y in range(visible.get_height()):
            for x in range(visible.get_width()):
                var a = visible.get_pixel(x,y)
                var b = hidden.get_pixel(x,y)
                if absf(a.r-b.r)+absf(a.g-b.g)+absf(a.b-b.b) > 0.02:
                    changed_pixels += 1
        if changed_pixels < 2:
            push_error("Native Godot particle instances produced no visible pixels")
            quit(1)
            return
        print("Native particle pixels: ", changed_pixels)
    print("ROADWARS_PARTICLE_SMOKE_OK: sparks=", host.get_spark_count(), "; tire smoke=", host.get_tire_particle_count(), "; native particle RIDs released on exit")
    var expected_count = host.get_live_rids()
    for cycle in range(3):
        host.restart_smoke_scene()
        await process_frame
        if host.get_live_rids() != expected_count:
            push_error("Native particle RIDs changed across scene restart")
            quit(1)
            return
    root.remove_child(host)
    if host.get_live_rids() != 0:
        quit(1)
        return
    host.free()
    quit(0)
