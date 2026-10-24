extends SceneTree

var host: Object
var failed := false
func _initialize() -> void:
    call_deferred("run_test")

func check(value: bool, message: String) -> bool:
    if not value:
        push_error(message)
        failed = true
    return value

func run_test() -> void:
    if not ClassDB.class_exists("GameHost"):
        push_error("GameHost extension is unavailable")
        quit(1)
        return
    host = ClassDB.instantiate("GameHost")
    root.add_child(host)
    host.set_process(false)
    check(host.test_graphics_quality(), "Graphics quality texture/shader/RID contract failed")
    var original: Dictionary = host.get_graphics_state()
    check(original.has("active_backend"), "Missing actual backend")
    check(host.graphics_test_apply({"aa": 1, "render_scale": 0.75, "fps_limit": 120, "vsync": false}, false), "Live quality apply failed")
    check(host.get_graphics_state().fps_limit == 120 and Engine.max_fps == 120, "FPS limit did not reach engine")
    check(not host.graphics_test_apply({"render_scale": 0.1}, false), "Invalid scale accepted")
    check(host.graphics_test_apply({"window_mode": 1}, false), "Borderless failed")
    await process_frame
    check(DisplayServer.window_get_mode() == DisplayServer.WINDOW_MODE_FULLSCREEN, "Borderless fullscreen mode absent")
    check(host.graphics_test_apply({"window_mode": 2}, false), "Fullscreen failed")
    await process_frame
    check(DisplayServer.window_get_mode() in [DisplayServer.WINDOW_MODE_FULLSCREEN, DisplayServer.WINDOW_MODE_EXCLUSIVE_FULLSCREEN], "Fullscreen mode absent")
    check(host.graphics_test_apply(original, false), "Restore live settings failed")
    for frame in range(3):
        await process_frame
    check(DisplayServer.window_get_mode() == DisplayServer.WINDOW_MODE_WINDOWED, "Window restore did not reach the actual display")
    check(host.graphics_test_apply({"fps_limit": 90}, true), "Atomic save failed")
    var saved := ConfigFile.new()
    check(saved.load("user://graphics.cfg") == OK and saved.get_value("graphics", "fps_limit") == 90, "Saved settings invalid")
    var target_backend: int = 4 if 4 in original.backends else 1
    if target_backend in original.backends:
        check(host.graphics_test_apply({"backend": target_backend}, true), "Backend save/probe failed")
        check(host.get_graphics_state().active_backend == original.active_backend, "Backend changed before restart")
    # Parent harness restores user files even if this process aborts.
    check(host.graphics_test_apply(original, false), "Final restore failed")
    root.remove_child(host)
    check(host.get_live_rids() == 0, "Graphics shutdown leaked renderer RIDs")
    host.free()
    if not failed:
        print("ROADWARS_GRAPHICS_OK")
    quit(1 if failed else 0)
