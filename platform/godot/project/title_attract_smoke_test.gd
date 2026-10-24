extends SceneTree

var host
var failed := false

func _initialize() -> void:
    call_deferred("run_test")

func require(value: bool, message: String) -> bool:
    if not value:
        push_error(message)
        failed = true
    return value

func key(code: Key, pressed: bool) -> void:
    var event := InputEventKey.new()
    event.keycode = code
    event.physical_keycode = code
    event.pressed = pressed
    root.push_input(event)

func capture(name: String, black: bool = false) -> void:
    if DisplayServer.get_name() == "headless":
        return
    await RenderingServer.frame_post_draw
    var image := root.get_texture().get_image()
    if black:
        var maximum := 0.0
        for y in range(0, image.get_height(), 20):
            for x in range(0, image.get_width(), 20):
                var pixel := image.get_pixel(x, y)
                maximum = maxf(maximum, maxf(pixel.r, maxf(pixel.g, pixel.b)))
        require(maximum < 0.01, "Handoff did not render full black: " + name + " maximum=" + str(maximum))
    for argument in OS.get_cmdline_user_args():
        if argument.begins_with("--capture-dir="):
            require(image.save_png(argument.trim_prefix("--capture-dir=").path_join(name + ".png")) == OK,
                    "Cannot capture attract transition")

func load_demo() -> void:
    require(host.get_frontend_screen() == "title", "Demo must start from Title")
    require(host.frontend_test_step(6.99 - host.get_competition_state().frontend_elapsed, ""), "Title idle advance failed")
    require(host.get_frontend_screen() == "title", "Title timed out early")
    require(host.frontend_test_step(0.31000001192092896, ""), "Title timeout failed")
    require(host.get_frontend_screen() == "loading", "Title did not request demo loading")
    require(host.get_competition_state().menu_music_playing, "Demo loading changed menu theme")
    await capture("attract_loading_black", true)
    for frame in range(100):
        if host.get_frontend_screen() != "loading":
            break
        require(host.frontend_test_step(0.1, ""), "Demo loading stage failed")
        await process_frame
    var state: Dictionary = host.get_competition_state()
    require(state.attract_demo and state.phase == "countdown", "Demo did not enter simulation")
    require(state.participants == 6 and is_equal_approx(state.countdown_remaining, 2), "Wrong demo setup")
    require(state.hud_quads == 1 and is_equal_approx(state.hud_last_alpha, 1), "Demo entry lacks opaque black")
    require(state.menu_music_playing, "Menu theme stopped during attract")
    await capture("attract_race_black", true)

func return_title() -> void:
    require(host.get_competition_state().attract_black, "Exit must reach full black")
    await capture("attract_exit_black", true)
    await process_frame
    require(host.competition_test_step(0, false), "Black handoff failed")
    require(host.get_frontend_screen() == "title", "Demo did not return to Title")
    require(is_equal_approx(host.get_competition_state().hud_last_alpha, 1), "Title did not start black")
    await capture("attract_title_black", true)

func run_test() -> void:
    root.size = Vector2i(960, 540)
    host = ClassDB.instantiate("GameHost")
    root.add_child(host)
    host.set_process(false)
    for page in range(4):
        if host.get_frontend_screen() == "title":
            break
        require(host.frontend_test_step(0, ""), "Startup release failed")
        require(host.frontend_test_step(0.3, "accept"), "Startup skip failed")
    await load_demo()
    var start: Vector3 = host.get_vehicle_position()
    for tick in range(600):
        require(host.competition_test_step(1.0 / 60, false), "Attract simulation failed")
        if tick % 6 == 0:
            await process_frame
        var geometry: Dictionary = host.get_competition_state()
        require(geometry.vehicle_body_count == 6 and geometry.vehicle_body_alignment_error < 0.00001,
                "Attract body and wheels use different simulation ticks")
    var state: Dictionary = host.get_competition_state()
    require(host.get_vehicle_position().distance_to(start) > 5, "Selected vehicle was not AI-driven")
    require(state.hud_quads == 0, "Demo HUD should be hidden")
    require(host.get_crash_sound_count() == 0 and host.get_tire_sound_start_count() == 0, "Demo SFX were not muted")
    require(not host.is_race_music_playing(), "Demo started race BGM")
    host.notification(Node.NOTIFICATION_APPLICATION_FOCUS_OUT)
    host.notification(Node.NOTIFICATION_APPLICATION_FOCUS_IN)
    require(Input.mouse_mode != Input.MOUSE_MODE_CAPTURED, "Focus restoration captured demo mouse")
    await capture("attract_running")
    # A screenshot key and mouse motion must not dismiss the demo.
    var motion := InputEventMouseMotion.new()
    motion.relative = Vector2(20, 10)
    root.push_input(motion)
    key(KEY_F12, true)
    key(KEY_F12, false)
    require(host.competition_test_step(1.0 / 60, false), "F12 step failed")
    require(not host.get_competition_state().attract_exiting, "F12 dismissed demo")
    key(KEY_ESCAPE, true)
    key(KEY_ESCAPE, false)
    for tick in range(31):
        require(host.competition_test_step(1.0 / 60, false), "Input exit fade failed")
    require(not host.get_competition_state().pause_menu, "Demo input opened pause menu")
    await return_title()
    # Complete the second demo through its45-second automatic deadline.
    await load_demo()
    for tick in range(2700):
        require(host.competition_test_step(1.0 / 60, false), "Automatic demo simulation failed")
        if tick % 6 == 0:
            await process_frame
    require(host.get_competition_state().attract_exiting, "Demo deadline did not fire")
    for tick in range(31):
        require(host.competition_test_step(1.0 / 60, false), "Automatic exit fade failed")
    await return_title()
    # A fresh Title key goes to the main menu, rather than restarting attract.
    require(host.frontend_test_step(0, ""), "Title release failed")
    require(host.frontend_test_step(0.31, "accept"), "Title activation failed")
    require(host.get_frontend_screen() == "main_menu", "Title input no longer opens main menu")
    host.queue_free()
    await process_frame
    await process_frame
    await create_timer(0.2).timeout
    if not failed:
        print("ROADWARS_TITLE_ATTRACT_SMOKE_OK")
    quit(1 if failed else 0)
