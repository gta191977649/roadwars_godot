extends SceneTree

# Infrastructure acceptance only; no recovered game behavior is exercised.
func _initialize() -> void:
    call_deferred("run_test")

func run_test() -> void:
    var host = ClassDB.instantiate("GameHost")
    if host == null:
        push_error("GameHost extension is not registered")
        quit(1)
        return
    root.add_child(host)
    for cycle in range(10):
        host.restart_smoke_scene()
        await process_frame
        if host.get_live_rids() != 6:
            push_error("Unexpected owned RID count")
            quit(1)
            return
    if DisplayServer.get_name() != "headless":
        await RenderingServer.frame_post_draw
        var image = root.get_texture().get_image()
        var colored_pixels = 0
        for y in range(image.get_height()):
            for x in range(image.get_width()):
                var pixel = image.get_pixel(x, y)
                if pixel.g > 0.5 and pixel.g > pixel.r * 2.0:
                    colored_pixels += 1
        if colored_pixels < 100:
            push_error("RenderingServer triangle not visible")
            quit(1)
            return
        for argument in OS.get_cmdline_user_args():
            if argument.begins_with("--capture="):
                if image.save_png(argument.trim_prefix("--capture=")) != OK:
                    push_error("Could not save smoke capture")
                    quit(1)
                    return
        print("Visible triangle pixels: ", colored_pixels)
    root.remove_child(host)
    if host.get_live_rids() != 0:
        push_error("RIDs retained after tree exit")
        quit(1)
        return
    host.free()
    await process_frame
    print("ROADWARS_SMOKE_OK: 10 restarts; all owned RIDs released")
    quit(0)
