extends SceneTree

func _initialize() -> void:
    call_deferred("run_test")

func require(value: bool, message: String) -> bool:
    if not value:
        push_error(message)
        quit(1)
    return value

func garage(host: Object, enabled: bool) -> bool:
    if not require(host.preview_frontend_screen("modify_vehicle", 2.0, ""),
            "Chrome garage preview failed"):
        return false
    var state: Dictionary = host.get_presentation_state()
    return require(state.get("chrome_mapping_enabled", not enabled) == enabled and
            (int(state.get("chrome_materials", -1)) > 0 if enabled else
            int(state.get("chrome_materials", -1)) == 0),
            "Garage chrome state/material count mismatch: %s" % state)

func toggle_chrome(host: Object) -> bool:
    if not require(host.preview_frontend_screen("graphics", 1.0, ""),
            "Cannot open graphics settings for chrome toggle"):
        return false
    return require(host.frontend_test_select_option("page") and
            host.frontend_test_step(0.0, "right") and
            host.frontend_test_step(0.0, "") and
            host.frontend_test_select_option("chrome_mapping") and
            host.frontend_test_step(0.0, "right") and
            host.frontend_test_step(0.0, "") and
            host.frontend_test_select_option("apply") and
            host.frontend_test_step(0.0, "accept") and
            host.frontend_test_step(0.0, ""), "Chrome toggle/apply input failed")

func capture(name: String) -> bool:
    await process_frame
    await RenderingServer.frame_post_draw
    var image: Image = root.get_texture().get_image()
    if not require(image != null and not image.is_empty(), "Chrome garage capture is empty"):
        return false
    var destination: String = "res://../../../build/" + name + ".png"
    return require(image.save_png(destination) == OK,
            "Cannot save chrome garage capture: " + destination)

func run_test() -> void:
    var host = ClassDB.instantiate("GameHost")
    root.add_child(host)
    host.set_process(false)
    if not garage(host, true) or not await capture("chrome_garage_on"):
        return
    host.set_frontend_visible(false)
    if not await capture("chrome_garage_clean"):
        return
    host.set_frontend_visible(true)
    if not toggle_chrome(host) or not garage(host, false):
        return
    if not await capture("chrome_garage_off"):
        return
    host.restart_smoke_scene()
    host.set_process(false)
    await process_frame
    if not garage(host, false):
        return
    if not toggle_chrome(host) or not garage(host, true):
        return
    # Graphics navigation warms menu-specific textures; compare like-for-like fresh scenes.
    var initial_rids: int = -1
    for cycle in range(3):
        host.restart_smoke_scene()
        host.set_process(false)
        await process_frame
        if not garage(host, true):
            return
        if initial_rids < 0:
            initial_rids = host.get_live_rids()
        if not require(host.get_live_rids() == initial_rids,
                "Chrome garage restart changed RID ownership in cycle %d" % cycle):
            return
    root.remove_child(host)
    if not require(host.get_live_rids() == 0,
            "Chrome garage retained RIDs after scene exit"):
        host.free()
        return
    host.free()
    print("ROADWARS_CHROME_ASSETS_OK")
    quit(0)
