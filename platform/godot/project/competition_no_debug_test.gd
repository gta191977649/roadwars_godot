extends SceneTree

func _initialize() -> void:
    call_deferred("run_test")

func require(value: bool, message: String) -> bool:
    if not value:
        push_error(message)
        quit(1)
    return value

func run_test() -> void:
    var host = ClassDB.instantiate("GameHost")
    root.add_child(host)
    host.set_process(false)
    if not require(host.get_debug_live_rids() == 0 and
                   host.get_competition_state().get("debug_flags", []).is_empty(),
                   "Competition created debug windows without --debug"):
        return
    var destination = ""
    for argument in OS.get_cmdline_user_args():
        if argument.begins_with("--capture-dir="):
            destination = argument.trim_prefix("--capture-dir=")
    for step in range(36):
        if not require(host.competition_test_step(0.25, false), "Competition intro failed"):
            return
        await process_frame
        if step == 0 and not require(not host.is_race_music_playing(),
                                     "Race music started before the original Go transition"):
            return
        if DisplayServer.get_name() != "headless":
            await create_timer(0.25).timeout
            if destination != "" and step in [0, 8, 16, 20, 24, 28, 32]:
                await RenderingServer.frame_post_draw
                if not require(root.get_texture().get_image().save_png(
                    destination.path_join("start_clean_" + str(step) + ".png")) == OK,
                    "Failed to capture clean race start"):
                    return
    if not require(host.get_competition_state().get("countdown_cues", 0) == 4,
                   "Original countdown cues were not each triggered once"):
        return
    if not require(host.is_race_music_playing(), "Race music did not start after Go"):
        return
    # Let AudioServer consume queued playback starts before stopping the voices.
    await create_timer(0.5).timeout
    root.remove_child(host)
    if not require(host.get_live_rids() == 0, "Competition retained renderer resources"):
        return
    host.free()
    await create_timer(0.5).timeout
    print("ROADWARS_COMPETITION_NO_DEBUG_OK")
    quit(0)
