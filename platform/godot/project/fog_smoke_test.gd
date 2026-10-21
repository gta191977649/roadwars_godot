extends SceneTree

func _initialize() -> void:
    call_deferred("run_test")

func run_test() -> void:
    var host = ClassDB.instantiate("GameHost")
    root.add_child(host)
    host.start_fog_fixture()
    await process_frame
    await RenderingServer.frame_post_draw
    var image = root.get_texture().get_image()
    # Original vertex-linear fog: depths100,155,185,199; start140,end200.
    var remaining = [1.0, 0.75, 0.25, 1.0/60.0]
    for i in range(4):
        var x = int((0.2 + i*0.2)*image.get_width())
        var fogged = image.get_pixel(x, int(image.get_height()*0.3))
        var exempt = image.get_pixel(x, int(image.get_height()*0.7))
        var expected = Color(remaining[i],remaining[i],remaining[i]).linear_to_srgb().r
        if abs(fogged.r-expected)>0.035 or exempt.r<0.97:
            push_error("Original linear fog mismatch at depth index %d: %s expected %f, exempt %s" % [i,fogged,expected,exempt])
            quit(1)
            return
    for argument in OS.get_cmdline_user_args():
        if argument.begins_with("--capture="):
            if image.save_png(argument.trim_prefix("--capture=")) != OK:
                push_error("Could not save fog fixture capture")
                quit(1)
                return
    root.remove_child(host)
    if host.get_live_rids()!=0:
        push_error("Fog fixture retained RIDs")
        quit(1)
        return
    host.free()
    print("ROADWARS_FOG_SMOKE_OK: linear vertex fog and fog exemption rendered")
    quit(0)
