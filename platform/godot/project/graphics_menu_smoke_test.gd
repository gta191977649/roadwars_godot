extends SceneTree
var host: Object
func _initialize() -> void:
    call_deferred("run_test")
func check(value: bool, message: String) -> bool:
    if not value:
        push_error(message)
        quit(1)
    return value
func capture(name: String) -> void:
    for frame in range(3):
        await process_frame
        await RenderingServer.frame_post_draw
    var image := root.get_texture().get_image()
    image.save_png("res://../../../build/graphics_" + name + ".png")
func press(id: String, direction := "accept") -> bool:
    return host.frontend_test_step(0, "") and host.frontend_test_select_option(id) and host.frontend_test_step(0, direction)
func run_test() -> void:
    if not ClassDB.class_exists("GameHost"):
        push_error("GameHost extension is unavailable")
        quit(1)
        return
    host = ClassDB.instantiate("GameHost")
    root.add_child(host)
    host.set_process(false)
    if not check(host.preview_frontend_screen("graphics", 1, ""), "Graphics page failed"):
        return
    root.size = Vector2i(1100, 700)
    await process_frame
    if not check(host.frontend_test_step(0, "") and host.get_graphics_state().width == 1100 and
                 host.get_graphics_state().height == 700, "Window resize did not update resolution"):
        return
    await capture("display")
    if not check(press("window_mode", "right") and press("apply"), "Display preview failed"):
        return
    if not check(host.frontend_test_select_option("confirm"), "Display confirmation modal absent"):
        return
    if not check(host.frontend_test_step(16, "") and not host.frontend_test_select_option("confirm"), "Display confirmation did not expire"):
        return
    if not check(press("page", "right") and host.frontend_test_select_option("chrome_mapping"), "Quality tab or chrome ID missing"):
        return
    await capture("quality")
    if not check(press("chrome_mapping", "right") and host.frontend_test_step(0, "") and
                 host.frontend_test_step(0, "back"), "ESC save question failed"):
        return
    await capture("save_question")
    if not check(host.frontend_test_step(0, "") and host.frontend_test_step(0, "back") and
                 host.frontend_test_select_option("chrome_mapping"), "ESC did not cancel save question"):
        return
    if not check(DisplayServer.window_get_mode() == DisplayServer.WINDOW_MODE_WINDOWED, "Window mode rollback did not reach display server"):
        return
    if not check(host.frontend_test_step(0, "") and host.frontend_test_step(0, "back") and
                 host.frontend_test_step(0, "") and host.frontend_test_step(0, "left") and
                 host.frontend_test_step(0, "") and host.frontend_test_step(0, "accept") and
                 host.get_frontend_screen() == "main_menu", "YES did not save and leave Graphics"):
        return
    root.remove_child(host)
    if not check(host.get_live_rids() == 0, "Settings menu leaked RIDs"):
        return
    host.free()
    print("ROADWARS_GRAPHICS_MENU_OK")
    quit()
