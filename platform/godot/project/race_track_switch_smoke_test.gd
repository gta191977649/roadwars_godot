extends SceneTree

var host

func _initialize() -> void:
    call_deferred("run_test")

func require(value: bool, message: String) -> bool:
    if not value:
        push_error(message)
        quit(1)
    return value

func pump(frames: int = 2) -> bool:
    for frame in range(frames):
        if not require(host.competition_test_step(0.0, false), "Track-switch presentation failed"):
            return false
        await process_frame
    return true

func key_event(code: Key, pressed: bool) -> void:
    var event = InputEventKey.new()
    event.keycode = code
    event.physical_keycode = code
    event.pressed = pressed
    root.push_input(event)

func click(position: Vector2) -> bool:
    if not require(position.x > 0 and position.y > 0, "Track debug control is not visible"):
        return false
    var motion = InputEventMouseMotion.new()
    motion.position = position
    root.push_input(motion)
    if not await pump():
        return false
    for pressed in [true, false]:
        var event = InputEventMouseButton.new()
        event.position = position
        event.button_index = MOUSE_BUTTON_LEFT
        event.pressed = pressed
        root.push_input(event)
        if not await pump():
            return false
    return true

func same_setup(state: Dictionary, initial: Dictionary) -> bool:
    for field in ["vehicle", "laps", "participants", "diagnostic_seed", "debug_flags", "auto_drive", "camera_target"]:
        if not require(state.get(field) == initial.get(field), "Track switch changed " + field):
            return false
    return true

func run_test() -> void:
    root.size = Vector2i(960, 540)
    host = ClassDB.instantiate("GameHost")
    root.add_child(host)
    host.set_process(false)
    if DisplayServer.get_name() == "headless":
        host.notification(MainLoop.NOTIFICATION_APPLICATION_FOCUS_IN)
    if not await pump(4):
        return
    var initial: Dictionary = host.get_competition_state()
    var tracks: Array = initial.get("available_tracks", [])
    if not require(tracks.size() > 1, "Original track-selection table has fewer than two available tracks"):
        return
    var original: String = initial.get("track", "")
    var other_index = -1
    for index in range(tracks.size()):
        if str(tracks[index]).to_upper() != original.to_upper():
            other_index = index
            break
    if not require(other_index >= 0, "Original track list contains no alternate track"):
        return
    var alternate: String = tracks[other_index]
    if not require(not host.request_race_debug_track("__missing_track__"), "Unknown track request accepted"):
        return
    if not require(host.request_race_debug_track(original.to_upper()), "Current canonical track was rejected"):
        return
    if not await pump():
        return
    if not require(host.get_competition_state().get("track_generation") == initial.get("track_generation"), "Selecting the current track rebuilt the race"):
        return
    key_event(KEY_F8, true)
    key_event(KEY_F8, false)
    if not await pump():
        return
    if not await click(host.get_race_debug_checkbox_position(1)):
        return
    initial = host.get_competition_state()
    var original_rids: int = host.get_live_rids()
    if not await click(host.get_race_debug_track_position(0)):
        return
    if not await click(host.get_race_debug_track_position(other_index + 1)):
        return
    var switched: Dictionary = host.get_competition_state()
    if not require(str(switched.get("track")).to_upper() == alternate.to_upper() and switched.get("track_generation") == initial.get("track_generation") + 1, "Actual dropdown did not rebuild the selected original track"):
        return
    if not require(switched.get("ticks", -1) == 0 and switched.get("phase") == "countdown", "Track change did not start a fresh race") or not same_setup(switched, initial):
        return
    key_event(KEY_ESCAPE, true)
    key_event(KEY_ESCAPE, false)
    if not await pump():
        return
    if not require(host.get_competition_state().get("paused", false), "Cannot pause switched race"):
        return
    key_event(KEY_F8, true)
    key_event(KEY_F8, false)
    if not await pump():
        return
    if not require(host.request_race_debug_track(original), "Paused track request rejected"):
        return
    if not require(str(host.get_competition_state().get("track")).to_upper() == alternate.to_upper(), "Track request rebuilt synchronously inside UI dispatch"):
        return
    if not await pump(4):
        return
    var paused: Dictionary = host.get_competition_state()
    if not require(str(paused.get("track")).to_upper() == original.to_upper() and paused.get("paused", false) and paused.get("tree_paused", false) and paused.get("pause_menu", false), "Paused switch did not rebuild and preserve pause/menu state") or not same_setup(paused, initial):
        return
    key_event(KEY_ESCAPE, true)
    key_event(KEY_ESCAPE, false)
    if not await pump(4):
        return
    if not require(not host.get_competition_state().get("paused", true), "Rebuilt pause menu cannot resume"):
        return
    for track in [alternate, original]:
        if not require(host.request_race_debug_track(track), "Repeated track switch rejected"):
            return
        if not await pump(4):
            return
    if not require(host.get_live_rids() == original_rids, "Repeated track teardown retained renderer resources"):
        return
    if not require(host.competition_test_step(1.0 / 60.0, false), "Fresh switched race cannot simulate"):
        return
    var advanced: Dictionary = host.get_competition_state()
    if not require(advanced.get("ticks", 0) > 0 and not advanced.get("paused", true), "Resumed switched race did not advance its simulation"):
        return
    if DisplayServer.get_name() != "headless":
        for argument in OS.get_cmdline_user_args():
            if argument.begins_with("--capture="):
                await RenderingServer.frame_post_draw
                if not require(root.get_texture().get_image().save_png(argument.trim_prefix("--capture=")) == OK, "Cannot save track dropdown capture"):
                    return
    print("RACE_TRACK_SWITCH_OK original=%s alternate=%s reloads=4 preferences=preserved paused_menu=preserved" % [original, alternate])
    host.free()
    await process_frame
    quit(0)
