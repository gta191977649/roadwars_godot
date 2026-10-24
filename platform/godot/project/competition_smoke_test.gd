extends SceneTree

var host
const Presentation = preload("res://presentation_assertions.gd")

func _initialize() -> void:
    call_deferred("run_test")

func require(value: bool, message: String) -> bool:
    if not value:
        push_error(message)
        quit(1)
    return value

func pump(frames: int = 2) -> bool:
    for frame in range(frames):
        if not require(host.competition_test_step(0.0, false), "Competition presentation failed"):
            return false
        if not Presentation.verify_presentation(host, host.get_presentation_state()):
            quit(1)
            return false
        await process_frame
    return true

func key_event(code: Key, pressed: bool) -> void:
    var event = InputEventKey.new()
    event.keycode = code
    event.physical_keycode = code
    event.pressed = pressed
    root.push_input(event)

func mouse_motion(position: Vector2) -> void:
    var event = InputEventMouseMotion.new()
    event.position = position
    root.push_input(event)

func mouse_button(position: Vector2, pressed: bool) -> void:
    var event = InputEventMouseButton.new()
    event.position = position
    event.button_index = MOUSE_BUTTON_LEFT
    event.pressed = pressed
    root.push_input(event)

func capture(name: String) -> bool:
    if DisplayServer.get_name() == "headless":
        return true
    var destination = ""
    for argument in OS.get_cmdline_user_args():
        if argument.begins_with("--capture-dir="):
            destination = argument.trim_prefix("--capture-dir=")
    if destination == "":
        return true
    await RenderingServer.frame_post_draw
    var image = root.get_texture().get_image()
    return require(image.save_png(destination.path_join(name + ".png")) == OK,
                   "Cannot save competition capture")

