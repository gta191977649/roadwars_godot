extends SceneTree
func _initialize() -> void:
    call_deferred("run_test")
func run_test() -> void:
    var cfg := ConfigFile.new()
    if cfg.load("user://graphics.cfg") != OK:
        push_error("Startup fixture missing saved cfg")
        quit(1)
        return
    var key := "rendering_device/driver.windows" if OS.get_name() == "Windows" else ("rendering_device/driver.macos" if OS.get_name() == "macOS" else "rendering_device/driver.linuxbsd")
    if OS.get_name() == "Windows" and cfg.get_value("rendering", "renderer/rendering_method.windows", cfg.get_value("rendering", "renderer/rendering_method", "forward_plus")) == "gl_compatibility":
        key = "gl_compatibility/driver.windows"
    var expected: String = cfg.get_value("rendering", key)
    if RenderingServer.get_current_rendering_driver_name() != expected:
        push_error("Saved backend was not applied before renderer initialization")
        quit(1)
        return
    print("ROADWARS_GRAPHICS_STARTUP_OK:", expected)
    quit()
