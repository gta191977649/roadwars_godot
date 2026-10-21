extends SceneTree

# Asset/render lifecycle acceptance. Driving is tested separately in the portable core.
func _initialize() -> void:
    call_deferred("run_test")

func run_test() -> void:
    var host = ClassDB.instantiate("GameHost")
    root.add_child(host)
    var count = host.get_live_rids()
    if count < 100:
        push_error("Original scene did not upload")
        quit(1)
        return
    for cycle in range(3):
        host.restart_smoke_scene()
        await process_frame
        if host.get_live_rids() != count:
            push_error("Race RID count changed across restart")
            quit(1)
            return
    if DisplayServer.get_name() != "headless":
        await RenderingServer.frame_post_draw
        var image = root.get_texture().get_image()
        var sky_pixels = 0
        for y in range(image.get_height() / 4):
            for x in range(image.get_width()):
                var pixel = image.get_pixel(x,y)
                if pixel.b > pixel.g + 0.02 and pixel.b > pixel.r + 0.05:
                    sky_pixels += 1
        if sky_pixels < 1000 and "--race-track=Dark Forest" in OS.get_cmdline_user_args():
            push_error("Original Dark Forest sky was not visible")
            quit(1)
            return
        print("Original sky pixels: ", sky_pixels)
        for argument in OS.get_cmdline_user_args():
            if argument.begins_with("--capture="):
                if image.save_png(argument.trim_prefix("--capture=")) != OK:
                    quit(1)
                    return
    root.remove_child(host)
    if host.get_live_rids() != 0:
        push_error("Race RIDs retained after tree exit")
        quit(1)
        return
    host.free()
    print("ROADWARS_RACE_SMOKE_OK: ", count, " RIDs; 3 restarts; all released")
    quit(0)
