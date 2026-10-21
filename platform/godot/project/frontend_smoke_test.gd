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
    if host.get_frontend_player_position() != Vector2(67, 404):
        fail("Dark Forest Start1 HUD anchor differs from original configuration")
        return
    var count = host.get_live_rids()
    if DisplayServer.get_name() != "headless":
        host.present_test_frame()
        await RenderingServer.frame_post_draw
        var shown = root.get_texture().get_image()
        host.set_frontend_visible(false)
        await process_frame
        await RenderingServer.frame_post_draw
        var hidden = root.get_texture().get_image()
        var map_pixels = 0
        var meter_pixels = 0
        for y in range(shown.get_height() * 352 / 480, shown.get_height()):
            for x in range(shown.get_width()):
                var a = shown.get_pixel(x, y)
                var b = hidden.get_pixel(x, y)
                if abs(a.r-b.r) + abs(a.g-b.g) + abs(a.b-b.b) > 0.03:
                    if x < shown.get_width() / 5:
                        map_pixels += 1
                    if x >= shown.get_width() * 4 / 5:
                        meter_pixels += 1
        if map_pixels < 500 or meter_pixels < 500:
            fail("HUD textures did not change both native canvas screen regions")
            return
        var label_pixels = 0
        for y in range(shown.get_height() * 440 / 480, shown.get_height()):
            for x in range(shown.get_width() * 200 / 640, shown.get_width() * 440 / 640):
                var a = shown.get_pixel(x, y)
                var b = hidden.get_pixel(x, y)
                if abs(a.r-b.r) + abs(a.g-b.g) + abs(a.b-b.b) > 0.03:
                    label_pixels += 1
        var debug_enabled = "--debug" in OS.get_cmdline_user_args()
        if (debug_enabled and label_pixels < 100) or (not debug_enabled and label_pixels != 0):
            fail("Dev Build label visibility does not match --debug")
            return
        print("Dev Build label pixels: ", label_pixels, "; --debug=", debug_enabled)
        host.set_frontend_visible(true)
        print("Frontend visible pixels: map=", map_pixels, " meter=", meter_pixels)
    for tick in range(360):
        if not host.drive_test_step(1.0 / 120, 0, 1, 0, false):
            fail("Driving HUD update failed")
            return
    if host.get_frontend_player_position().distance_to(Vector2(67, 404)) < 1:
        fail("Player map dot did not follow actual vehicle motion")
        return
    if DisplayServer.get_name() != "headless":
        host.present_test_frame()
        await process_frame
        await RenderingServer.frame_post_draw
        for argument in OS.get_cmdline_user_args():
            if argument.begins_with("--capture="):
                root.get_texture().get_image().save_png(argument.trim_prefix("--capture="))
        root.size = Vector2i(800, 600)
        host.present_test_frame()
        await process_frame
        await RenderingServer.frame_post_draw
    if not host.drive_test_step(1.0 / 30, 0, 0, 0, true):
        fail("HUD reset failed")
        return
    if host.get_frontend_player_position() != Vector2(67, 404):
        fail("HUD map reset lost Start1 anchor")
        return
    for cycle in range(3):
        host.restart_smoke_scene()
        host.set_process(false)
        await process_frame
        if host.get_live_rids() != count:
            fail("Frontend textures/canvas RIDs changed across restart")
            return
    root.remove_child(host)
    if host.get_live_rids() != 0:
        fail("Frontend teardown retained RIDs")
        return
    host.free()
    print("ROADWARS_FRONTEND_SMOKE_OK: textures, player motion, reset, resize, 3 restarts")
    quit(0)
