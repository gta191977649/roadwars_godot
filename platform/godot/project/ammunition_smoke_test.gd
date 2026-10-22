extends SceneTree
var host
var failed := false
func _initialize(): call_deferred("run_test")
func require(value: bool, message: String) -> bool:
    if not value:
        failed = true
        push_error(message)
        quit(1)
    return value
func key(code: Key):
    for pressed in [true, false]:
        var event = InputEventKey.new()
        event.keycode = code
        event.physical_keycode = code
        event.pressed = pressed
        root.push_input(event)
func capture(name: String):
    if DisplayServer.get_name() == "headless": return
    await RenderingServer.frame_post_draw
    for argument in OS.get_cmdline_user_args():
        if argument.begins_with("--capture-dir="):
            require(root.get_texture().get_image().save_png(argument.trim_prefix("--capture-dir=").path_join(name + ".png")) == OK,"Ammo capture failed")
func run_test():
    root.size = Vector2i(1280,720)
    host = ClassDB.instantiate("GameHost")
    root.add_child(host)
    host.set_process(false)
    var before: Dictionary = host.get_competition_state()
    if not require(host.competition_weapon_test_step(1.0/60,0,false), "Locked weapon step failed"): return
    var locked = host.get_competition_state()
    if not require(locked.weapon_ammunition == before.weapon_ammunition, "Locked weapon consumed ammunition"): return
    for notice in locked.ammunition_notices:
        if not require(notice.remaining == 0, "Rejected weapon produced a notification"): return
    for tick in range(1800):
        if not require(host.competition_test_step(1.0/60,false), "Ammo warmup failed"): return
        if tick % 60 == 0: await process_frame
    var loadouts: Array = host.get_competition_state().loadouts
    var player: int = loadouts.size()-1
    for slot in range(3):
        before = host.get_competition_state()
        if not require(host.competition_weapon_test_step(1.0/60,slot,false), "Weapon activation failed"): return
        var after = host.get_competition_state()
        if not require(after.weapon_ammunition[slot] == before.weapon_ammunition[slot]-1, "Weapon activation was not accepted"): return
        var expected: String = str(loadouts[player].weapons[slot]).to_upper()+" "+str(after.weapon_ammunition[slot])
        var found := false
        for notice in after.ammunition_notices:
            found = found or (notice.text == expected and notice.remaining > 1.8)
        if not require(found, "Missing post-consumption count: "+expected): return
    before = host.get_competition_state()
    if not require(before.nitrous_ammunition > 0 and host.competition_weapon_test_step(1.0/60,-1,true), "Nitrous activation failed"): return
    var used = host.get_competition_state()
    if not require(used.nitrous_ammunition == before.nitrous_ammunition-1, "Nitrous count incorrect"): return
    var nitrous_found := false
    for notice in used.ammunition_notices:
        nitrous_found = nitrous_found or (notice.text == "NITROUS BOOST "+str(used.nitrous_ammunition) and notice.remaining > 1.8)
    if not require(nitrous_found, "Nitrous notification missing"): return
    for view in range(4):
        host.set_camera_view_for_test(view)
        if not require(host.competition_test_step(1.0/60,false), "View change failed"): return
        await process_frame
        await capture("ammunition_view_"+str(view))
        if view == 0:
            for size in [Vector2i(960,720),Vector2i(1920,540)]:
                root.size = size
                await process_frame
                host.present_test_frame()
                await capture("ammunition_"+str(size.x)+"x"+str(size.y))
            root.size = Vector2i(1280,720)
            await process_frame
            host.present_test_frame()
    var frozen: Array = host.get_competition_state().ammunition_notices
    for repeat in range(8): host.present_test_frame()
    if not require(host.get_competition_state().ammunition_notices == frozen,"Rendering aged notifications"): return
    key(KEY_ESCAPE)
    host.set_process(true)
    for frame in range(5): await process_frame
    host.set_process(false)
    frozen = host.get_competition_state().ammunition_notices
    if not require(host.competition_test_step(.7,false) and host.get_competition_state().ammunition_notices == frozen,"Pause aged notifications"): return
    key(KEY_ESCAPE)
    host.set_process(true)
    for frame in range(5): await process_frame
    host.set_process(false)
    if not require(host.competition_test_step(2.1,false),"Notification expiry step failed"): return
    for notice in host.get_competition_state().ammunition_notices:
        if not require(notice.remaining == 0,"Notification did not expire"): return
    await capture("ammunition_expired")
    if not require(host.drive_test_step(1.0/60,0,0,0,true),"Notification reset failed"): return
    for notice in host.get_competition_state().ammunition_notices:
        if not require(notice.remaining == 0 and notice.text == "","Restart retained an old notification"): return
    root.remove_child(host)
    host.free()
    # Let the audio mixer drop its just-stopped combat playback references.
    await create_timer(.25).timeout
    if not failed:
        print("ROADWARS_AMMUNITION_SMOKE_OK")
        quit(0)
