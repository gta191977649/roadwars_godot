extends SceneTree
var host
var failed := false
func _initialize():
    call_deferred("run_test")
func require(value: bool, message: String) -> bool:
    if not value:
        failed = true
        push_error(message)
        quit(1)
    return value
func step(dt: float = 1.0 / 60) -> bool:
    if not require(host.competition_test_step(dt, true), "Cockpit race step failed"):
        return false
    await process_frame
    return true
func pump() -> bool:
    for frame in range(3):
        if not require(host.competition_test_step(0.0, true), "Cockpit UI pump failed"): return false
        await process_frame
    return true
func key(code: Key, pressed: bool = true, echo: bool = false):
    var event = InputEventKey.new()
    event.keycode = code
    event.physical_keycode = code
    event.pressed = pressed
    event.echo = echo
    root.push_input(event)
func capture(name: String):
    if DisplayServer.get_name() == "headless": return
    await RenderingServer.frame_post_draw
    for argument in OS.get_cmdline_user_args():
        if argument.begins_with("--capture-dir="):
            require(root.get_texture().get_image().save_png(argument.trim_prefix("--capture-dir=").path_join(name + ".png")) == OK, "Cockpit capture failed")
func run_test():
    root.size = Vector2i(960, 720)
    host = ClassDB.instantiate("GameHost")
    root.add_child(host)
    host.set_process(false)
    for tick in range(900):
        if not require(host.competition_test_step(1.0 / 60, true), "Cockpit warm-up failed"): return
        if tick % 30 == 0: await process_frame
    if not require(host.get_competition_state().phase == "racing" and host.get_competition_state().time > 0, "Cockpit test did not reach driving"): return
    if "--cockpit-visual-only" in OS.get_cmdline_user_args():
        host.set_camera_view_for_test(3)
        if not await step(): return
        for size in [Vector2i(960,720),Vector2i(1280,720),Vector2i(1280,800),Vector2i(1680,720),Vector2i(1920,540)]:
            root.size = size
            await process_frame
            host.present_test_frame()
            await capture("cockpit_" + str(size.x) + "x" + str(size.y))
        host.queue_free()
        await process_frame
        await create_timer(.1).timeout
        if not failed:
            print("ROADWARS_COCKPIT_SMOKE_OK")
            quit(0)
        return
    for view in range(4):
        host.set_camera_view_for_test(view)
        if not await step(): return
        var state = host.get_competition_state()
        var presentation = host.get_presentation_state()
        if not require(state.camera_view == view, "Camera mode missing"): return
        if not require((presentation.camera_hidden_instances > 0) == (view >= 2), "Target model visibility mask wrong"): return
        if not require(is_equal_approx(float(presentation.camera_near), .1 if view >= 2 else 1.0), "Camera near plane wrong"): return
        await capture("view_" + str(view))
    # Both aliases must advance exactly once, ignoring OS key repeat.
    key(KEY_C)
    if not await step(): return
    key(KEY_C, true, true)
    if not await step(): return
    key(KEY_C, false)
    if not require(host.get_competition_state().camera_view == 0, "C key repeated or failed"): return
    key(KEY_F1)
    key(KEY_F1, false)
    if not await step(): return
    if not require(host.get_competition_state().camera_view == 1, "F1 alias failed"): return
    host.set_camera_view_for_test(3)
    if not await step(): return
    for size in [Vector2i(960,720),Vector2i(1280,720),Vector2i(1280,800),Vector2i(1680,720),Vector2i(1920,540)]:
        root.size = size
        await process_frame
        host.present_test_frame()
        var before = host.get_vehicle_position()
        var ticks = host.get_simulation_ticks()
        var previous = host.get_presentation_state()
        var quads: Array = previous.aspect_hud_pixels
        if not require(quads.size() >= 12, "Cockpit aspect quads missing"): return
        # The original wheel is the last quad of the cockpit image/needle/map block.
        var wheel_index: int = 13 if size.x * 3 > size.y * 4 else 11
        var wheel: Array = quads[wheel_index]
        var a: Vector2 = wheel[1] - wheel[0]
        var b: Vector2 = wheel[2] - wheel[1]
        if not require(abs(a.length() - b.length()) < .01 and abs(a.dot(b)) < .1, "Steering wheel stretched at " + str(size)): return
        for repeat in range(4): host.present_test_frame()
        if not require(host.get_simulation_ticks() == ticks and host.get_vehicle_position() == before and host.get_presentation_state().camera == previous.camera, "Presentation changed simulation or camera snapshots"): return
        await capture("cockpit_" + str(size.x) + "x" + str(size.y))
    # Operate the actual Inspect popup; the selected mode follows the target.
    root.size = Vector2i(960, 540)
    if not await pump(): return
    if not host.get_ui_mouse_visible():
        key(KEY_F8)
        key(KEY_F8, false)
        if not await pump(): return
    for target in [0, -1]:
        for row in [0, 2 if target == 0 else 1]:
            var position = host.get_race_debug_inspect_position(row)
            if not require(position.x > 0, "Inspect popup missing"): return
            var motion = InputEventMouseMotion.new()
            motion.position = position
            root.push_input(motion)
            if not await pump(): return
            var click = InputEventMouseButton.new()
            click.position = position
            click.button_index = MOUSE_BUTTON_LEFT
            click.pressed = true
            root.push_input(click)
            if not await pump(): return
            click = InputEventMouseButton.new()
            click.position = position
            click.button_index = MOUSE_BUTTON_LEFT
            click.pressed = false
            root.push_input(click)
            if not await pump(): return
        if not await pump(): return
        if not await step(): return
        var target_state = host.get_competition_state()
        print("COCKPIT_INSPECT ", target, " state=", target_state.camera_target, " view=", target_state.camera_view, " cockpit=", target_state.cockpit_target)
        if not require(target_state.camera_target == target and target_state.camera_view == 3 and target_state.cockpit_target == (0 if target == 0 else 5), "Inspect lost cockpit mode or target"): return
        await capture("cockpit_ai" if target == 0 else "cockpit_player_restored")
    key(KEY_F8)
    key(KEY_F8, false)
    if not await pump(): return
    # Mixed render cadence must remain finite and preserve the presentation contract.
    for dt in [1.0/240,1.0/120,1.0/75,1.0/30,.35,.9]:
        if not await step(dt): return
        var state = host.get_competition_state()
        if not require(state.camera_view == 3, "Cadence changed mode"): return
        var rpm: float = lerpf(float(state.cockpit_previous_rpm),float(state.cockpit_current_rpm),float(state.cockpit_alpha))
        if not require(abs(rpm - float(state.cockpit_rpm)) < .02, "Cockpit used different interpolation interval"): return
    key(KEY_ESCAPE)
    key(KEY_ESCAPE, false)
    host.set_process(true)
    for frame in range(5): await process_frame
    host.set_process(false)
    if not require(host.get_competition_state().paused, "Cockpit pause failed"): return
    var frozen = host.get_simulation_ticks()
    key(KEY_C)
    key(KEY_C, false)
    if not await step(.2): return
    if not require(host.get_simulation_ticks() == frozen and host.get_competition_state().camera_view == 3, "Pause changed mode or simulation"): return
    key(KEY_ESCAPE)
    key(KEY_ESCAPE, false)
    host.set_process(true)
    for frame in range(5): await process_frame
    host.set_process(false)
    if not require(not host.get_competition_state().paused, "Cockpit resume failed"): return
    # Restart via the real pause menu, retaining cockpit mode.
    key(KEY_ESCAPE)
    key(KEY_ESCAPE, false)
    host.set_process(true)
    await create_timer(.4).timeout
    key(KEY_DOWN)
    key(KEY_DOWN, false)
    await create_timer(.1).timeout
    key(KEY_ENTER)
    key(KEY_ENTER, false)
    await create_timer(.1).timeout
    host.set_process(false)
    if not require(host.get_competition_state().camera_view == 3 and not host.get_competition_state().paused and host.get_competition_state().time < 1, "Restart did not preserve cockpit mode"): return
    key(KEY_ESCAPE)
    key(KEY_ESCAPE, false)
    host.set_process(true)
    await create_timer(.4).timeout
    for row in range(3):
        key(KEY_DOWN)
        key(KEY_DOWN, false)
        await create_timer(.1).timeout
    key(KEY_ENTER)
    key(KEY_ENTER, false)
    await create_timer(.2).timeout
    host.set_process(false)
    if not require(host.get_frontend_screen() == "main_menu" and host.get_competition_state().camera_view == 0 and host.get_presentation_state().camera_hidden_instances == 0, "Cockpit exit did not restore default camera and release masks"): return
    host.queue_free()
    await process_frame
    await create_timer(.1).timeout
    if not failed:
        print("ROADWARS_COCKPIT_SMOKE_OK")
        quit(0)
