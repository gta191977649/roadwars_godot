extends SceneTree

func _initialize() -> void:
    call_deferred("run_test")

func run_test() -> void:
    var host = ClassDB.instantiate("GameHost")
    root.add_child(host)
    host.set_process(false)
    var spawn: Vector3 = host.get_vehicle_position()
    var heading: float = host.get_vehicle_heading()
    for step in range(180):
        if not host.drive_test_step(1.0 / 60.0, 0, 1, 0, false):
            quit(1)
            return
    var driven: Vector3 = host.get_vehicle_position()
    var speed: float = host.get_vehicle_speed()
    if not driven.is_finite() or driven.distance_to(spawn) < 2 or speed <= 0:
        push_error("Vehicle did not drive forward")
        quit(1)
        return
    for step in range(30):
        if not host.drive_test_step(1.0 / 60.0, 1, 1, 0, false):
            quit(1)
            return
    if absf(host.get_vehicle_heading() - heading) < 1:
        push_error("Steering did not change heading")
        quit(1)
        return
    var before_brake: float = host.get_vehicle_speed()
    for step in range(60):
        if not host.drive_test_step(1.0 / 60.0, 0, 0, 1, false):
            quit(1)
            return
    if host.get_vehicle_speed() >= before_brake:
        push_error("Brake did not reduce speed")
        quit(1)
        return
    if host.get_skid_vertex_count() < 4:
        push_error("Original braking did not generate native skid geometry")
        quit(1)
        return
    await process_frame
    if DisplayServer.get_name() != "headless":
        await RenderingServer.frame_post_draw
        var with_skids = root.get_texture().get_image()
        host.set_skid_marks_visible(false)
        await process_frame
        await RenderingServer.frame_post_draw
        var without_skids = root.get_texture().get_image()
        host.set_skid_marks_visible(true)
        var changed_pixels = 0
        for y in range(with_skids.get_height()):
            for x in range(with_skids.get_width()):
                var a = with_skids.get_pixel(x,y)
                var b = without_skids.get_pixel(x,y)
                if absf(a.r-b.r)+absf(a.g-b.g)+absf(a.b-b.b)>0.02:
                    changed_pixels += 1
        if changed_pixels<2:
            push_error("Native skid meshes produced no visible pixels")
            quit(1)
            return
        print("Native skid pixels: ",changed_pixels)
        for argument in OS.get_cmdline_user_args():
            if argument.begins_with("--capture="):
                with_skids.save_png(argument.trim_prefix("--capture="))
    if not host.drive_test_step(1.0 / 60.0, 0, 0, 0, true):
        quit(1)
        return
    if host.get_vehicle_position().distance_to(spawn) > 0.001:
        push_error("Test reset did not restore spawn")
        quit(1)
        return
    if host.get_particle_count() != 0:
        push_error("Reset retained original particle state")
        quit(1)
        return
    if host.get_skid_vertex_count() != 0:
        push_error("Reset retained skid geometry")
        quit(1)
        return
    print("ROADWARS_DRIVE_SMOKE_OK: displacement=", driven.distance_to(spawn), "; speed=", speed, "; throttle/steer/brake/reset")
    root.remove_child(host)
    if host.get_live_rids() != 0:
        quit(1)
        return
    host.free()
    quit(0)
