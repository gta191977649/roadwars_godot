extends SceneTree

var host

func _initialize() -> void:
    call_deferred("run_test")

func require(value: bool, message: String) -> bool:
    if not value:
        push_error(message)
        quit(1)
    return value

func key(code: Key) -> void:
    for pressed in [true, false]:
        var event = InputEventKey.new()
        event.keycode = code
        event.physical_keycode = code
        event.pressed = pressed
        root.push_input(event)

func pump() -> void:
    for frame in range(3):
        host.competition_test_step(0.0, false)
        await process_frame

func run_test() -> void:
    root.size = Vector2i(960,540)
    host = ClassDB.instantiate("GameHost")
    root.add_child(host)
    host.set_process(false)
    await pump()
    host.notification(MainLoop.NOTIFICATION_APPLICATION_FOCUS_IN)
    for tick in range(1500):
        if not require(host.competition_test_step(1.0/60,false),"Replay capture race failed"):
            return
        if tick % 120 == 0:
            await process_frame
    var live: Dictionary = host.get_competition_state()
    if not require(live.get("replay_available",false),"Diagnostic recorder did not expose genuine ready replay"):
        return
    key(KEY_ESCAPE)
    await pump()
    # Native CONTINUE, RESTART RACE, REPLAY sequence.
    key(KEY_DOWN)
    await pump()
    key(KEY_DOWN)
    await pump()
    key(KEY_ENTER)
    await pump()
    var replay: Dictionary = host.get_competition_state()
    if not require(replay.get("replaying",false) and replay.replay_selection == 2,
                   "Original pause REPLAY did not enter seven-button interface"):
        return
    for tick in range(240):
        if not require(host.competition_test_step(1.0/60,false),"Real replay world update failed"):
            return
        if tick % 60 == 0:
            await process_frame
    if DisplayServer.get_name() != "headless":
        await process_frame
        await RenderingServer.frame_post_draw
        for argument in OS.get_cmdline_user_args():
            if argument.begins_with("--capture="):
                if not require(root.get_texture().get_image().save_png(argument.trim_prefix("--capture=")) == OK,
                               "Cannot save native Replay overlay capture"):
                    return
    key(KEY_ENTER)
    var paused: Dictionary = host.get_competition_state()
    host.competition_test_step(0.5,false)
    if not require(paused.replay_paused and host.get_competition_state().replay_time == paused.replay_time,
                   "Native replay play/pause button failed"):
        return
    # Forward step, last, exit; selection is clamped and operated by Enter.
    key(KEY_RIGHT)
    key(KEY_ENTER)
    key(KEY_RIGHT)
    key(KEY_ENTER)
    var final_sample: Dictionary = host.get_competition_state()
    key(KEY_RIGHT)
    key(KEY_ENTER)
    await pump()
    var restored: Dictionary = host.get_competition_state()
    if not require(not restored.replaying and restored.ticks == live.ticks and restored.time == final_sample.time,
                   "Replay exit did not retain the native last-sample clock"):
        return
    if not require(restored.native_camera_mode == 4,"Native replay exit did not select chase camera"):
        return
    if not require(host.competition_test_step(1.0/60,false),"Restored actual race did not continue"):
        return
    var rear = InputEventKey.new()
    rear.keycode = KEY_KP_0
    rear.physical_keycode = KEY_KP_0
    rear.pressed = true
    root.push_input(rear)
    host.competition_test_step(1.0/60,false)
    if not require(host.get_competition_state().look_back,"Original configured rear camera did not engage"):
        return
    rear.pressed = false
    root.push_input(rear)
    host.competition_test_step(1.0/60,false)
    if not require(not host.get_competition_state().look_back,"Original held rear camera did not release"):
        return
    root.remove_child(host)
    if not require(host.get_live_rids()==0 and host.get_debug_live_rids()==0,"Replay teardown leaked rendering owners"):
        return
    host.free()
    print("ROADWARS_REPLAY_SMOKE_OK")
    quit(0)