func run_test() -> void:
    root.size = Vector2i(960, 540)
    host = ClassDB.instantiate("GameHost")
    root.add_child(host)
    host.set_process(false)
    if not await pump(4):
        return
    var initial: Dictionary = host.get_competition_state()
    var expected_ai = 5
    for argument in OS.get_cmdline_user_args():
        if argument.begins_with("--race-ai="):
            expected_ai = argument.trim_prefix("--race-ai=").to_int()
    if not require(initial.get("participants", 0) == expected_ai + 1,
                   "Competition did not instantiate the requested grid"):
        return
    if not require(initial.get("phase", "") == "countdown", "Competition did not start at countdown"):
        return
    if not require(initial.get("ai_rows", -1) == expected_ai, "AI debug rows do not match live opponents"):
        return
    var initial_rids: int = host.get_live_rids()
    if not require(initial_rids > 0, "Competition renderer has no resources"):
        return
    if not await capture("countdown"):
        return

    # Use real engine input to operate the same checkboxes as the user.
    if DisplayServer.get_name() == "headless":
        host.notification(MainLoop.NOTIFICATION_APPLICATION_FOCUS_IN)
    key_event(KEY_QUOTELEFT, true)
    key_event(KEY_QUOTELEFT, false)
    if not await pump():
        return
    if not require(host.get_ui_mouse_visible(), "Backquote did not focus the race debug UI"):
        return
    if not require(host.get_race_debug_line_count() == 0, "Race debug primitives must default off"):
        return
    if not require(not initial.get("auto_drive", true), "Auto Drive must default off"):
        return
    for index in range(5):
        var position: Vector2 = host.get_race_debug_checkbox_position(index)
        if not require(position.x > 0 and position.y > 0, "Missing race debug checkbox"):
            return
        mouse_motion(position)
        if not await pump():
            return
        mouse_button(position, true)
        if not await pump():
            return
        mouse_button(position, false)
        if not await pump():
            return
        var flags: Array = host.get_competition_state().get("debug_flags", [])
        print("COMPETITION_UI_CHECK ", index, " position=", position, " flags=", flags,
              " dpi=", host.get_debug_dpi_scale(), " ui=", host.get_ui_mouse_visible())
        var checked = flags[index] if index < 4 and flags.size() == 4 else host.get_competition_state().get("auto_drive", false)
        if not checked:
            await capture("failure_checkbox_" + str(index))
        if not require(flags.size() == 4 and checked, "Race debug checkbox did not toggle: " + str(index)):
            return
    # Verify that the gameplay control can be disabled and enabled again.
    for enabled in [false, true]:
        var position = host.get_race_debug_checkbox_position(4)
        mouse_motion(position)
        if not await pump():
            return
        mouse_button(position, true)
        if not await pump():
            return
        mouse_button(position, false)
        if not await pump():
            return
        if not require(host.get_competition_state().get("auto_drive") == enabled,
                       "Auto Drive did not restore the requested control mode"):
            return
    # Inspect is a camera selector, initially the player. Operate its real popup.
    if not require(host.get_competition_state().get("camera_target", 0) == -1,
                   "Inspect must default to the player"):
        return
    for target in [0, -1]:
        var before_eye = host.get_camera_position()
        for row in [0, 2 if target == 0 else 1]:
            var position = host.get_race_debug_inspect_position(row)
            if not require(position.x > 0, "Missing Inspect combo item"):
                return
            mouse_motion(position)
            if not await pump():
                return
            mouse_button(position, true)
            if not await pump():
                return
            mouse_button(position, false)
            if not await pump():
                return
        if not require(host.get_competition_state().get("camera_target", -2) == target and
                       host.get_camera_position().distance_to(before_eye) > 1,
                       "Inspect did not switch the camera target"):
            return
    # Escape transfers focus from debug UI to the original pause menu.
    key_event(KEY_ESCAPE, true)
    key_event(KEY_ESCAPE, false)
    if not await pump():
        return
    if not require(host.get_competition_state().get("paused", false) and
                   not host.get_ui_mouse_visible(), "Pause menu did not take keyboard focus"):
        return
    key_event(KEY_ESCAPE, true)
    key_event(KEY_ESCAPE, false)
    if not await pump():
        return
    key_event(KEY_F8, true)
    key_event(KEY_F8, false)
    if not await pump():
        return
    if not require(host.get_race_debug_line_count() > 0, "Route debug produced no visible primitives"):
        return
    if not await capture("race_debug"):
        return
    root.size = Vector2i(960, 540)
    if not await pump():
        return
    if not await capture("race_debug_wide"):
        return
    root.size = Vector2i(640, 480)
    key_event(KEY_F8, true)
    key_event(KEY_F8, false)
    if not await pump():
        return
    if not require(not host.get_ui_mouse_visible(), "F8 did not restore driving focus"):
        return
    key_event(KEY_ESCAPE, true)
    key_event(KEY_ESCAPE, false)
    if not await pump():
        return
    if not require(not host.get_ui_mouse_visible(), "Escape changed debug mouse focus"):
        return
    var paused: Dictionary = host.get_competition_state()
    if not require(paused.get("tree_paused", false) and paused.get("pause_menu", false),
                   "Escape did not pause Godot and open the original race menu"):
        return
    var paused_eye = host.get_camera_position()
    var paused_scheduler_ticks = host.get_simulation_ticks()
    host.set_process(true)
    for frame in range(30):
        await process_frame
    host.set_process(false)
    if not require(host.get_simulation_ticks() == paused_scheduler_ticks,
                   "Godot pause advanced the scheduler/interpolation clock"):
        return
    for sample in range(12):
        if not await pump():
            return
        if not require(host.get_camera_position().is_equal_approx(paused_eye),
                       "Paused presentation camera jittered"):
            return
    if not await capture("pause_menu"):
        return
    if DisplayServer.get_name() != "headless":
        await RenderingServer.frame_post_draw
        var frozen_pixels = root.get_texture().get_image().get_region(Rect2i(0, 350, 640, 130)).get_data()
        host.set_process(true)
        for frame in range(20):
            await process_frame
        host.set_process(false)
        await RenderingServer.frame_post_draw
        var later_pixels = root.get_texture().get_image().get_region(Rect2i(0, 350, 640, 130)).get_data()
        if not require(frozen_pixels == later_pixels, "Paused world pixels changed between frames"):
            return
    if not require(paused.get("paused", false), "Escape did not pause competition"):
        return
    if not require(host.competition_test_step(0.25, true), "Paused competition step failed"):
        return
    var still_paused: Dictionary = host.get_competition_state()
    if not require(still_paused.get("ticks", -1) == paused.get("ticks", -2) and
                   still_paused.get("time", -1) == paused.get("time", -2),
                   "Pause advanced AI or race time"):
        return
    key_event(KEY_ESCAPE, true)
    key_event(KEY_ESCAPE, false)
    if not await pump():
        return
    if not require(not host.get_competition_state().get("paused", true), "Escape did not resume competition"):
        return

    # Sample original intro targets and final countdown at native audio cadence.
    var intro_eye = host.get_camera_position()
    for step in range(36):
        if not require(host.competition_test_step(0.25, false), "Race intro update failed"):
            return
        await process_frame
        if DisplayServer.get_name() != "headless":
            await create_timer(0.25).timeout
        if step in [0, 8, 16, 24, 28, 32]:
            if not await capture("race_intro_" + str(step)):
                return
        if step == 16 and not require(host.get_camera_position().distance_to(intro_eye) > 1,
                                       "Start camera did not move between original targets"):
            return

    # Open the original menu and select its second row, RESTART RACE.
    key_event(KEY_ESCAPE, true)
    key_event(KEY_ESCAPE, false)
    if not await pump():
        return
    print("PAUSE_RESTART_BEFORE ", host.get_competition_state())
    key_event(KEY_DOWN, true)
    key_event(KEY_DOWN, false)
    if not await pump():
        return
    print("PAUSE_RESTART_AFTER ", host.get_competition_state())
    if not require(host.get_competition_state().get("pause_selection", -1) == 1,
                   "Original Restart Race row was not selected"):
        return
    key_event(KEY_ENTER, true)
    key_event(KEY_ENTER, false)
    if not await pump():
        return
    var restarted = host.get_competition_state()
    if not require(not root.get_tree().is_paused() and not restarted.get("paused", true) and
                   restarted.get("time", -1) == 0 and restarted.get("player_laps", -1) == 0 and
                   restarted.get("phase", "") == "countdown",
                   "Pause-menu restart did not reset the real race"):
        return
    if not await capture("pause_restart"):
        return

    # Advance real fixed ticks using the same recovered AI for the test player.
    # No pose, lap, checkpoint or finish state is injected.
    var completed = false
    var moving_pause_checked = false
    for frame in range(3600):
        if not require(host.competition_test_step(0.25, false), "Competition simulation failed"):
            return
        var state: Dictionary = host.get_competition_state()
        if state.get("phase", "") != "results" and not Presentation.verify_presentation(host, host.get_presentation_state()):
            quit(1)
            return
        if not moving_pause_checked and state.get("player_node", -1) in [14, 15]:
            moving_pause_checked = true
            root.size = Vector2i(1280, 960)
            if not await pump():
                return
            if not await capture("route_nodes_14_21"):
                return
            key_event(KEY_ESCAPE, true)
            key_event(KEY_ESCAPE, false)
            if not await pump():
                return
            var frozen_ticks = host.get_simulation_ticks()
            var frozen_presentation: Dictionary = host.get_presentation_state()
            host.set_process(true)
            for sample in range(30):
                await process_frame
            host.set_process(false)
            if not require(host.get_simulation_ticks() == frozen_ticks and
                           host.get_presentation_state() == frozen_presentation,
                           "Moving-race pause advanced scheduler ticks or transforms"):
                return
            if DisplayServer.get_name() != "headless":
                await RenderingServer.frame_post_draw
                var before_image = root.get_texture().get_image().get_region(Rect2i(0, 700, 1280, 250))
                host.set_process(true)
                for sample in range(20):
                    await process_frame
                host.set_process(false)
                await RenderingServer.frame_post_draw
                var after_image = root.get_texture().get_image().get_region(Rect2i(0, 700, 1280, 250))
                var pixels_match = Presentation.frozen_pixels_match(before_image, after_image)
                if not pixels_match:
                    for argument in OS.get_cmdline_user_args():
                        if argument.begins_with("--capture-dir="):
                            var folder = argument.trim_prefix("--capture-dir=")
                            before_image.save_png(folder.path_join("pause_pixels_before.png"))
                            after_image.save_png(folder.path_join("pause_pixels_after.png"))
                if not require(pixels_match and host.get_presentation_state() == frozen_presentation,
                               "Moving-race pause changed world pixels"):
                    return
            if not await capture("pause_moving"):
                return
            key_event(KEY_ESCAPE, true)
            key_event(KEY_ESCAPE, false)
            if not await pump():
                return
            root.size = Vector2i(640, 480)
        if state.get("pause_menu", false) and state.get("terminal", false):
            completed = true
            break
        if frame % 240 == 0:
            print("COMPETITION_PROGRESS ", state)
        if frame == 80:
            if not await capture("race_running"):
                return
            key_event(KEY_F8, true)
            key_event(KEY_F8, false)
            if not await pump():
                return
            for target in [0, -1]:
                var world: Dictionary = host.get_presentation_state().instances
                for row in [0, 2 if target == 0 else 1]:
                    var position = host.get_race_debug_inspect_position(row)
                    mouse_motion(position)
                    if not await pump():
                        return
                    mouse_button(position, true)
                    if not await pump():
                        return
                    mouse_button(position, false)
                    if not await pump():
                        return
                if not require(host.get_competition_state().camera_target == target and
                               world == host.get_presentation_state().instances,
                               "Moving camera cut changed world interpolation history"):
                    return
                for sample in range(120):
                    var dt = [1.0/120, 1.0/144, 1.0/59, 1.0/240][sample % 4]
                    if not require(host.competition_test_step(dt, false), "Moving spectate step failed"):
                        return
                    if not Presentation.verify_presentation(host, host.get_presentation_state()):
                        quit(1)
                        return
                    await process_frame
                if not await capture("race_spectate_" + str(target)):
                    return
            key_event(KEY_F8, true)
            key_event(KEY_F8, false)
            if not await pump():
                return
        await process_frame
    if not require(moving_pause_checked, "Moving-pause validation point was not reached"):
        return
    if not require(completed, "Recovered AI did not finish within 900 simulated seconds"):
        return
    if not require(host.get_competition_state().pause_menu and
                   host.get_competition_state().terminal,
                   "Terminal idle expiry did not open the live race-over menu"):
        return
    # CANCEL/ESC preserves the terminal race for native keyboard spectating.
    key_event(KEY_ESCAPE, true)
    key_event(KEY_ESCAPE, false)
    if not await pump():
        return
    if not require(not host.get_competition_state().paused,
                   "Race-over cancellation did not resume the live terminal race"):
        return
    for selection in [[KEY_F1, 0], [KEY_F2, 4], [KEY_F3, 1], [KEY_F4, 2],
                      [KEY_F5, 3], [KEY_F6, 5]]:
        key_event(selection[0], true)
        key_event(selection[0], false)
        if not require(host.competition_test_step(1.0 / 60, false), "Native spectator step failed"):
            return
        if not require(host.get_competition_state().native_camera_mode == selection[1],
                       "Native function-key camera mode was not selected"):
            return
        await process_frame
    var spectator_before: int = host.get_competition_state().spectator_target
    key_event(KEY_F6, true)
    key_event(KEY_F6, false)
    if not await pump():
        return
    if not require(host.get_competition_state().spectator_target ==
                   (spectator_before + 1) % host.get_competition_state().participants,
                   "Repeated native F6 did not cycle the full participant array"):
        return
    key_event(KEY_F11, true)
    key_event(KEY_F11, false)
    if not require(host.get_competition_state().time_table_remaining == 0,
                   "F11 did not hide the terminal table"):
        return
    key_event(KEY_F11, true)
    key_event(KEY_F11, false)
    if not require(host.get_competition_state().time_table_remaining == 15,
                   "F11 did not restore the terminal table"):
        return
    key_event(KEY_ESCAPE, true)
    key_event(KEY_ESCAPE, false)
    if not await pump():
        return
    if not await capture("race_results"):
        return
    # CANCEL resumes the live terminal world; the third no-replay row exits.
    key_event(KEY_DOWN, true)
    key_event(KEY_DOWN, false)
    if not await pump():
        return
    key_event(KEY_DOWN, true)
    key_event(KEY_DOWN, false)
    if not await pump():
        return
    key_event(KEY_ENTER, true)
    key_event(KEY_ENTER, false)
    # The exit action releases the race session during this step.
    if not require(host.competition_test_step(0.0, false), "Race-over exit action failed"):
        return
    await process_frame
    if not require(host.get_frontend_screen() == "main_menu",
                   "Race-over EXIT TO MAIN MENU did not return to the main menu"):
        return
    for cycle in range(3):
        host.restart_smoke_scene()
        host.set_process(false)
        if not await pump(3):
            return
        if not require(host.get_competition_state().get("phase", "") == "countdown",
                       "Competition restart did not reconstruct the grid"):
            return
        if not require(host.get_live_rids() == initial_rids, "Competition restart changed RID ownership"):
            return
    root.remove_child(host)
    if not require(host.get_live_rids() == 0 and host.get_debug_live_rids() == 0,
                   "Competition exit retained renderer resources"):
        return
    host.free()
    await create_timer(0.1).timeout
    print("ROADWARS_COMPETITION_SMOKE_OK")
    quit(0)
