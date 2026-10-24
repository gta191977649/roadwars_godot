extends SceneTree

func _initialize() -> void:
    call_deferred("run_test")

func require(value: bool, message: String) -> bool:
    if not value:
        push_error(message)
        quit(1)
    return value

func capture(name: String) -> void:
    if DisplayServer.get_name() == "headless":
        return
    await RenderingServer.frame_post_draw
    var image = root.get_texture().get_image()
    var destination = ""
    for argument in OS.get_cmdline_user_args():
        if argument.begins_with("--capture-dir="):
            destination = argument.trim_prefix("--capture-dir=")
    if destination != "":
        require(image.save_png(destination.path_join(name + ".png")) == OK, "Cannot save frontend capture")

func run_test() -> void:
    var host = ClassDB.instantiate("GameHost")
    root.add_child(host)
    host.set_process(false)
    if not require(host.get_frontend_screen() == "publisher_1", "Default launch did not enter publisher intro"):
        return
    var initial_rids = host.get_live_rids()
    for page in range(6):
        if host.get_frontend_screen() == "main_menu":
            break
        if not require(host.frontend_test_step(0, "") and host.frontend_test_step(0.31, "accept"),
                       "Startup input failed"):
            return
    if not require(host.get_frontend_screen() == "main_menu", "Startup did not reach main menu"):
        return
    await capture("default_main_menu")
    var screens = ["publisher_1", "publisher_2", "intro", "title", "main_menu", "select_option", "select_controller", "select_character", "select_track", "season_type", "select_league", "host_or_join", "select_connection", "select_session", "create_season", "load_season", "graphics", "sound", "game", "keyboard", "joystick", "start_race_summary", "race_results", "standings", "season_results", "loading", "dialog", "pause", "info", "modify_vehicle", "victory", "defeat"]
    for screen in screens:
        if not require(host.preview_frontend_screen(screen, 1.0 if screen != "race_results" else 4.0, ""), "Screen failed: " + screen):
            return
        await capture(screen)
        print("FRONTEND_SCREEN_OK ", screen)
    var variants = {
        "modify_vehicle": ["tires", "drive_train", "weapons", "armor", "repair", "paint"],
        "host_or_join": ["IPX", "TCP-IP", "SERIAL", "MODEM"],
        "pause": ["Single Race", "Single Race No Replay", "Single Race Over", "Single Race Over No Replay", "Season Race", "Season Race No Replay", "Season Race Over", "Season Race Over No Replay", "Multiplayer", "Multiplayer Over", "Multiplayer Over Join"],
        "race_results": ["dnf"], "season_results": ["join"],
        "load_season": ["empty"], "select_session": ["empty"],
        "loading": ["empty", "complete"],
        "dialog": ["delete_season", "downgrade_item"]
    }
    for screen in variants:
        for variant in variants[screen]:
            if not require(host.preview_frontend_screen(screen, 2.0, variant), "Variant failed: " + screen + " / " + variant):
                return
            await capture(screen + "_" + variant.replace(" ", "_"))
            print("FRONTEND_VARIANT_OK ", screen, " / ", variant)
    require(host.preview_frontend_screen("start_race_summary", 1.0, ""), "Summary navigation setup failed")
    require(host.frontend_test_step(0.0, "accept"), "Garage navigation input failed")
    require(host.frontend_test_step(0.4, ""), "Garage navigation failed")
    if not require(host.get_frontend_screen() == "modify_vehicle", "Summary did not enter garage"):
        return
    require(host.frontend_test_step(0.0, "accept"), "Store navigation input failed")
    require(host.frontend_test_step(0.4, ""), "Store navigation failed")
    require(host.frontend_test_step(0.0, "right"), "Store product selection failed")
    require(host.frontend_test_step(0.0, ""), "Store product release failed")
    require(host.frontend_test_step(0.0, "back"), "Store return input failed")
    require(host.frontend_test_step(0.4, ""), "Store return failed")
    require(host.preview_frontend_screen("main_menu", 1.0, ""), "Main menu preview failed")
    require(host.frontend_test_step(0.0, "right"), "Selection input failed")
    for value in [0.03, 0.04, 0.06]:
        require(host.frontend_test_step(value, ""), "Selection animation failed")
        await capture("main_menu_animation_" + str(value))
    root.size = Vector2i(960, 540)
    host.frontend_test_step(0.0, "")
    await capture("main_menu_wide")
    root.size = Vector2i(640, 480)
    for cycle in range(3):
        host.restart_smoke_scene()
        host.set_process(false)
        # Let AudioStreamPlayer consume its queued start before the next restart.
        await process_frame
        if not require(host.get_live_rids() == initial_rids, "Frontend RID count changed after restart"):
            return
    host.queue_free()
    await process_frame
    # AudioServer releases stopped playback references on its next mix callback.
    await create_timer(0.1).timeout
    print("ROADWARS_MENU_SMOKE_OK")
    quit(0)
