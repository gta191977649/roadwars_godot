extends SceneTree

func _initialize() -> void:
    call_deferred("run_test")

func key_event(host, code: Key, pressed: bool, unicode_value: int = 0) -> void:
    var event = InputEventKey.new()
    event.keycode = code
    event.physical_keycode = code
    event.pressed = pressed
    event.unicode = unicode_value
    Input.parse_input_event(event)

func click(host, position: Vector2, pressed: bool) -> void:
    var motion = InputEventMouseMotion.new()
    motion.position = position
    Input.parse_input_event(motion)
    var event = InputEventMouseButton.new()
    event.position = position
    event.button_index = MOUSE_BUTTON_LEFT
    event.pressed = pressed
    Input.parse_input_event(event)

func run_test() -> void:
    var host = ClassDB.instantiate("GameHost")
    root.add_child(host)
    for frame in range(8):
        await process_frame
    if not host.get_console_collapsed() or host.get_debug_live_rids() < 4:
        push_error("Debug console must initialize collapsed with live draw resources")
        quit(1)
        return
    if DisplayServer.get_name() != "headless":
        if host.get_render_triangles() <= 0:
            push_error("Render profiler did not receive actual viewport primitives")
            quit(1)
            return
        await RenderingServer.frame_post_draw
        root.get_texture().get_image().save_png("res://../../../build/debug_overlay_collapsed.png")
    var scale: float = host.get_debug_dpi_scale()
    print("ROADWARS_LOG_CAPTURE_TEST")
    push_warning("ROADWARS_WARNING_CAPTURE_TEST")
    push_error("ROADWARS_ERROR_CAPTURE_TEST_EXPECTED")
    host.debug_stdio_test()
    for frame in range(4):
        await process_frame
    for message in ["ROADWARS_LOG_CAPTURE_TEST", "ROADWARS_WARNING_CAPTURE_TEST", "ROADWARS_ERROR_CAPTURE_TEST_EXPECTED", "ROADWARS_STDOUT_CAPTURE_TEST", "ROADWARS_STDERR_CAPTURE_TEST"]:
        if not host.console_contains(message):
            push_error("Debug console missed message: " + message)
            quit(1)
            return
    key_event(host, KEY_F8, true)
    key_event(host, KEY_F8, false)
    await process_frame
    if not host.get_ui_mouse_visible() or Input.mouse_mode != Input.MOUSE_MODE_VISIBLE:
        quit(1)
        return
    click(host, Vector2(22, 22) * scale, true)
    await process_frame
    click(host, Vector2(22, 22) * scale, false)
    for frame in range(4):
        await process_frame
    if host.get_console_collapsed() or not host.get_ui_mouse_visible():
        push_error("Console title click failed to expand without stealing game focus")
        quit(1)
        return
    click(host, host.get_console_input_position(), true)
    await process_frame
    click(host, host.get_console_input_position(), false)
    await process_frame
    for character in 'echo "ROADWARS_COMMAND_TEST"':
        var code = character.unicode_at(0)
        key_event(host, code, true, code)
        await process_frame
        key_event(host, code, false)
        await process_frame
    key_event(host, KEY_ENTER, true)
    await process_frame
    key_event(host, KEY_ENTER, false)
    for frame in range(4):
        await process_frame
    if not host.console_contains("ROADWARS_COMMAND_TEST"):
        await RenderingServer.frame_post_draw
        root.get_texture().get_image().save_png("res://../../../build/debug_overlay.png")
        push_error("Console command input did not execute")
        quit(1)
        return
    if DisplayServer.get_name() != "headless":
        await RenderingServer.frame_post_draw
        root.get_texture().get_image().save_png("res://../../../build/debug_overlay.png")
    click(host, Vector2(root.size.x - 20, root.size.y - 20), true)
    click(host, Vector2(root.size.x - 20, root.size.y - 20), false)
    await process_frame
    if host.get_ui_mouse_visible() or ("--race-track=Dark Forest" in OS.get_cmdline_user_args() and Input.mouse_mode != Input.MOUSE_MODE_CAPTURED):
        push_error("Game background click did not restore focus")
        quit(1)
        return
    for cycle in range(3):
        root.remove_child(host)
        if host.get_debug_live_rids() != 0:
            push_error("Debug overlay retained RIDs after exit")
            quit(1)
            return
        host.free()
        host = ClassDB.instantiate("GameHost")
        root.add_child(host)
        for frame in range(3):
            await process_frame
        if not host.get_console_collapsed():
            quit(1)
            return
    root.remove_child(host)
    host.free()
    print("ROADWARS_DEBUG_OVERLAY_SMOKE_OK")
    quit(0)
